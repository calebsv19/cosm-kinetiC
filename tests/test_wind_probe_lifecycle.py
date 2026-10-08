"""Exercise the actual probe CLI with synthetic scenes/workers and retained output."""
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
WORKER = r"""#!/usr/bin/env python3
import json,sys,time,os,fcntl
from pathlib import Path
def arg(name):return sys.argv[sys.argv.index(name)+1]
root=Path(arg('--output-root'))
assert '--overwrite' not in sys.argv
assert not root.exists(), 'native output must start fresh'
scene=json.loads(Path(arg('--runtime-scene')).read_text())
rotation=scene['objects'][0]['transform']['rotation']['z']
clean=Path(__file__).resolve().parents[2]/'tmp/locks/clean.lock'
with clean.open('r') as lock:
 try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
 except BlockingIOError:pass
 else:raise AssertionError('worker did not inherit cleanup exclusion')
print('synthetic worker log retained',flush=True)
if MODE=='fail':sys.exit(17)
if MODE=='sleep':time.sleep(30)
root.mkdir(parents=True)
for name in ('render_frames','wind_projection_frames'):
 p=root/name;p.mkdir();(p/('frame_%06d.bmp'%(int(arg('--frames'))-1))).write_bytes(b'fixture frame')
row={'frame_index':int(arg('--frames'))-1,'available':True,'object_drag_available':True,
'object_projected_area':1+rotation/100,'object_drag_pressure_proxy':2+rotation/100,
'outlet_throughput':3,'object_solid_cells':4,'object_pressure_delta':5,'pressure_delta':6,
'vorticity_avg':7,'vorticity_max':8}
(root/'wind_analysis_timeseries.jsonl').write_text(json.dumps(row)+'\n')
"""

