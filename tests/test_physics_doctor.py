"""Read-only doctor reports actionable scope without inventing acceptance."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from physics_doctor import doctor, probe, SOURCE_BASELINE
from build_outputs import record


class Doctor(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();(self.repo/'scripts').mkdir()
        for name in ('requirements-cfd-reference.txt','requirements-cfd-reference-amg.txt'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        for name in SOURCE_BASELINE:
            path=self.repo/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'controlled source baseline')
        self.roots={k:Path(v) for k,v in {'build':'build/fresh','test':'tmp/tests','experiments':'data/experiments','tools':'data/tools'}.items()}

    def fake_probe(self,command,env=None):
        if command[-2:]==['get','homebrew_prefix']:
            return {'status':'passed','exit_code':0,'stdout':'/opt/homebrew','stderr':''}
        if '-c' in command:
            row={name:{'version':version,'runtime_version':version,'module_path':'controlled module','imported':True} for name,version in
                 (('scikit-fem','12.0.2'),('numpy','2.5.3'),('scipy','1.18.1'),('pyamg','5.3.0'))}
            return {'status':'passed','exit_code':0,'stdout':json.dumps(row),'stderr':''}
        return {'status':'passed','exit_code':0,'stdout':'test version','stderr':''}

    def run_doctor(self, **kwargs):
        with patch('physics_doctor.selected_command',return_value=['/controlled-tool']):
            return doctor(self.repo,self.roots,run_probe=kwargs.pop('run_probe',self.fake_probe),**kwargs)

    def interpreter(self):
        path=self.repo/'data/tools/cfd-reference-venv/bin/python';path.parent.mkdir(parents=True);path.write_bytes(b'controlled interpreter')
        return path

    def test_missing_reference_is_optional_for_headless_and_roots_are_not_created(self):
        before=set(self.repo.rglob('*'));row=self.run_doctor()
        self.assertEqual(row['status'],'ready_for_build_attempt')
        self.assertEqual(row['reference_environment']['status'],'missing')
        self.assertEqual(row['mutations_performed'],[])
        for field in ('build_verified','test_verified','installed_or_published_state_verified','physical_qualification_verified'):
            self.assertFalse(row[field])
        self.assertEqual(set(self.repo.rglob('*')),before)

    def test_reference_profile_requires_matching_pins_and_successful_imports(self):
        self.assertEqual(self.run_doctor(profile='cfd-reference')['status'],'attention_required')
        self.interpreter();row=self.run_doctor(profile='cfd-reference')
        self.assertEqual(row['status'],'ready_for_build_attempt')
        def wrong(command,env=None):
            result=self.fake_probe(command,env)
            if '-c' in command:
                data=json.loads(result['stdout']);data['numpy']['version']='old';result['stdout']=json.dumps(data)
            return result
        row=self.run_doctor(profile='cfd-reference',run_probe=wrong)
        self.assertEqual(row['status'],'attention_required');self.assertEqual(row['reference_environment']['status'],'mismatch')

    def test_incomplete_portable_checkout_is_not_ready(self):
        (self.repo/SOURCE_BASELINE[-1]).unlink()
        row=self.run_doctor()
        self.assertEqual(row['status'],'attention_required')
        self.assertEqual(row['checks']['source_checkout']['status'],'unverified')
        self.assertFalse(row['checks']['source_checkout']['full_build_graph_verified'])

    def test_runtime_package_version_cannot_hide_behind_matching_distribution_metadata(self):
        self.interpreter()
        def wrong(command,env=None):
            row=self.fake_probe(command,env)
            if '-c' in command:
                data=json.loads(row['stdout']);data['scipy']['runtime_version']='different';row['stdout']=json.dumps(data)
            return row
        row=self.run_doctor(profile='cfd-reference',run_probe=wrong)
        self.assertEqual(row['reference_environment']['status'],'mismatch')
        self.assertEqual(row['status'],'attention_required')

    def test_invalid_roots_refuse_before_tool_execution(self):
        self.roots['experiments']=Path('src/runs')
        def forbidden(*args,**kwargs):raise AssertionError('must not execute a probe')
        row=self.run_doctor(run_probe=forbidden)
        self.assertEqual(row['checks']['layout']['status'],'held')
        self.assertEqual(row['status'],'attention_required')
        self.assertFalse((self.repo/'src/runs').exists())

    def test_missing_tools_and_dependency_probe_failure_are_actionable(self):
        with patch('physics_doctor.selected_command',side_effect=ValueError('unavailable')):
            row=doctor(self.repo,self.roots,run_probe=self.fake_probe)
        self.assertEqual(row['status'],'attention_required')
        self.assertEqual(row['checks']['compiler']['status'],'missing')
        def failure(command,env=None):
            if command[-1]=='SDL2_ttf':return {'status':'failed','exit_code':1,'stdout':'','stderr':'missing'}
            return self.fake_probe(command,env)
        row=self.run_doctor(run_probe=failure)
        self.assertEqual(row['checks']['dependencies']['status'],'unverified')
        self.assertEqual(row['status'],'attention_required')

    def test_registered_executable_bytes_are_separate_from_profile_identity(self):
        path=self.repo/'build/fresh/bin/physics_sim_headless';path.parent.mkdir(parents=True);path.write_bytes(b'compiled')
        record(self.repo,path,'compiler')
        row=self.run_doctor()
        evidence=row['output_ownership']['physics_sim_headless']
        self.assertEqual(evidence['status'],'verified_disposable_bytes')
        self.assertFalse(evidence['profile_identity_verified'])
        path.write_bytes(b'changed')
        row=self.run_doctor();self.assertEqual(row['output_ownership']['physics_sim_headless']['status'],'unverified')
        self.assertEqual(row['status'],'attention_required')

    def test_fifo_requirement_and_malformed_runtime_json_are_diagnostics(self):
        pin=self.repo/'scripts/requirements-cfd-reference.txt';pin.unlink();os.mkfifo(pin)
        row=self.run_doctor(profile='cfd-reference');self.assertEqual(row['reference_environment']['status'],'unverified')
        pin.unlink();shutil.copy2(ROOT/'scripts/requirements-cfd-reference.txt',pin)
        self.interpreter()
        def broken(command,env=None):
            result=self.fake_probe(command,env)
            if '-c' in command:result['stdout']='{"numpy":NaN}'
            return result
        self.assertEqual(self.run_doctor(profile='cfd-reference',run_probe=broken)['reference_environment']['status'],'unverified')

    def test_invalid_recorded_metadata_is_not_hidden_by_prerequisite_availability(self):
        active=self.repo/'build/fresh/.configuration/active.json';active.parent.mkdir(parents=True)
        active.write_text('{"digest":"invalid","generation":1}')
        record(self.repo,active,'configuration')
        row=self.run_doctor()
        self.assertEqual(row['local_status']['cleanup']['status'],'plan_available')
        self.assertEqual(row['checks']['local_metadata']['status'],'unverified')
        self.assertEqual(row['status'],'attention_required')

    def test_json_019_requires_the_declared_018_archive(self):
        from types import SimpleNamespace
        def json019(command,env=None):
            row=self.fake_probe(command,env)
            if command[-1]=='json-c':row['stdout']='0.19'
            return row
        compat=self.repo/'json-compat'
        with patch('physics_doctor.os.uname',return_value=SimpleNamespace(sysname='Darwin')):
            row=self.run_doctor(run_probe=json019,json_compat_prefix=str(compat))
            self.assertEqual(row['checks']['dependencies']['packages']['json-c']['status'],'missing_compatible_archive')
            self.assertEqual(row['status'],'attention_required')
            archive=compat/'lib/libjson-c.a';archive.parent.mkdir(parents=True);archive.write_bytes(b'controlled archive presence')
            row=self.run_doctor(run_probe=json019,json_compat_prefix=str(compat))
            self.assertEqual(row['status'],'ready_for_build_attempt')
            self.assertFalse(row['checks']['dependencies']['abi_or_link_qualification_verified'])

    def test_probe_wall_time_and_output_limits_terminate_and_reap(self):
        row=probe([sys.executable,'-c','import time;time.sleep(30)'],timeout=.2)
        self.assertEqual(row['status'],'unverified');self.assertIn('wall-time',row['reason'])
        row=probe([sys.executable,'-c','print("x"*100000)'],limit=1024)
        self.assertEqual(row['status'],'unverified');self.assertIn('output limit',row['reason'])

    def test_probe_reaps_group_when_parent_exits_but_child_keeps_pipe_open(self):
        marker=self.repo/'unexpected-after-timeout'
        child="import time,pathlib;time.sleep(.6);pathlib.Path("+repr(str(marker))+").write_text('orphan')"
        parent="import subprocess,sys;subprocess.Popen([sys.executable,'-c',"+repr(child)+"])"
        row=probe([sys.executable,'-c',parent],timeout=.2)
        self.assertEqual(row['status'],'unverified');self.assertIn('wall-time',row['reason'])
        time.sleep(.7)
        self.assertFalse(marker.exists())

    def test_actual_control_only_make_doctor_works_without_build_fragments(self):
        for folder in ('make',): (self.repo/folder).mkdir(exist_ok=True)
        for name in ('config.mk','sources-tools.mk','rules-runtime.mk','desktop_release_target_contract.sh'):
            shutil.copy2(ROOT/'make'/name,self.repo/'make'/name)
        for name in ('tool_probe.py','physics_doctor.py','physics_status.py','clean_outputs.py','build_outputs.py',
                     'check_clean_root.py','build_owner.py','cfd_evidence.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        shutil.copy2(ROOT/'makefile',self.repo/'makefile')
        for name in ('VERSION','WORKER_VERSION'):(self.repo/name).write_text('test\n')
        before={str(p.relative_to(self.repo)):p.read_bytes() for p in self.repo.rglob('*') if p.is_file()}
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
        env['PYTHONDONTWRITEBYTECODE']='1'
        result=subprocess.run(['make','doctor','BUILD_DIR=build/fresh','CLANG=definitely-not-a-compiler','PKG_CONFIG=definitely-not-pkg-config'],
                              cwd=self.repo,env=env,capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,2,result.stdout+result.stderr)
        row=json.loads(result.stdout);self.assertEqual(row['checks']['compiler']['status'],'missing')
        after={str(p.relative_to(self.repo)):p.read_bytes() for p in self.repo.rglob('*') if p.is_file()}
        self.assertEqual(before,after);self.assertFalse((self.repo/'tmp').exists());self.assertFalse((self.repo/'build').exists())


if __name__=='__main__':unittest.main()
