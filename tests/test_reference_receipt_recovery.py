"""Read-only candidate plans and explicit digest-bound publication recovery."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c
import reference_receipt_recovery as recovery

class Recovery(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.repo=Path(self.tmp.name).resolve();self.data=self.repo/'build/c3d-test/runs'
        hashes={'probe.py':hashlib.sha256(b'# source').hexdigest()};digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
        self.run=self.data/digest;(self.run/'source').mkdir(parents=True);(self.run/'source/probe.py').write_bytes(b'# source')
        self.path=self.run/'proof-receipt.json';self.log=self.run/'proof.log';self.log.write_bytes(b'known')
        self.value={'command':['owned'],'source_sha256':hashes,'returncode':0,'stop_reason':None,'supervision':{'all_external_descendants_verified_terminal':False,'direct_child_reaped':True,'group_cleanup_identity_anchored':True,'terminal_processes_verified':True},'artifact_sha256':{str(self.log):hashlib.sha256(b'known').hexdigest()}}
        self.context={'command':self.value['command'],'source_sha256':hashes,'artifact_paths':[str(self.log)]}
        with c.reference_ownership(self.repo,self.data),patch.object(c.os,'link',side_effect=OSError('controlled before-link interruption')):
            with self.assertRaises(OSError):c.publish_reference_receipt(self.path,self.value)
        self.attempt=next((self.run/'.receipt-attempts').iterdir());self.candidate=self.attempt/'receipt.candidate.json';self.sha=c.factor_digest(self.candidate)
    def plan(self):return recovery.plan(self.repo,self.data,self.path,self.attempt,self.context)
    def apply(self,sha=None):return recovery.apply(self.repo,self.data,self.path,self.attempt,self.context,sha or self.sha)
    def snapshot(self):return {str(p.relative_to(self.repo)):(p.stat().st_ino,p.stat().st_mtime_ns,p.read_bytes()) for p in self.repo.rglob('*') if p.is_file()}
    def test_plan_is_read_only_and_apply_publishes_exact_bytes_idempotently(self):
        before=self.snapshot();row=self.plan();self.assertEqual(row['status'],'ready_to_publish');self.assertEqual(self.snapshot(),before)
        result=self.apply();self.assertTrue(result['mutation_performed']);self.assertEqual(self.path.read_bytes(),self.candidate.read_bytes())
        before=self.snapshot();self.assertEqual(self.apply()['status'],'already_published');self.assertEqual(self.snapshot(),before)
    def test_predecessor_and_changed_planned_digest_hold_without_replacement(self):
        with self.assertRaises(ValueError):self.apply('0'*64)
        self.assertFalse(self.path.exists());self.path.write_bytes(b'predecessor');before=self.snapshot()
        with self.assertRaises(ValueError):self.apply()
        self.assertEqual(self.snapshot(),before)
    def test_changed_artifact_source_request_or_candidate_holds(self):
        original={p:p.read_bytes() for p in (self.log,self.run/'source/probe.py',self.attempt/'request.json',self.candidate)}
        for p,data in original.items():
            with self.subTest(path=p):
                p.write_bytes(b'changed');before=self.snapshot()
                with self.assertRaises(ValueError):self.plan()
                self.assertEqual(self.snapshot(),before);p.write_bytes(data)
    def test_active_namespace_prevents_inspection_and_apply(self):
        with c.reference_ownership(self.repo,self.data):
            with self.assertRaisesRegex(ValueError,'active or held'):self.plan()
            with self.assertRaises(ValueError):self.apply()
        self.assertFalse(self.path.exists())
    def test_missing_lock_namespace_is_not_created_by_plan(self):
        other=self.repo/'build/c3d-unowned/runs';before=self.snapshot()
        with self.assertRaises(FileNotFoundError):recovery.plan(self.repo,other,self.path,self.attempt,self.context)
        self.assertEqual(self.snapshot(),before);self.assertFalse(other.exists())
    def test_unverified_terminal_scope_and_wrong_context_hold(self):
        value=dict(self.value);value['supervision']={'all_external_descendants_verified_terminal':False}
        self.candidate.write_text(json.dumps(value));request=c.factor_json(self.attempt/'request.json');request['bytes']=self.candidate.stat().st_size;request['sha256']=c.factor_digest(self.candidate);(self.attempt/'request.json').write_text(json.dumps(request))
        with self.assertRaisesRegex(ValueError,'terminal'):self.plan()
        bad={**self.context,'command':['other']}
        with self.assertRaises(ValueError):recovery.plan(self.repo,self.data,self.path,self.attempt,bad)
    def test_failed_outcome_is_recovered_without_becoming_success(self):
        value={**self.value,'returncode':2,'diagnostic_failure':{'kind':'child_error'}}
        self.candidate.write_text(json.dumps(value));request=c.factor_json(self.attempt/'request.json');request['bytes']=self.candidate.stat().st_size;request['sha256']=c.factor_digest(self.candidate);(self.attempt/'request.json').write_text(json.dumps(request));self.sha=request['sha256']
        self.assertEqual(self.apply()['status'],'published_and_verified')
        record=c.factor_json(self.path);self.assertEqual(record['returncode'],2);self.assertEqual(record['diagnostic_failure']['kind'],'child_error')
    def test_raced_final_predecessor_is_preserved(self):
        original=os.link;before=self.candidate.read_bytes()
        def raced(source,target,**kwargs):
            Path(target).write_bytes(b'raced predecessor');return original(source,target,**kwargs)
        with patch.object(recovery.os,'link',side_effect=raced),self.assertRaises(FileExistsError):self.apply()
        self.assertEqual(self.path.read_bytes(),b'raced predecessor');self.assertEqual(self.candidate.read_bytes(),before)
    def test_cli_defaults_read_only_and_requires_apply_digest(self):
        context=self.repo/'context.json';context.write_text(json.dumps(self.context));before=self.snapshot()
        command=[sys.executable,'-B',str(ROOT/'scripts/reference_receipt_recovery.py'),'--repo',str(self.repo),'--data',str(self.data),'--receipt',str(self.path),'--attempt',str(self.attempt),'--context',str(context)]
        result=subprocess.run(command,capture_output=True,text=True,timeout=5);self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertEqual(json.loads(result.stdout)['status'],'ready_to_publish');self.assertEqual(self.snapshot(),before)
        result=subprocess.run([*command,'--apply'],capture_output=True,text=True,timeout=5);self.assertEqual(result.returncode,2);self.assertFalse(self.path.exists())

if __name__=='__main__':unittest.main()
