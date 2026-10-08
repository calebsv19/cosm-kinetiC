"""Retained package assembly with exact reuse and failure preservation."""
import fcntl
import json
import os
from pathlib import Path
import shutil
import subprocess
import signal
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import package_transaction as transaction
from clean_outputs import plan as clean_plan


class Transaction(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();self.root=self.repo/'build/release/job'
        self.input=self.repo/'input';self.input.write_bytes(b'original input')
        self.assembler=self.repo/'assemble.py'
        self.assembler.write_text("from pathlib import Path\nimport sys\na=dict(x.split('=',1) for x in sys.argv[1:])\nd=Path(a['OUT_DIR']);d.mkdir(parents=True);(d/'binary').write_bytes(b'package')\nPath(a['OUT_ARCHIVE']).write_bytes(b'archive')\n")
        self.directories={'OUT_DIR':self.root/'package'};self.files={'OUT_ARCHIVE':self.root/'package.tar'}

    def assemble(self, values=(), tools=()):
        return transaction.run(self.repo,self.root,self.directories,self.files,{},[self.input,self.assembler],list(values),[sys.executable,str(self.assembler)],tools)

    def receipt(self):
        return next((self.root/'.package-transactions').glob('*/receipt.json'))

    def test_input_budget_exhaustion_precedes_output_root_creation(self):
        budget=transaction.InventoryBudget(max_bytes=1)
        with patch('package_transaction.InventoryBudget',return_value=budget):
            with self.assertRaisesRegex(ValueError,'byte bound'):self.assemble()
        self.assertFalse(self.root.exists())

    def test_combined_output_budget_failure_retains_stage_without_publication(self):
        original=transaction.output_inventory
        def bounded(root,outputs):
            return original(root,outputs,budget=transaction.InventoryBudget(max_bytes=10))
        with patch('package_transaction.output_inventory',side_effect=bounded):
            with self.assertRaisesRegex(ValueError,'byte bound'):self.assemble()
        self.assertFalse((self.root/'package').exists())
        self.assertFalse((self.root/'package.tar').exists())
        self.assertEqual((self.receipt().parent/'stage/package/binary').read_bytes(),b'package')
        record=json.loads(self.receipt().read_text())
        self.assertEqual(record['state'],'failed_retained')
        self.assertTrue(record['terminal_processes_verified'])

    def test_completed_readback_reuse_preserves_outputs_and_holds_clean(self):
        result=self.assemble();self.assertEqual(result['status'],'completed')
        before=(self.root/'package.tar').stat().st_mtime_ns
        self.assertEqual(self.assemble()['status'],'verified_reuse')
        self.assertEqual((self.root/'package.tar').stat().st_mtime_ns,before)
        record=json.loads(self.receipt().read_text())
        stage=self.receipt().parent/'stage'
        self.assertEqual((stage/'package/binary').read_bytes(),b'package')
        self.assertEqual(record['published'],['package','package.tar'])
        with self.assertRaises(ValueError):clean_plan(self.repo,self.root,[self.repo/'data'],[])

    def test_changed_inputs_or_tampered_outputs_hold_without_overwrite(self):
        self.assemble();self.input.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'inputs changed'):self.assemble()
        self.assertEqual((self.root/'package.tar').read_bytes(),b'archive')
        self.input.write_bytes(b'original input');(self.root/'package.tar').write_bytes(b'tamper')
        with self.assertRaisesRegex(ValueError,'readback changed'):self.assemble()
        self.assertEqual((self.root/'package.tar').read_bytes(),b'tamper')

    def test_failed_assembly_retains_attempt_and_never_publishes(self):
        self.assembler.write_text("import sys\nprint('failed assembly diagnostic')\nsys.exit(9)\n")
        with self.assertRaises(transaction.ExecutionFailure):self.assemble()
        record=json.loads(self.receipt().read_text());self.assertEqual(record['state'],'failed_retained')
        self.assertIn('failed assembly diagnostic',(self.receipt().parent/'assembly.stdout').read_text())
        self.assertFalse((self.root/'package').exists())
        with self.assertRaisesRegex(ValueError,'reserved'):self.assemble()

    def test_partial_publication_retains_stage_and_holds_unverified_reuse(self):
        rename=transaction.rename_exclusive
        def fail(source,target):
            if Path(target).name=='package.tar':raise OSError('publication failed')
            rename(source,target)
        with patch.object(transaction,'rename_exclusive',side_effect=fail):
            with self.assertRaises(OSError):self.assemble()
        self.assertEqual((self.root/'package/binary').read_bytes(),b'package')
        self.assertEqual((self.receipt().parent/'stage/package.tar').read_bytes(),b'archive')
        self.assertEqual(json.loads(self.receipt().read_text())['published'],['package'])
        with self.assertRaisesRegex(ValueError,'reserved'):self.assemble()

    def test_active_cleanup_prevents_creating_package_root(self):
        locks=self.repo/'tmp/locks';locks.mkdir(parents=True)
        with (locks/'clean.lock').open('w') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError,'active cleanup'):self.assemble()
        self.assertFalse(self.root.exists())

    def test_existing_unknown_output_and_unsafe_mapping_are_held(self):
        self.root.mkdir(parents=True);(self.root/'package.tar').write_bytes(b'predecessor')
        with self.assertRaisesRegex(ValueError,'Existing'):self.assemble()
        self.assertEqual((self.root/'package.tar').read_bytes(),b'predecessor')
        with self.assertRaisesRegex(ValueError,'mapping escapes'):
            transaction.run(self.repo,self.root,self.directories,self.files,{'BAD':self.repo/'src'},[self.input],[],[sys.executable,str(self.assembler)])

    def test_input_drift_during_assembly_refuses_publication(self):
        self.assembler.write_text(self.assembler.read_text()+"Path('"+str(self.input)+"').write_bytes(b'drift')\n")
        with self.assertRaisesRegex(ValueError,'changed during assembly'):self.assemble()
        self.assertFalse((self.root/'package').exists())
        self.assertTrue((self.receipt().parent/'stage/package/binary').exists())

    def test_input_change_during_reuse_is_not_accepted(self):
        self.assemble();original=transaction.output_inventory
        def readback(root,outputs):
            result=original(root,outputs);self.input.write_bytes(b'racing drift');return result
        with patch.object(transaction,'output_inventory',side_effect=readback):
            with self.assertRaisesRegex(ValueError,'during reuse'):self.assemble()
        self.assertEqual((self.root/'package.tar').read_bytes(),b'archive')

    def test_active_package_owner_blocks_competing_assembly(self):
        control=self.root/'.package-transactions';control.mkdir(parents=True)
        with (control/'owner.lock').open('w') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX)
            with self.assertRaisesRegex(ValueError,'another package'):self.assemble()
        self.assertFalse((self.root/'package').exists())

    def test_declared_tool_bytes_bind_reuse(self):
        tool=self.repo/'package-tool';tool.write_text('#!/bin/sh\nexit 0\n');tool.chmod(0o755)
        self.assemble(tools=[str(tool)])
        tool.write_text('#!/bin/sh\nexit 1\n')
        with self.assertRaisesRegex(ValueError,'inputs changed'):self.assemble(tools=[str(tool)])
        self.assertEqual((self.root/'package.tar').read_bytes(),b'archive')

    def test_archive_environment_change_holds_completed_reuse(self):
        self.assemble()
        with patch.dict(os.environ,TAR_OPTIONS='--changed-option-for-identity-only'):
            with self.assertRaisesRegex(ValueError,'inputs changed'):self.assemble()
        self.assertEqual((self.root/'package.tar').read_bytes(),b'archive')

    def partial_attempt(self):
        rename=transaction.rename_exclusive
        def fail(source,target):
            if Path(target).name=='package.tar':raise OSError('publication failed')
            return rename(source,target)
        with patch.object(transaction,'rename_exclusive',side_effect=fail):
            with self.assertRaises(OSError):self.assemble()
        return self.receipt().parent

    def test_partial_recovery_is_readonly_then_completes_without_reassembly(self):
        attempt=self.partial_attempt();receipt=attempt/'receipt.json';original=receipt.read_bytes()
        binary=self.root/'package/binary';before=binary.stat().st_mtime_ns
        planned=transaction.recover(self.repo,attempt)
        self.assertEqual(planned['status'],'plan_only');self.assertEqual(planned['missing'],['package.tar'])
        self.assertFalse((attempt/'recovery').exists())
        result=transaction.recover(self.repo,attempt,apply=True)
        self.assertEqual(result['status'],'completed')
        self.assertEqual((self.root/'package.tar').read_bytes(),b'archive')
        self.assertEqual(binary.stat().st_mtime_ns,before)
        self.assertEqual(receipt.read_bytes(),original)
        self.assertEqual(transaction.recover(self.repo,attempt,apply=True)['status'],'already_completed')
        self.assertEqual(self.assemble()['status'],'verified_reuse')

    def test_recovery_refuses_changed_stage_inputs_or_published_bytes(self):
        attempt=self.partial_attempt()
        paths=(attempt/'stage/package.tar',self.input,self.root/'package/binary')
        for path in paths:
            with self.subTest(path=path):
                original=path.read_bytes();path.write_bytes(b'tampered')
                with self.assertRaises(ValueError):transaction.recover(self.repo,attempt,apply=True)
                self.assertFalse((self.root/'package.tar').exists())
                path.write_bytes(original)
        reservation=self.root/'.package-reservations'
        receipt=next(p for p in reservation.glob('*.json') if json.loads(p.read_text())['output'].endswith('package.tar'))
        row=json.loads(receipt.read_text());row['attempt_id']='another owner';receipt.write_text(json.dumps(row))
        with self.assertRaisesRegex(ValueError,'reservation ownership'):transaction.recover(self.repo,attempt,apply=True)

    def test_recovery_failed_copy_preserves_attempt_and_retries_fresh(self):
        attempt=self.partial_attempt();original=(attempt/'receipt.json').read_bytes()
        with patch.object(transaction.shutil,'copy2',side_effect=OSError('recovery copy failed')):
            with self.assertRaises(OSError):transaction.recover(self.repo,attempt,apply=True)
        self.assertFalse((self.root/'package.tar').exists())
        self.assertEqual((attempt/'receipt.json').read_bytes(),original)
        self.assertEqual(transaction.recover(self.repo,attempt,apply=True)['status'],'completed')
        self.assertEqual(len(list((attempt/'recovery').glob('*/receipt.json'))),2)

    def test_recovery_excludes_cleanup_and_live_package_owner(self):
        attempt=self.partial_attempt()
        for path in (self.repo/'tmp/locks/clean.lock',self.root/'.package-transactions/owner.lock'):
            with path.open('r') as stream:
                fcntl.flock(stream,fcntl.LOCK_EX)
                with self.assertRaisesRegex(ValueError,'active cleanup or package owner'):transaction.recover(self.repo,attempt,apply=True)
        self.assertFalse((self.root/'package.tar').exists())

    def test_sigkill_publication_boundaries_recover_from_observed_files(self):
        code="""
import os, signal, sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import package_transaction as t
repo=Path(sys.argv[2]);root=repo/'build/release/job';boundary=sys.argv[3]
rename=t.rename_exclusive
def crash(source,target):
    if boundary=='before_first':os.kill(os.getpid(),signal.SIGKILL)
    result=rename(source,target)
    if (boundary=='after_first' and Path(target).name=='package') or (boundary=='after_last' and Path(target).name=='package.tar'):
        os.kill(os.getpid(),signal.SIGKILL)
    return result
t.rename_exclusive=crash
t.run(repo,root,{'OUT_DIR':root/'package'},{'OUT_ARCHIVE':root/'package.tar'},{},[repo/'input',repo/'assemble.py'],[],[sys.executable,str(repo/'assemble.py')])
"""
        for boundary in ('before_first','after_first','after_last'):
            with self.subTest(boundary=boundary):
                # Copy only this test's known inputs into a fresh independent root.
                sandbox=self.repo/boundary;sandbox.mkdir()
                shutil.copy2(self.input,sandbox/'input');shutil.copy2(self.assembler,sandbox/'assemble.py')
                process=subprocess.run([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(sandbox),boundary],capture_output=True,text=True,timeout=10)
                self.assertEqual(process.returncode,-signal.SIGKILL,process.stdout+process.stderr)
                root=sandbox/'build/release/job';attempt=next((root/'.package-transactions').glob('*/receipt.json')).parent
                original=(attempt/'receipt.json').read_bytes()
                self.assertEqual(transaction.recover(sandbox,attempt,apply=True)['status'],'completed')
                self.assertEqual((root/'package/binary').read_bytes(),b'package')
                self.assertEqual((root/'package.tar').read_bytes(),b'archive')
                self.assertEqual((attempt/'receipt.json').read_bytes(),original)

    def test_recovery_cli_requires_exact_attempt_and_defaults_to_plan(self):
        attempt=self.partial_attempt()
        command=[sys.executable,'-B',str(ROOT/'scripts/package_transaction.py'),'--recover-attempt',str(attempt)]
        for extra,status in (([],'plan_only'),(['--apply'],'completed')):
            result=subprocess.run(command+extra,cwd=self.repo,capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(json.loads(result.stdout)['status'],status)
        with self.assertRaises(ValueError):transaction.recover(self.repo,self.repo/'input',apply=True)

    def test_sigkill_during_recovery_reconciles_without_changing_original_journal(self):
        attempt=self.partial_attempt();original=(attempt/'receipt.json').read_bytes()
        code="""
import os, signal, sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
import package_transaction as t
rename=t.rename_exclusive
def crash(source,target):
    rename(source,target)
    os.kill(os.getpid(),signal.SIGKILL)
t.rename_exclusive=crash
t.recover(Path(sys.argv[2]),Path(sys.argv[3]),apply=True)
"""
        result=subprocess.run([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(self.repo),str(attempt)],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,-signal.SIGKILL,result.stdout+result.stderr)
        interrupted=next((attempt/'recovery').glob('*/receipt.json'));snapshot=interrupted.read_bytes()
        self.assertEqual(json.loads(snapshot)['state'],'publishing')
        self.assertEqual(transaction.recover(self.repo,attempt,apply=True)['status'],'completed')
        self.assertEqual(interrupted.read_bytes(),snapshot)
        self.assertEqual((attempt/'receipt.json').read_bytes(),original)
        self.assertEqual(self.assemble()['status'],'verified_reuse')

    def test_recovery_refuses_an_occupied_missing_name_and_invalid_contract(self):
        attempt=self.partial_attempt();target=self.root/'package.tar';target.mkdir()
        with self.assertRaises(ValueError):transaction.recover(self.repo,attempt,apply=True)
        self.assertTrue(target.is_dir());target.rmdir()
        receipt=attempt/'receipt.json';row=json.loads(receipt.read_text())
        row['contract']['outputs'][0]['relative']='../escape';receipt.write_text(json.dumps(row))
        with self.assertRaisesRegex(ValueError,'contract identity'):transaction.recover(self.repo,attempt,apply=True)
        self.assertFalse(target.exists())

    def test_assembly_wall_and_log_caps_preserve_partial_stage_without_publication(self):
        for name,extra,options,reason in (
                ('timeout','import time;print("partial timeout",flush=True);time.sleep(10)\n',{'wall_cap':.15},'wall cap'),
                ('overflow','print("x"*10000,flush=True)\n',{'log_cap':64},'log cap')):
            root=self.repo/'build'/name
            self.assembler.write_text('from pathlib import Path\nimport sys\na=dict(x.split("=",1) for x in sys.argv[1:]);d=Path(a["OUT_DIR"]);d.mkdir(parents=True);(d/"partial").write_bytes(b"retained")\n'+extra)
            with self.assertRaisesRegex(ValueError,reason):
                transaction.run(self.repo,root,{'OUT_DIR':root/'package'},{'OUT_ARCHIVE':root/'archive'}, {},[self.input,self.assembler],[],[sys.executable,str(self.assembler)],**options)
            receipt=next((root/'.package-transactions').glob('*/receipt.json'));row=json.loads(receipt.read_text())
            self.assertEqual(row['state'],'failed_retained');self.assertTrue(row['terminal_processes_verified'])
            self.assertFalse((root/'package').exists());self.assertEqual((receipt.parent/'stage/package/partial').read_bytes(),b'retained')
            self.assertTrue((receipt.parent/'assembly.stdout').read_bytes());self.assertEqual(row['published'],[])
            with self.assertRaisesRegex(ValueError,'never reached verified staging'):transaction.recovery_plan(self.repo,receipt.parent)

    def test_uncertain_assembly_teardown_blocks_recovery_and_reuse(self):
        def held(*args):raise transaction.IncompleteTeardown('controlled unfinished assembler')
        with self.assertRaisesRegex(ValueError,'unfinished'):
            transaction.run(self.repo,self.root,self.directories,self.files,{},[self.input,self.assembler],[],[sys.executable,str(self.assembler)],run_command=held)
        receipt=self.receipt();row=json.loads(receipt.read_text())
        self.assertFalse(row['terminal_processes_verified']);self.assertIsNone(row['assembly_exit_code'])
        self.assertEqual(row['state'],'failed_retained');self.assertEqual(row['published'],[])
        with self.assertRaisesRegex(ValueError,'unverified assembly teardown'):transaction.recovery_plan(self.repo,receipt.parent)
        with self.assertRaisesRegex(ValueError,'unverified assembly teardown'):self.assemble()

    @unittest.skipUnless(sys.platform=='darwin','macOS recovery copy supervision')
    def test_unverified_recovery_copy_teardown_blocks_reuse_and_retry(self):
        with patch('package_transaction.rename_exclusive',side_effect=OSError('publication interrupted')):
            with self.assertRaises(OSError):self.assemble()
        attempt=self.receipt().parent
        with patch('package_transaction.execute',side_effect=transaction.IncompleteTeardown('copy teardown unverified')):
            with self.assertRaises(transaction.IncompleteTeardown):transaction.recover(self.repo,attempt,apply=True)
        receipt=next((attempt/'recovery').glob('*/receipt.json'))
        record=json.loads(receipt.read_text())
        self.assertFalse(record['terminal_processes_verified'])
        self.assertEqual(record['state'],'failed_retained')
        with self.assertRaisesRegex(ValueError,'unverified copy teardown'):transaction.recovery_plan(self.repo,attempt)
        with self.assertRaisesRegex(ValueError,'unverified copy teardown'):self.assemble()
        self.assertFalse((self.root/'package').exists())

    def test_assembly_limit_admission_before_output_or_lock_allocation(self):
        for options in ({'wall_cap':0},{'wall_cap':3601},{'wall_cap':float('nan')},{'log_cap':0},{'log_cap':True}):
            with self.assertRaises(ValueError):
                transaction.run(self.repo,self.root,self.directories,self.files,{},[self.input,self.assembler],[],[sys.executable,str(self.assembler)],**options)
        self.assertFalse(self.root.exists());self.assertFalse((self.repo/'tmp').exists())

    def test_cli_sigterm_reaps_assembler_and_releases_package_owner(self):
        pid_file=self.repo/'assembler.pid'
        self.assembler.write_text(f'import os,time\nfrom pathlib import Path\nPath({str(pid_file)!r}).write_text(str(os.getpid()))\nprint("partial assembly",flush=True)\ntime.sleep(30)\n')
        command=[sys.executable,'-B',str(ROOT/'scripts/package_transaction.py'),'--root',str(self.root),
                 '--directory','OUT_DIR='+str(self.root/'package'),'--file','OUT_ARCHIVE='+str(self.root/'package.tar'),
                 '--input',str(self.assembler),'--',sys.executable,str(self.assembler)]
        process=subprocess.Popen(command,cwd=self.repo,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.addCleanup(lambda:process.kill() if process.poll() is None else None)
        deadline=time.monotonic()+5
        while not pid_file.exists() and process.poll() is None and time.monotonic()<deadline:time.sleep(.02)
        self.assertTrue(pid_file.exists());pid=int(pid_file.read_text())
        with (self.root/'.package-transactions/owner.lock').open('r') as lock:
            with self.assertRaises(BlockingIOError):fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            process.send_signal(signal.SIGTERM);out,err=process.communicate(timeout=10)
            self.assertNotEqual(process.returncode,0,out+err)
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        with self.assertRaises(ProcessLookupError):os.kill(pid,0)
        row=json.loads(self.receipt().read_text());self.assertEqual(row['state'],'failed_retained')
        self.assertTrue(row['terminal_processes_verified']);self.assertEqual(row['published'],[])
        self.assertFalse((self.root/'package').exists());self.assertIn('partial assembly',(self.receipt().parent/'assembly.stdout').read_text())

    def test_actual_worker_make_recipe_maps_stage_and_reuses_portable_sidecars(self):
        scripts=self.repo/'scripts';scripts.mkdir()
        for name in ('package_transaction.py','package_outputs.py','build_owner.py','check_clean_root.py','build_outputs.py','clean_outputs.py','desktop_replace.py','contract_proof.py','cfd_evidence.py'):
            shutil.copy2(ROOT/'scripts'/name,scripts/name)
        (scripts/'agent_session').mkdir()
        shutil.copy2(ROOT/'scripts/agent_session/owned_command.py',scripts/'agent_session/owned_command.py')
        writer=self.repo/'tools/packaging/write_linux_worker_artifact_manifest.py'
        writer.parent.mkdir(parents=True);shutil.copy2(ROOT/'tools/packaging'/writer.name,writer)
        for folder in ('make','config','docs'):(self.repo/folder).mkdir()
        for name in ('README.md','docs/README.md','docs/headless_cli.md','VERSION','WORKER_VERSION'):(self.repo/name).write_text('fixture\n')
        for name in ('headless','jobrunner'):(self.repo/name).write_bytes(b'fake executable')
        source=(ROOT/'make/package-linux-worker.mk').read_text()
        (self.repo/'make/worker.mk').write_text(source)
        prefix=source.split('.PHONY:',1)[0]
        recipes='package-linux-worker:'+source.split('package-linux-worker:',1)[1].split('package-linux-worker-self-test:',1)[0]
        values={'RELEASE_DIR':str(self.root),'RELEASE_VERSION':'0.4.0','WORKER_VERSION':'0.3.4','RELEASE_PROGRAM_KEY':'physics_sim','PHYSICS_SIM_HEADLESS_TOOL_BIN':str(self.repo/'headless'),'PHYSICS_SIM_JOB_RUNNER_TOOL_BIN':str(self.repo/'jobrunner')}
        text='\n'.join(k+' := '+v for k,v in values.items())+'\n'+prefix+'\npackage-linux-worker-host-check physics_sim_headless physics-sim-job-runner:\n\t@true\n'+recipes
        (self.repo/'makefile').write_text(text)
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES') and not k.startswith('PHYSICS_SIM_BUILD_')}
        for count in range(2):
            result=subprocess.run(['make','-j2','package-linux-worker'],cwd=self.repo,env=env,capture_output=True,text=True,timeout=15)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('completed' if count==0 else 'verified_reuse',result.stdout)
            self.assertNotIn('jobserver unavailable',result.stderr)
        checksum=next(self.root.glob('*.sha256')).read_text()
        self.assertNotIn('.package-transactions',checksum)
        self.assertEqual(len(list((self.root/'.package-transactions').glob('*/receipt.json'))),1)


if __name__=='__main__':unittest.main()
