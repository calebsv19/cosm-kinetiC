"""Actual retained report CLI reruns, failure preservation and output admission."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
ENV={k:v for k,v in os.environ.items() if not k.startswith('PHYSICS_SIM_BUILD_')}

class ReportProof(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.repo=Path(self.tmp.name).resolve()
        for name in ('scripts','tests','build/profile/open-atmosphere'):(self.repo/name).mkdir(parents=True)
        for name in ('report_proof.py','build_owner.py','cfd_evidence.py','tool_probe.py','clean_outputs.py','check_clean_root.py','build_outputs.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        self.worker=self.repo/'build/profile/open-atmosphere/worker';self.worker.write_bytes(b'worker')
        self.script=self.repo/'tests/report.py';self.script.write_text('import json,sys\nfrom pathlib import Path\nPath(sys.argv[-1]).write_text(json.dumps({"schema":"physics_sim_open_convergence/v1","tests_passed":3,"metrics":{"finite":1}}))\nprint("done")\n')
    def run_report(self,*extra):
        return subprocess.run([sys.executable,'-B','scripts/report_proof.py','--worker',str(self.worker),'--parent',str(self.repo/'data/experiments/report-proofs'),'--script',str(self.script),*extra],cwd=self.repo,env=ENV,capture_output=True,text=True,timeout=10)
    def packets(self):return sorted((self.repo/'data/experiments/report-proofs').glob('*'))
    def test_reruns_fresh_sealed_inventory_and_no_overwrite(self):
        a=self.run_report();self.assertEqual(a.returncode,0,a.stderr);first=self.packets()[0];snapshot={str(p.relative_to(first)):p.read_bytes() for p in first.rglob('*') if p.is_file()}
        b=self.run_report();self.assertEqual(b.returncode,0,b.stderr);self.assertEqual(len(self.packets()),2)
        self.assertEqual(snapshot,{str(p.relative_to(first)):p.read_bytes() for p in first.rglob('*') if p.is_file()})
        for packet in self.packets():
            row=json.loads((packet/'receipt.json').read_text());self.assertEqual(row['status'],'passed');self.assertTrue((packet/'bundle_manifest.json').is_file())
        sys.path.insert(0,str(ROOT/'scripts'))
        from cfd_evidence import verify_bundle
        clone=self.repo/'relocated';shutil.copytree(first,clone)
        self.assertEqual(verify_bundle(clone)['status'],'verified')
        (clone/'metrics.json').write_bytes(b'changed result')
        with self.assertRaises(ValueError):verify_bundle(clone)

    def test_failed_partial_and_invalid_metrics_are_retained(self):
        for suffix in ('raise SystemExit(3)','Path(sys.argv[-1]).write_text("{}")'):
            original=self.script.read_text();self.script.write_text(original+'\n'+suffix+'\n')
            r=self.run_report();self.assertNotEqual(r.returncode,0)
            rows=[json.loads((p/'receipt.json').read_text()) for p in self.packets()]
            self.assertTrue(all(row['status']=='failed' for row in rows));self.assertTrue(all((p/'bundle_manifest.json').is_file() for p in self.packets()))
            self.script.write_text(original)
    def test_unverified_child_teardown_retains_unsealed_attempt(self):
        with (self.repo/'scripts/tool_probe.py').open('a') as stream:
            stream.write('\ndef capture(*args,**kwargs):return {"status":"unverified","reason":"controlled teardown refusal","stdout":b"partial","stderr":b"diagnostic","terminal_processes_verified":False}\n')
        r=self.run_report('--source',str(self.repo/'scripts/tool_probe.py'));self.assertNotEqual(r.returncode,0)
        packet=self.packets()[0];self.assertFalse((packet/'bundle_manifest.json').exists())
        self.assertFalse(json.loads((packet/'receipt.json').read_text())['terminal_processes_verified'])
        self.assertEqual((packet/'stdout.log').read_bytes(),b'partial')
    def test_protected_retained_parent_holds_before_allocation(self):
        r=self.run_report('--parent',str(self.repo/'build/profile/report-proofs'));self.assertNotEqual(r.returncode,0);self.assertFalse((self.repo/'build/profile/report-proofs').exists())
    def test_direct_convergence_existing_or_build_report_holds_before_worker_execution(self):
        path=Path(self.tmp.name)/'existing.json';path.write_bytes(b'preserved')
        for target in (path,ROOT/'build/report-proof-forbidden.json'):
            r=subprocess.run([sys.executable,'-B',str(ROOT/'tests/test_open_atmosphere_convergence.py'),'--report',str(target)],cwd=ROOT,env=ENV,capture_output=True,text=True,timeout=5)
            self.assertNotEqual(r.returncode,0);self.assertNotIn('Ran 3 tests',r.stderr)
        self.assertEqual(path.read_bytes(),b'preserved');self.assertFalse((ROOT/'build/report-proof-forbidden.json').exists())

if __name__=='__main__':unittest.main()
