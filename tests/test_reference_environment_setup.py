"""Reference setup preserves existing roots and retains every mutation attempt."""
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from reference_environment_setup import setup, execute
from physics_doctor import reference_environment, probe


class Setup(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();(self.repo/'scripts').mkdir()
        for name in ('requirements-cfd-reference.txt','requirements-cfd-reference-amg.txt'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        self.tools=self.repo/'data/tools';self.interpreter=self.tools/'cfd-reference-venv/bin/python'
        self.calls=[]
        self.env_patch=patch.dict(os.environ,{key:str(self.repo/path) for key,path in
            (('PHYSICS_SIM_BUILD_ROOT','build'),('PHYSICS_SIM_TEST_ROOT','tmp/tests'),('PHYSICS_SIM_EXPERIMENT_ROOT','data/experiments'))})
        self.env_patch.start();self.addCleanup(self.env_patch.stop)

    def inspector(self,repo,interpreter,run_probe,amg=True,inspect_setup=True):
        marker=interpreter.parent.parent/'.reference-setup.json'
        if inspect_setup and marker.exists() and json.loads(marker.read_text())['status']!='passed':return {'status':'held'}
        if not interpreter.exists():return {'status':'missing'}
        return {'status':'ready_for_reference_attempt','expected_profile':'amg' if amg else 'base','numerical_qualification_verified':False}

    def runner(self,command,log,descriptors,wall_cap):
        self.calls.append(command);log.write_text('controlled setup command log\n')
        self.assertEqual(len(descriptors),2)
        for fd in descriptors:os.fstat(fd)
        if 'venv' in command:
            self.interpreter.parent.mkdir(parents=True);self.interpreter.write_bytes(b'controlled interpreter')
        return {'command':command,'exit_code':0}

    def run_setup(self,**kwargs):
        return setup(self.repo,self.tools,self.interpreter,run_command=kwargs.pop('run_command',self.runner),
                     inspect=kwargs.pop('inspect',self.inspector),**kwargs)

    def test_plan_creates_no_roots_and_apply_publishes_passed_marker_at_final_prefix(self):
        row=self.run_setup(amg=True);self.assertEqual(row['status'],'creation_planned')
        self.assertFalse(self.tools.exists());self.assertFalse((self.repo/'tmp').exists())
        row=self.run_setup(amg=True,apply=True);self.assertEqual(row['status'],'created')
        state=json.loads((self.interpreter.parent.parent/'.reference-setup.json').read_text())
        self.assertEqual(state['status'],'passed');self.assertFalse(state['existing_environment_modified'])
        self.assertEqual(self.calls[0][-1],str(self.interpreter.parent.parent))
        self.assertIn('--isolated',self.calls[1]);self.assertEqual(self.calls[1].count('-r'),2)
        self.assertEqual(json.loads(Path(row['receipt']).read_text())['status'],'passed')
        self.assertTrue((Path(row['attempt'])/'venv.log').exists());self.assertTrue((Path(row['attempt'])/'pip.log').exists())

    def test_matching_existing_environment_is_reused_without_any_writes(self):
        self.interpreter.parent.mkdir(parents=True);self.interpreter.write_bytes(b'existing')
        before={str(p):p.read_bytes() for p in self.repo.rglob('*') if p.is_file()}
        row=self.run_setup(amg=True,apply=True);self.assertEqual(row['status'],'reused');self.assertEqual(self.calls,[])
        self.assertEqual(before,{str(p):p.read_bytes() for p in self.repo.rglob('*') if p.is_file()})
        self.assertFalse((self.repo/'tmp').exists())

    def test_existing_mismatch_and_unclassified_empty_root_are_held(self):
        self.interpreter.parent.mkdir(parents=True);self.interpreter.write_bytes(b'preserve')
        def mismatch(*args,**kwargs):return {'status':'mismatch'}
        row=self.run_setup(amg=True,apply=True,inspect=mismatch);self.assertEqual(row['status'],'held')
        self.assertEqual(self.interpreter.read_bytes(),b'preserve');self.assertEqual(self.calls,[])
        self.interpreter.unlink()
        self.assertEqual(self.run_setup(apply=True)['status'],'held')
        self.assertFalse((self.repo/'tmp').exists())

    def test_failed_install_retains_prefix_logs_and_refuses_a_retry(self):
        def failure(command,log,descriptors,wall_cap):
            if 'pip' in command:log.write_bytes(b'failed install log');raise ValueError('controlled install failure')
            return self.runner(command,log,descriptors,wall_cap)
        row=self.run_setup(apply=True,run_command=failure);self.assertEqual(row['status'],'failed')
        state=json.loads((self.interpreter.parent.parent/'.reference-setup.json').read_text());self.assertEqual(state['status'],'failed')
        self.assertTrue((Path(row['attempt'])/'pip.log').exists())
        self.assertEqual(self.run_setup(apply=True)['status'],'held')
        self.assertEqual(self.interpreter.read_bytes(),b'controlled interpreter')

    def test_protected_global_interpreter_and_active_cleanup_are_refused(self):
        with self.assertRaises(ValueError):setup(self.repo,self.repo/'src',self.repo/'src/cfd-reference-venv/bin/python',apply=True)
        with self.assertRaisesRegex(ValueError,'never a global Python'):setup(self.repo,self.tools,Path(sys.executable).resolve(),apply=True)
        lock=self.repo/'tmp/locks/clean.lock';lock.parent.mkdir(parents=True)
        with lock.open('w') as stream:
            fcntl.flock(stream,fcntl.LOCK_EX|fcntl.LOCK_NB)
            with self.assertRaisesRegex(ValueError,'active cleanup'):self.run_setup(apply=True)
        self.assertFalse(self.tools.exists());self.assertEqual(self.calls,[])

    def test_requirements_drift_fails_and_retains_attempt(self):
        def drift(command,log,descriptors,wall_cap):
            result=self.runner(command,log,descriptors,wall_cap)
            if 'pip' in command:
                with (self.repo/'scripts/requirements-cfd-reference.txt').open('a') as stream:stream.write('# changed during install\n')
            return result
        row=self.run_setup(apply=True,run_command=drift);self.assertEqual(row['status'],'failed')
        self.assertIn('changed during setup',row['failure'])
        self.assertEqual(json.loads(Path(row['receipt']).read_text())['status'],'failed')

    def test_real_venv_creation_keeps_final_prefix_and_failed_pip_is_retained(self):
        def offline(command,log,descriptors,wall_cap):
            if 'pip' in command:log.write_bytes(b'controlled offline failure');raise ValueError('network install deliberately not run')
            return execute(command,log,descriptors,wall_cap)
        row=self.run_setup(apply=True,run_command=offline);self.assertEqual(row['status'],'failed')
        output=subprocess.check_output([str(self.interpreter),'-I','-B','-c','import sys;print(sys.prefix)'],text=True)
        self.assertEqual(output.strip(),str(self.interpreter.parent.parent))
        self.assertEqual(reference_environment(self.repo,self.interpreter,probe)['status'],'held')
        self.assertTrue((self.interpreter.parent/'activate').exists())

    def test_managed_base_marker_cannot_claim_an_amg_profile_or_a_different_prefix(self):
        self.interpreter.parent.mkdir(parents=True);self.interpreter.write_bytes(b'not executed')
        marker=self.interpreter.parent.parent/'.reference-setup.json'
        marker.write_text(json.dumps({'schema':'physics_sim_reference_setup_state_v1','status':'passed',
            'target':str(self.interpreter.parent.parent),'expected':{'scikit-fem':'12.0.2','numpy':'2.5.3','scipy':'1.18.1'}}))
        def forbidden(*args,**kwargs):raise AssertionError('must not execute import probe')
        self.assertEqual(reference_environment(self.repo,self.interpreter,forbidden)['status'],'held')
        state=json.loads(marker.read_text());state['expected']['pyamg']='5.3.0';state['target']='different prefix';marker.write_text(json.dumps(state))
        self.assertEqual(reference_environment(self.repo,self.interpreter,forbidden)['status'],'held')
        marker.unlink();marker.symlink_to(self.repo/'missing-state')
        with self.assertRaises(OSError):reference_environment(self.repo,self.interpreter,forbidden)

    def test_actual_make_mutation_goals_route_without_build_fragments(self):
        for folder in ('make',): (self.repo/folder).mkdir()
        for name in ('config.mk','sources-tools.mk','rules-runtime.mk'):
            shutil.copy2(ROOT/'make'/name,self.repo/'make'/name)
        # Stub only the installer: this verifies actual Make argument routing
        # without contacting an index or pretending to qualify numerical packages.
        (self.repo/'scripts/reference_environment_setup.py').write_text("import sys,json;print(json.dumps(sys.argv[1:]))\n")
        shutil.copy2(ROOT/'makefile',self.repo/'makefile')
        for name in ('VERSION','WORKER_VERSION'):(self.repo/name).write_text('test\n')
        environment={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES')}
        result=subprocess.run(['make','cfd-reference-env','cfd-reference-amg-env'],cwd=self.repo,env=environment,capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        rows=[json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(rows),2);self.assertNotIn('--amg',rows[0]);self.assertIn('--amg',rows[1]);self.assertTrue(all('--apply' in row for row in rows))
        self.assertFalse(self.tools.exists())


if __name__=='__main__':unittest.main()
