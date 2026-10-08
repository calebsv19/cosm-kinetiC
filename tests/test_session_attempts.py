"""Failed/normal local worker attempts retain inputs, output and terminal records."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service
import attempts as attempt_module
from attempts import attempt
from owned_command import execute,IncompleteTeardown


class Attempts(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();self.worker=self.root/'fake-worker'
        self.worker.write_text('#!'+sys.executable+'\nimport sys\nprint("fixture diagnostic")\nraise SystemExit(7)\n');self.worker.chmod(0o755)

    def receipts(self):return sorted((self.root/'attempts').glob('*/receipt.json'))

    def test_failed_author_reruns_keep_distinct_inputs_and_logs(self):
        service=Service(self.root,worker=self.worker)
        for _ in range(2):
            with self.assertRaises(ValueError) as failed:service.scene_create('scene')
            self.assertIn('retained attempt:',str(failed.exception))
        receipts=self.receipts();self.assertEqual(len(receipts),2)
        for path in receipts:
            receipt=json.loads(path.read_text());self.assertEqual(receipt['status'],'failed_retained')
            self.assertTrue(receipt['terminal_processes_verified']);self.assertTrue((path.parent/'stage/scene_authoring.json').exists())
            self.assertIn('fixture diagnostic',(path.parent/'compile.stdout').read_text())
        self.assertFalse((self.root/'scenes/scene').exists())

    def test_failed_validation_keeps_request_scene_and_output(self):
        service=Service(self.root,worker=self.worker)
        with patch.object(service,'request',return_value=({'grid':[8,8,8]}, {'schema':'fixture'})),patch.object(service,'_copy_assets'):
            with self.assertRaises(ValueError):service.scene_validate('scene','revision')
        path=self.receipts()[0];self.assertEqual(json.loads(path.read_text())['phase'],'validate')
        self.assertTrue((path.parent/'stage/request.json').exists());self.assertTrue((path.parent/'stage/scene_runtime.json').exists())
        self.assertIn('fixture diagnostic',(path.parent/'validate.stdout').read_text())

    def test_timeout_and_log_limit_reap_commands_and_keep_attempts(self):
        for tag,code,wall,cap in (('timeout','import time;time.sleep(10)',.1,1048576),('logs','print("x"*10000)',2,64)):
            with self.assertRaises(ValueError):
                with attempt(self.root,'validate',self.worker,{'fixture':tag}) as (capsule,stage,state):
                    execute([sys.executable,'-c',code],self.root,capsule,tag,(),wall,cap)
        self.assertEqual(len(self.receipts()),2)
        self.assertTrue(all(json.loads(path.read_text())['terminal_processes_verified'] for path in self.receipts()))

    def test_unknown_teardown_is_held_not_completed(self):
        with self.assertRaises(IncompleteTeardown):
            with attempt(self.root,'validate',self.worker,{}) as (_,_,_):raise IncompleteTeardown('fixture')
        receipt=json.loads(self.receipts()[0].read_text());self.assertEqual(receipt['status'],'held')
        self.assertFalse(receipt['terminal_processes_verified'])

    def test_worker_drift_holds_attempt(self):
        with self.assertRaisesRegex(ValueError,'changed'):
            with attempt(self.root,'validate',self.worker,{}) as (_,_,_):self.worker.write_bytes(b'changed')
        self.assertEqual(json.loads(self.receipts()[0].read_text())['status'],'failed_retained')

    def test_linked_attempt_storage_is_held_without_writing_external_root(self):
        external=self.root/'external';external.mkdir();(self.root/'attempts').symlink_to(external,target_is_directory=True)
        with self.assertRaises(ValueError):
            with attempt(self.root,'validate',self.worker,{}) as (_,_,_):pass
        self.assertEqual(list(external.iterdir()),[])

    def test_unavailable_worker_keeps_initial_request_and_failure(self):
        self.worker.unlink()
        with self.assertRaises(OSError):
            with attempt(self.root,'validate',self.worker,{'source':'retained'}) as (_,_,_):pass
        path=self.receipts()[0];self.assertEqual(json.loads(path.read_text())['status'],'failed_retained')
        self.assertEqual(json.loads((path.parent/'request.json').read_text())['request'],{'source':'retained'})

    def test_malformed_successful_validation_is_retained_as_failure(self):
        self.worker.write_text('#!'+sys.executable+'\nprint("{}")\n');self.worker.chmod(0o755)
        service=Service(self.root,worker=self.worker)
        with patch.object(service,'request',return_value=({'grid':[8,8,8]},{})),patch.object(service,'_copy_assets'):
            with self.assertRaisesRegex(ValueError,'output_invalid'):service.scene_validate('scene','revision')
        path=self.receipts()[0];self.assertEqual(json.loads(path.read_text())['status'],'failed_retained')
        self.assertEqual((path.parent/'validate.stdout').read_text().strip(),'{}')

    def test_published_scene_reuse_requires_completed_author_receipt(self):
        self.worker.write_text('#!'+sys.executable+'\nimport shutil,sys\nshutil.copyfile(sys.argv[2],sys.argv[3])\n');self.worker.chmod(0o755)
        service=Service(self.root,worker=self.worker);scene=service.scene_create('scene')
        self.assertEqual(service.scene_create('scene')['scene_revision'],scene['scene_revision'])
        path=self.receipts()[0];receipt=json.loads(path.read_text());self.assertEqual(receipt['status'],'completed')
        receipt['status']='failed_retained';path.write_text(json.dumps(receipt))
        with self.assertRaisesRegex(ValueError,'not verified'):service.scene_create('scene')
        with self.assertRaisesRegex(ValueError,'not verified'):service.request('scene',scene['scene_revision'])

    def test_worker_and_control_payloads_are_preserved_with_hashes(self):
        import hashlib
        self.worker.write_text('#!'+sys.executable+'\nprint("verified worker")\n');self.worker.chmod(0o755)
        before=self.worker.read_bytes()
        with attempt(self.root,'validate',self.worker,{}) as (capsule,_,state):
            state['command']=execute([str(self.worker)],self.root,capsule,'fixture',(),2,1024)
        self.assertEqual((capsule/'source/worker').read_bytes(),before)
        receipt=json.loads((capsule/'receipt.json').read_text())
        self.assertFalse(receipt['complete_dependency_capture'])
        for name,digest in receipt['control_sha256'].items():
            self.assertEqual(hashlib.sha256((capsule/'source/runtime_modules'/name).read_bytes()).hexdigest(),digest)
        self.assertIn('owned_command.py',receipt['control_sha256'])

    def test_control_drift_since_import_stops_before_worker_effect(self):
        changed=dict(attempt_module.CONTROL_BASELINE);changed['service.py']+=b'changed'
        entered=False
        with patch.object(attempt_module,'capture_controls',return_value=changed):
            with self.assertRaisesRegex(ValueError,'since import'):
                with attempt(self.root,'validate',self.worker,{}) as (_,_,_):entered=True
        self.assertFalse(entered)
        self.assertEqual(json.loads(self.receipts()[0].read_text())['status'],'failed_retained')

    def test_mid_attempt_control_drift_preserves_original_sources(self):
        original=dict(attempt_module.CONTROL_BASELINE);changed=dict(original);changed['owned_command.py']+=b'changed'
        with patch.object(attempt_module,'capture_controls',side_effect=[original,changed]):
            with self.assertRaisesRegex(ValueError,'during attempt'):
                with attempt(self.root,'validate',self.worker,{}) as (capsule,_,_):pass
        self.assertEqual((capsule/'source/runtime_modules/owned_command.py').read_bytes(),original['owned_command.py'])
        self.assertEqual(json.loads((capsule/'receipt.json').read_text())['status'],'failed_retained')

    def test_control_capture_budget_and_symlinks_hold(self):
        directory=self.root/'controls';directory.mkdir();(directory/'module.py').write_bytes(b'control')
        with patch.object(attempt_module,'MAX_CONTROL_BYTES',1):
            with self.assertRaisesRegex(ValueError,'byte bound'):attempt_module.capture_controls(directory)
        with patch.object(attempt_module,'MAX_CONTROL_FILES',0):
            with self.assertRaisesRegex(ValueError,'file bound'):attempt_module.capture_controls(directory)
        (directory/'module.py').unlink();(directory/'module.py').symlink_to(self.worker)
        with self.assertRaises(ValueError):attempt_module.capture_controls(directory)

    def test_worker_permission_drift_and_non_executable_worker_hold(self):
        self.worker.chmod(0o644)
        with self.assertRaisesRegex(ValueError,'not executable'):
            with attempt(self.root,'validate',self.worker,{}) as (_,_,_):pass
        self.worker.chmod(0o755)
        with self.assertRaisesRegex(ValueError,'permissions changed'):
            with attempt(self.root,'validate',self.worker,{}) as (_,_,_):self.worker.chmod(0o644)
        self.assertEqual(len(self.receipts()),2)

    def test_retained_payload_drift_prevents_completion(self):
        for kind in ('worker','control','unknown'):
            with self.assertRaises(ValueError):
                with attempt(self.root,'validate',self.worker,{}) as (capsule,_,_):
                    source=capsule/'source'
                    if kind=='worker':(source/'worker').write_bytes(b'changed snapshot')
                    elif kind=='control':(source/'runtime_modules/owned_command.py').write_bytes(b'changed snapshot')
                    else:(source/'unknown').write_bytes(b'unknown')
        self.assertTrue(all(json.loads(path.read_text())['status']=='failed_retained' for path in self.receipts()))

    def test_unverified_worker_completion_is_held(self):
        with self.assertRaises(IncompleteTeardown):
            with attempt(self.root,'validate',self.worker,{}) as (_,_,_):pass
        receipt=json.loads(self.receipts()[0].read_text())
        self.assertEqual(receipt['status'],'held');self.assertFalse(receipt['terminal_processes_verified'])

    def test_storage_drift_leaves_original_receipt_held_and_replacement_untouched(self):
        from service import SessionError
        original=self.root/'session';service=Service(original,worker=self.worker)
        with self.assertRaisesRegex(SessionError,'identity changed'):
            with attempt(original,'validate',self.worker,{},storage_guard=service._assert_storage) as (capsule,_,_):
                original.rename(self.root/'retained-session');original.mkdir()
        receipt=next((self.root/'retained-session/attempts').glob('*/receipt.json'))
        row=json.loads(receipt.read_text());self.assertEqual(row['status'],'running');self.assertFalse(row['terminal_processes_verified'])
        self.assertTrue(row['storage_guard_applied']);self.assertEqual(list(original.iterdir()),[])
        service.close()

    def test_threaded_validation_does_not_require_signal_registration(self):
        import concurrent.futures
        directory=self.root/'logs';directory.mkdir()
        with concurrent.futures.ThreadPoolExecutor(1) as pool:
            result=pool.submit(execute,[sys.executable,'-c','print("done")'],self.root,directory,'thread',(),2,1024).result(timeout=5)
        self.assertEqual(result['exit_code'],0)

if __name__=='__main__':unittest.main()