class ProbeLifecycle(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name).resolve() / 'checkout with spaces'
        (self.repo / 'tools').mkdir(parents=True)
        (self.repo / 'scripts/agent_session').mkdir(parents=True)
        (self.repo / 'build/bin').mkdir(parents=True)
        for name in ('build_owner.py','clean_outputs.py','build_outputs.py','check_clean_root.py',
                     'cfd_evidence.py','wind_probe_lifecycle.py','agent_session/sample_retention.py',
                     'agent_session/owned_command.py'):
            shutil.copy2(ROOT/'scripts'/name, self.repo/'scripts'/name)
        shutil.copy2(ROOT/'tools/wind_orientation_probe.py',self.repo/'tools/wind_orientation_probe.py')
        self.scene = self.repo / 'scene.json'
        self.scene.write_text(json.dumps({'scene_id':'synthetic','objects':[{'object_id':'box','object_type':'box',
            'transform':{'rotation':{'x':0,'y':0,'z':0}}}],
            'extensions':{'physics_sim':{'wind_tunnel':{'active':True}}}}))
        self.worker = self.repo / 'build/bin/physics_sim_headless'
        self.worker_mode('pass')
        self.parent = self.repo / 'tmp/probes'
        self.parent.mkdir(parents=True)
        self.old = self.parent / 'previous';self.old.mkdir();(self.old/'frame.bmp').write_bytes(b'original evidence')

    def worker_mode(self, mode):
        self.worker.write_text(WORKER.replace('from pathlib import Path','MODE='+repr(mode)+'\nfrom pathlib import Path'))
        self.worker.chmod(0o700)

    def snapshot(self, root):
        return {str(p.relative_to(root)):(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes())
                for p in root.rglob('*') if p.is_file() and not p.is_symlink()}

    def run_probe(self, *extra, output=None, timeout=20):
        before = self.snapshot(self.old)
        scene_before = self.scene.read_bytes();worker_before=self.worker.read_bytes()
        p=subprocess.run([sys.executable,'-B',str(self.repo/'tools/wind_orientation_probe.py'),
            '--runtime-scene',str(self.scene),'--output-root',str(output or self.parent),
            '--headless-bin',str(self.worker),'--frames','1','--grid','8x8x8',*extra],
            cwd=self.repo,capture_output=True,text=True,timeout=timeout)
        self.assertEqual(self.snapshot(self.old),before)
        self.assertEqual(self.scene.read_bytes(),scene_before)
        self.assertEqual(self.worker.read_bytes(),worker_before)
        self.assertFalse(any(self.repo.rglob('__pycache__')))
        return p

    def attempts(self):return sorted(self.parent.glob('orientation-*'))

    def test_success_and_rerun_preserve_all_previous_bytes(self):
        first=self.run_probe();self.assertEqual(first.returncode,0,first.stdout+first.stderr)
        capsule=self.attempts()[0];before=self.snapshot(capsule)
        second=self.run_probe('--keep-existing');self.assertEqual(second.returncode,0,second.stdout+second.stderr)
        self.assertEqual(len(self.attempts()),2);self.assertEqual(self.snapshot(capsule),before)
        receipt=json.loads((capsule/'receipt.json').read_text())
        self.assertEqual(receipt['status'],'passed');self.assertEqual(len(receipt['commands']),3)
        self.assertFalse(receipt['all_external_descendants_verified_terminal'])
        self.assertTrue(all(row['direct_child_reaped'] for row in receipt['commands']))
        self.assertTrue((capsule/'source_runtime.json').exists());self.assertTrue((capsule/'worker').exists())
        report=json.loads((capsule/'work/orientation_probe_summary.json').read_text())
        self.assertEqual([row['rotation_deg']['z'] for row in report['results']],[0,45,90])
        self.assertEqual(report['results'][0]['object_drag_pressure_proxy'],2)

    def test_nonzero_worker_failure_retains_source_logs_and_receipt(self):
        self.worker_mode('fail');p=self.run_probe();self.assertNotEqual(p.returncode,0)
        capsule=self.attempts()[0];receipt=json.loads((capsule/'receipt.json').read_text())
        self.assertEqual(receipt['status'],'failed');self.assertIn('17',receipt['failure'])
        self.assertIn('synthetic worker',(capsule/'logs/baseline.stdout').read_text())
        self.assertEqual((capsule/'source_runtime.json').read_bytes(),self.scene.read_bytes())
        self.assertFalse((capsule/'work/orientation_probe_summary.json').exists())

    def test_timeout_keeps_attempt_and_failure_diagnostics(self):
        self.worker_mode('sleep');p=self.run_probe('--wall-cap','0.2')
        self.assertNotEqual(p.returncode,0);capsule=self.attempts()[0]
        receipt=json.loads((capsule/'receipt.json').read_text())
        self.assertIn(receipt['status'],('failed','held'));self.assertIn('wall cap',receipt['failure'])
        self.assertTrue((capsule/'logs/baseline.outcome.json').exists())

    def test_listing_is_read_only_even_with_protected_output_and_missing_worker(self):
        self.worker.unlink();before=self.snapshot(self.repo)
        p=self.run_listing(self.repo/'src')
        self.assertEqual(p.returncode,0,p.stderr);self.assertIn('id=box',p.stdout)
        self.assertEqual(self.snapshot(self.repo),before);self.assertFalse((self.repo/'tmp/locks').exists())

    def run_listing(self, output):
        return subprocess.run([sys.executable,'-B',str(self.repo/'tools/wind_orientation_probe.py'),
            '--runtime-scene',str(self.scene),'--output-root',str(output),'--list-objects'],
            cwd=self.repo,capture_output=True,text=True,timeout=5)

    def test_protected_and_external_output_refuse_before_attempt(self):
        for path in (self.repo/'src',self.repo/'build/probe',self.repo/'data/tools/probe',self.repo.parent/'outside'):
            with self.subTest(path=path):
                p=self.run_probe(output=path);self.assertNotEqual(p.returncode,0)
                self.assertFalse(path.exists());self.assertEqual(self.attempts(),[])

    def test_linked_output_parent_refuses(self):
        linked=self.repo/'tmp/linked';linked.symlink_to(self.parent,target_is_directory=True)
        p=self.run_probe(output=linked);self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_linked_scene_refuses_without_attempt(self):
        original=self.scene.with_name('original.json');self.scene.rename(original);self.scene.symlink_to(original)
        p=self.run_probe();self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_linked_worker_refuses_without_attempt(self):
        other=self.worker.with_name('original');self.worker.rename(other);self.worker.symlink_to(other)
        p=self.run_probe();self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_external_or_nonexecutable_worker_refuses(self):
        external=self.repo/'worker';shutil.copy2(self.worker,external)
        for args in (('--headless-bin',str(external)),()):
            if not args:self.worker.chmod(0o600)
            p=self.run_probe(*args);self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_colliding_normalized_names_refuse_without_attempt(self):
        p=self.run_probe('--orientation','a b:0,0,0','--orientation','a_b:0,0,90')
        self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_nonfinite_or_excessive_orientations_refuse(self):
        for extra in (('--orientation','a:nan,0,0'),('--orientation','a:1e999,0,0'),
                      tuple(v for i in range(33) for v in ('--orientation',str(i)+':0,0,0'))):
            p=self.run_probe(*extra);self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_resource_bounds_refuse_before_allocation(self):
        for extra in (('--frames','10001'),('--sim-steps-per-frame','100001'),('--wall-cap','nan'),('--wall-cap','0'),('--log-cap','0')):
            p=self.run_probe(*extra);self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_invalid_or_large_scene_refuses_without_attempt(self):
        for data in ('{"objects":NaN}', '{"objects":[],"objects":[]}', 'x'*2097153):
            self.scene.write_text(data);p=self.run_probe();self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_ambiguous_or_missing_target_refuses_without_attempt(self):
        data=json.loads(self.scene.read_text());data['objects']*=2;self.scene.write_text(json.dumps(data))
        for target in ('box','missing'):
            p=self.run_probe('--object-id',target);self.assertNotEqual(p.returncode,0);self.assertEqual(self.attempts(),[])

    def test_output_file_refuses_and_remains_unchanged(self):
        target=self.repo/'tmp/file';target.write_text('original')
        p=self.run_probe(output=target);self.assertNotEqual(p.returncode,0)
        self.assertEqual(target.read_text(),'original');self.assertEqual(self.attempts(),[])

    def test_log_bound_failure_retains_attempt(self):
        p=self.run_probe('--log-cap','1');self.assertNotEqual(p.returncode,0)
        receipt=json.loads((self.attempts()[0]/'receipt.json').read_text())
        self.assertIn('log cap',receipt['failure'])

    def test_relative_rotation_retains_original_source(self):
        data=json.loads(self.scene.read_text());data['objects'][0]['transform']['rotation']['z']=10
        self.scene.write_text(json.dumps(data))
        p=self.run_probe('--rotation-mode','relative','--orientation','single:0,0,20')
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        report=json.loads((self.attempts()[0]/'work/orientation_probe_summary.json').read_text())
        self.assertEqual(report['results'][0]['rotation_deg']['z'],30)

    def test_competing_build_owner_holds_before_attempt(self):
        # Use the same kernel ownership helper as real builds, not a PID file.
        sys.path.insert(0,str(ROOT/'scripts'))
        from build_owner import acquire
        descriptors=acquire(self.repo,self.repo/'build')
        try:
            p=self.run_probe();self.assertNotEqual(p.returncode,0)
            self.assertIn('active cleanup or another build',p.stderr)
            self.assertEqual(self.attempts(),[])
        finally:
            for fd in descriptors:os.close(fd)

    def test_control_and_sealed_evidence_namespaces_refuse(self):
        sealed=self.repo/'data/experiments/sealed';sealed.mkdir(parents=True)
        (sealed/'bundle_manifest.json').write_text('{}')
        for path in (self.repo/'tmp/locks/output',self.repo/'tmp/fixture-sessions/output',sealed/'nested'):
            p=self.run_probe(output=path);self.assertNotEqual(p.returncode,0)
            self.assertFalse(path.exists());self.assertEqual(self.attempts(),[])

    def test_source_drift_keeps_original_and_never_prints_success(self):
        original=self.scene.read_bytes()
        self.worker.write_text(self.worker.read_text()+"\nPath("+repr(str(self.scene))+").write_text('{}')\n")
        p=subprocess.run([sys.executable,'-B',str(self.repo/'tools/wind_orientation_probe.py'),
            '--runtime-scene',str(self.scene),'--output-root',str(self.parent),
            '--headless-bin',str(self.worker),'--frames','1','--orientation','single:0,0,0'],
            cwd=self.repo,capture_output=True,text=True,timeout=10)
        self.assertNotEqual(p.returncode,0);self.assertNotIn('probe passed:',p.stdout)
        capsule=self.attempts()[0]
        self.assertEqual((capsule/'source_runtime.json').read_bytes(),original)
        self.assertIn('source changed',json.loads((capsule/'receipt.json').read_text())['failure'])

    def test_nonfinite_and_excessive_timeseries_are_failed_attempts(self):
        for mode,tail in (('nan',"row['object_projected_area']=float('nan');(root/'wind_analysis_timeseries.jsonl').write_text(json.dumps(row)+'\\n')"),
                          ('rows',"(root/'wind_analysis_timeseries.jsonl').write_text((json.dumps(row)+'\\n')*10001)")):
            self.worker_mode(mode);self.worker.write_text(self.worker.read_text()+'\n'+tail+'\n')
            p=self.run_probe('--orientation','single:0,0,0');self.assertNotEqual(p.returncode,0)
            capsule=max(self.attempts(),key=lambda p:p.stat().st_mtime_ns)
            self.assertEqual(json.loads((capsule/'receipt.json').read_text())['status'],'failed')
            self.assertFalse((capsule/'work/orientation_probe_summary.json').exists())

if __name__ == '__main__':unittest.main()
