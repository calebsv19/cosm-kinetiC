"""Complete retained receipt bytes publish atomically without replacement."""
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_run_support as c

class Publication(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.repo=Path(self.tmp.name).resolve();self.data=self.repo/'build/c3d-test/runs';self.run=self.data/'run';self.run.mkdir(parents=True)
        self.path=self.run/'proof-receipt.json';self.artifact=self.run/'proof.log';self.artifact.write_bytes(b'known')
        self.value={'command':['owned'],'source_sha256':{'probe':'a'*64},'returncode':0,'stop_reason':None,'supervision':{'all_external_descendants_verified_terminal':False},'artifact_sha256':{str(self.artifact):hashlib.sha256(b'known').hexdigest()}}
    def publish(self):
        with c.reference_ownership(self.repo,self.data):return c.publish_reference_receipt(self.path,self.value)
    def test_success_retains_candidate_and_exact_readback(self):
        result=self.publish();candidate=Path(result['candidate'])
        self.assertEqual(self.path.read_bytes(),candidate.read_bytes());self.assertEqual(c.factor_json(self.path),self.value)
        request=c.factor_json(candidate.parent/'request.json');self.assertEqual(request['sha256'],hashlib.sha256(candidate.read_bytes()).hexdigest())
        self.assertEqual(request['bytes'],len(candidate.read_bytes()));self.assertFalse(request['replacement_allowed'])
        self.assertEqual(c.reference_receipt(self.path,self.run,self.value['command'],self.value['source_sha256'],expected_paths=(self.artifact,)),self.value)
    def test_existing_final_is_preserved_and_new_candidate_retained(self):
        self.path.write_bytes(b'predecessor')
        with self.assertRaises(FileExistsError):self.publish()
        self.assertEqual(self.path.read_bytes(),b'predecessor')
        candidate=next((self.run/'.receipt-attempts').glob('*/receipt.candidate.json'));self.assertEqual(c.factor_json(candidate),self.value)
    def test_raced_destination_cannot_be_replaced(self):
        link=os.link
        def collision(source,target,**kwargs):
            Path(target).write_bytes(b'raced predecessor');return link(source,target,**kwargs)
        with patch.object(c.os,'link',side_effect=collision),self.assertRaises(FileExistsError):self.publish()
        self.assertEqual(self.path.read_bytes(),b'raced predecessor');self.assertTrue(list((self.run/'.receipt-attempts').glob('*/receipt.candidate.json')))
    def test_failed_candidate_sync_keeps_final_absent_and_candidate_retained(self):
        sync=os.fsync;calls=0
        def fail(fd):
            nonlocal calls
            calls+=1
            if calls==2:raise OSError('controlled candidate fsync failure')
            return sync(fd)
        with patch.object(c.os,'fsync',side_effect=fail),self.assertRaises(OSError):self.publish()
        self.assertFalse(self.path.exists());candidate=next((self.run/'.receipt-attempts').glob('*/receipt.candidate.json'));self.assertEqual(c.factor_json(candidate),self.value)
    def test_invalid_or_oversized_receipt_holds_before_attempt_allocation(self):
        for value in ({'bad':float('nan')},{'oversize':'x'*16777216},{'deep':[[[[[[0]]]]]]}):
            if 'deep' in value:
                item=0
                for _ in range(65):item=[item]
                value={'deep':item}
            self.value=value
            with self.assertRaises(ValueError):self.publish()
            self.assertFalse((self.run/'.receipt-attempts').exists());self.assertFalse(self.path.exists())
    def test_linked_attempt_storage_does_not_touch_target(self):
        outside=self.repo/'outside';outside.mkdir();(outside/'sentinel').write_bytes(b'keep')
        (self.run/'.receipt-attempts').symlink_to(outside,target_is_directory=True)
        with self.assertRaises(ValueError):self.publish()
        self.assertEqual(list(outside.iterdir()),[outside/'sentinel']);self.assertFalse(self.path.exists())
    def test_publication_requires_verified_namespace_ownership(self):
        with self.assertRaisesRegex(ValueError,'ownership'):c.publish_reference_receipt(self.path,self.value)
        self.assertFalse((self.run/'.receipt-attempts').exists())

if __name__=='__main__':unittest.main()
