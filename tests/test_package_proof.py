"""Fresh package proofs retain success/failure diagnostics without parent resets."""
import fcntl
import json
import os
import signal
import time
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import importlib.util

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import package_proof
from clean_outputs import plan


class Proof(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();self.root=self.repo/'build/package-proofs'
        self.input=self.repo/'artifact';self.input.write_bytes(b'candidate')
        self.script=self.repo/'proof.py'
        self.script.write_text("from pathlib import Path\nimport sys\na=dict(x.split('=',1) for x in sys.argv[1:]);p=Path(a['PACKAGE_PROOF_DIR']);(p/'diagnostic').write_text('retained');print('proof diagnostic')\n")

    def run_proof(self):
        return package_proof.run(self.repo,self.root,'fake-self-test',[self.input],[],{},[sys.executable,str(self.script)])

    def test_success_is_fresh_retained_bound_to_inputs_and_held_from_clean(self):
        self.root.mkdir(parents=True);(self.root/'sentinel').write_bytes(b'preexisting')
        first=self.run_proof();second=self.run_proof()
        self.assertNotEqual(first['work'],second['work'])
        self.assertEqual((self.root/'sentinel').read_bytes(),b'preexisting')
        for result in (first,second):
            row=json.loads(Path(result['receipt']).read_text());self.assertEqual(row['state'],'passed')
            self.assertIn(str(self.input),row['input_inventory']['inputs'])
            self.assertFalse(row['automatic_removal']);self.assertFalse(row['installed_or_published_state_verified'])
            self.assertEqual((Path(result['work'])/'diagnostic').read_text(),'retained')
            self.assertIn('proof diagnostic',Path(result['log']).read_text())
        with self.assertRaises(ValueError):plan(self.repo,self.root,[self.repo/'data'],[])

    def test_failure_keeps_log_and_work(self):
        self.script.write_text(self.script.read_text()+"raise SystemExit(7)\n")
        with self.assertRaisesRegex(ValueError,'command exited 7'):self.run_proof()
        receipt=next((self.root/'.package-proofs').glob('*/receipt.json'))
        row=json.loads(receipt.read_text());self.assertEqual(row['state'],'failed_retained');self.assertEqual(row['exit_code'],7)
        self.assertEqual((receipt.parent/'work/diagnostic').read_text(),'retained')

    def test_input_change_during_proof_cannot_report_pass(self):
        self.script.write_text(self.script.read_text()+"Path('"+str(self.input)+"').write_bytes(b'changed')\n")
        with self.assertRaisesRegex(ValueError,'inputs changed'):self.run_proof()
        receipt=next((self.root/'.package-proofs').glob('*/receipt.json'))
        self.assertEqual(json.loads(receipt.read_text())['state'],'failed_retained')

    def test_active_cleanup_and_package_assembly_hold_before_capsule_creation(self):
        for path in (self.repo/'tmp/locks/clean.lock',self.root/'.package-transactions/owner.lock'):
            path.parent.mkdir(parents=True,exist_ok=True)
            with path.open('w') as lock:
                fcntl.flock(lock,fcntl.LOCK_EX)
                with self.assertRaisesRegex(ValueError,'held by active'):self.run_proof()
            self.assertFalse((self.root/'.package-proofs').exists())

    def test_mapping_escape_and_source_parent_are_refused(self):
        with self.assertRaises(ValueError):package_proof.run(self.repo,self.root,'fake',[self.input],[],{'PATH':'../../escape'},[sys.executable,str(self.script)])
        with self.assertRaises(ValueError):package_proof.run(self.repo,self.repo/'src','fake',[self.input],[],{},[sys.executable,str(self.script)])
        self.assertFalse((self.repo/'src').exists())

    def test_concurrent_cli_proofs_allocate_distinct_capsules(self):
        command=[sys.executable,'-B',str(ROOT/'scripts/package_proof.py'),'--root',str(self.root),'--name','concurrent','--input',str(self.input),'--',sys.executable,str(self.script)]
        processes=[subprocess.Popen(command,cwd=self.repo,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) for _ in range(2)]
        results=[]
        for process in processes:
            out,err=process.communicate(timeout=10);self.assertEqual(process.returncode,0,out+err)
            results.append(json.loads(out))
        self.assertNotEqual(results[0]['work'],results[1]['work'])
        self.assertTrue(all((Path(result['work'])/'diagnostic').exists() for result in results))

    def test_wall_and_log_bounds_retain_partial_diagnostics(self):
        for name,body,limits,expected in (
                ('timeout','import time\nprint("partial diagnostic",flush=True)\ntime.sleep(10)\n',{'wall_cap':.15},'wall cap'),
                ('overflow','print("x"*10000,flush=True)\n',{'log_cap':64},'log cap')):
            self.script.write_text(body)
            with self.assertRaisesRegex(ValueError,expected):
                package_proof.run(self.repo,self.root,name,[self.input],[],{},[sys.executable,str(self.script)],**limits)
            receipt=next((self.root/'.package-proofs').glob(name+'-*/receipt.json'))
            row=json.loads(receipt.read_text());self.assertEqual(row['state'],'failed_retained');self.assertTrue(row['terminal_processes_verified'])
            self.assertTrue((receipt.parent/'command.stdout').read_bytes())
            self.assertTrue((receipt.parent/'command.stderr').is_file())

    def test_unverified_teardown_is_held_and_never_inventories_active_work(self):
        def held(*args):raise package_proof.IncompleteTeardown('controlled unfinished package child')
        with self.assertRaisesRegex(ValueError,'unfinished'):
            package_proof.run(self.repo,self.root,'held',[self.input],[],{},[sys.executable,str(self.script)],run_command=held)
        receipt=next((self.root/'.package-proofs').glob('held-*/receipt.json'));row=json.loads(receipt.read_text())
        self.assertEqual(row['state'],'failed_retained');self.assertFalse(row['terminal_processes_verified']);self.assertNotIn('work_inventory',row)
        self.assertIsNone(row['exit_code'])
        with self.assertRaises(ValueError):plan(self.repo,self.root,[],[])

    def test_resource_limit_admission_precedes_generated_allocation(self):
        for options in ({'wall_cap':0},{'wall_cap':3601},{'log_cap':0},{'log_cap':67108865},{'log_cap':True}):
            with self.assertRaises(ValueError):
                package_proof.run(self.repo,self.root,'bad-limit',[self.input],[],{},[sys.executable,str(self.script)],**options)
        self.assertFalse(self.root.exists());self.assertFalse((self.repo/'tmp').exists())

    def test_unreadable_work_inventory_still_has_terminal_failed_receipt(self):
        self.script.write_text('import os,sys\nfrom pathlib import Path\na=dict(x.split("=",1) for x in sys.argv[1:]);os.mkfifo(Path(a["PACKAGE_PROOF_DIR"])/"special")\n')
        with self.assertRaisesRegex(ValueError,'special file'):self.run_proof()
        receipt=next((self.root/'.package-proofs').glob('*/receipt.json'));row=json.loads(receipt.read_text())
        self.assertEqual(row['state'],'failed_retained');self.assertTrue(row['terminal_processes_verified']);self.assertIn('special file',row['work_inventory_error'])
        self.assertTrue((receipt.parent/'work/special').exists())

    def test_cli_sigterm_reaps_owned_child_and_releases_cleanup_exclusion(self):
        pid_file=self.repo/'child.pid'
        self.script.write_text(f'import os,time\nfrom pathlib import Path\nPath({str(pid_file)!r}).write_text(str(os.getpid()))\nprint("started",flush=True)\ntime.sleep(30)\n')
        command=[sys.executable,'-B',str(ROOT/'scripts/package_proof.py'),'--root',str(self.root),'--name','interrupt','--input',str(self.input),'--',sys.executable,str(self.script)]
        process=subprocess.Popen(command,cwd=self.repo,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.addCleanup(lambda: process.kill() if process.poll() is None else None)
        deadline=time.monotonic()+5
        while not pid_file.exists() and process.poll() is None and time.monotonic()<deadline:time.sleep(.02)
        self.assertTrue(pid_file.exists());pid=int(pid_file.read_text())
        with (self.repo/'tmp/locks/clean.lock').open('r') as lock:
            with self.assertRaises(BlockingIOError):fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            process.send_signal(signal.SIGTERM);out,err=process.communicate(timeout=10)
            self.assertNotEqual(process.returncode,0,out+err)
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        with self.assertRaises(ProcessLookupError):os.kill(pid,0)
        receipt=next((self.root/'.package-proofs').glob('interrupt-*/receipt.json'));row=json.loads(receipt.read_text())
        self.assertEqual(row['state'],'failed_retained');self.assertTrue(row['terminal_processes_verified'])
        self.assertIn('interrupted by signal',row['failure']);self.assertIn('started',(receipt.parent/'command.stdout').read_text())

    def session_app(self):
        app=self.repo/'dist/session.app';script=app/'Contents/Resources/scripts/physics_sim_session.py'
        script.parent.mkdir(parents=True)
        worker=app/'Contents/MacOS/physics_sim_session_worker';worker.parent.mkdir(parents=True);worker.write_bytes(b'fake worker')
        script.write_text("import json,sys\nprint('session diagnostic',file=sys.stderr)\nfor line in sys.stdin:\n r=json.loads(line);result={'tools':[{'name':n} for n in ('capabilities','run_start','run_control','run_sample')]} if r['id']==2 else {'isError':False};print(json.dumps({'id':r['id'],'result':result}))\n")
        return app

    def test_session_validator_retains_requests_logs_and_holds_used_output(self):
        app=self.session_app();output=self.root/'session-proof'
        command=[sys.executable,'-B',str(ROOT/'tools/packaging/validate_macos_session.py'),'--app',str(app),'--output-root',str(output)]
        result=subprocess.run(command,capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['status'],'passed')
        execution=json.loads((output/'execution.json').read_text())
        runtime=Path(execution['runtime_root'])
        self.assertFalse(runtime.is_relative_to(self.repo.resolve()))
        self.assertFalse(runtime.exists())
        requests=(output/'requests.jsonl').read_bytes();log=(output/'stdout.log').read_bytes()
        self.assertEqual(len(log.splitlines()),3)
        self.assertIn('session diagnostic',(output/'stderr.log').read_text())
        repeated=subprocess.run(command,capture_output=True,text=True,timeout=10)
        self.assertNotEqual(repeated.returncode,0)
        self.assertEqual((output/'requests.jsonl').read_bytes(),requests)
        self.assertEqual((output/'stdout.log').read_bytes(),log)

    def test_session_validator_timeout_keeps_partial_diagnostics(self):
        app=self.session_app();output=self.root/'timeout-proof'
        spec=importlib.util.spec_from_file_location('session_validator',ROOT/'tools/packaging/validate_macos_session.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with patch.object(sys,'argv',['validator','--app',str(app),'--output-root',str(output)]),patch.object(module.subprocess,'run',side_effect=subprocess.TimeoutExpired('fake',30,output=b'partial reply',stderr=b'partial error')):
            with self.assertRaisesRegex(SystemExit,'retained'):module.main()
        self.assertEqual((output/'stdout.log').read_text(),'partial reply')
        self.assertEqual((output/'stderr.log').read_text(),'partial error')
        self.assertEqual(json.loads((output/'execution.json').read_text())['state'],'timed_out')

    def test_actual_macos_self_test_recipe_retains_support_on_two_runs(self):
        scripts=self.repo/'scripts';scripts.mkdir()
        for name in ('package_proof.py','package_transaction.py','package_outputs.py','package_paths.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py','desktop_replace.py','contract_proof.py','cfd_evidence.py'):
            shutil.copy2(ROOT/'scripts'/name,scripts/name)
        (scripts/'agent_session').mkdir()
        shutil.copy2(ROOT/'scripts/agent_session/owned_command.py',scripts/'agent_session/owned_command.py')
        app=self.repo/'dist/kinetiC.app';macos=app/'Contents/MacOS';macos.mkdir(parents=True)
        launcher=macos/'physics-sim-launcher';launcher.write_text('#!/bin/sh\nmkdir -p "$PHYSICS_SIM_APP_SUPPORT_DIR"\nprintf kept > "$PHYSICS_SIM_APP_SUPPORT_DIR/result"\n');launcher.chmod(0o755)
        validator=self.repo/'tools/packaging/validate_macos_session.py';validator.parent.mkdir(parents=True);validator.write_text("print('fake validator')\n")
        source=(ROOT/'make/package-macos.mk').read_text()
        recipe='package-desktop-self-test:'+source.split('package-desktop-self-test:',1)[1].split('package-desktop-copy-desktop:',1)[0]
        (self.repo/'makefile').write_text('DIST_DIR := '+str(self.repo/'dist')+'\nPACKAGE_APP_DIR := '+str(app)+'\nPACKAGE_MACOS_DIR := '+str(macos)+'\nPACKAGE_PROFILE := fixture\npackage-desktop-smoke:\n\t@true\n'+recipe)
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES') and not k.startswith('PHYSICS_SIM_BUILD_')}
        for count in range(2):
            result=subprocess.run(['make','-j2','package-desktop-self-test'],cwd=self.repo,env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertNotIn('jobserver unavailable',result.stderr)
        receipts=list((self.repo/'dist/.package-proofs').glob('*/receipt.json'));self.assertEqual(len(receipts),2)
        for receipt in receipts:
            support=Path((receipt.parent/'work/support-root.txt').read_text().strip())
            self.addCleanup(shutil.rmtree,support)
            self.assertFalse(support.is_relative_to(self.repo.resolve()))
            self.assertEqual((support/'result').read_text(),'kept')


if __name__=='__main__':unittest.main()
