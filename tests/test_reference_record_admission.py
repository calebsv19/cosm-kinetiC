"""Reject malformed, escaping and over-limit retained reference records."""
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

class Records(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name).resolve();self.command=['owned-command'];self.hashes={'source':'a'*64}
        self.artifact=self.root/'artifact';self.artifact.write_bytes(b'known bytes')
        self.record={'command':self.command,'source_sha256':self.hashes,'returncode':0,'stop_reason':None,
                     'supervision':{'all_external_descendants_verified_terminal':False},
                     'artifact_sha256':{str(self.artifact):hashlib.sha256(b'known bytes').hexdigest()}}
        self.path=self.root/'receipt.json'
    def write(self):self.path.write_text(json.dumps(self.record))
    def read(self):return c.reference_receipt(self.path,self.root,self.command,self.hashes,expected_paths=(self.artifact,))
    def test_matching_receipt_and_artifact_readback_does_not_mutate(self):
        self.write();before=self.path.read_bytes();self.assertEqual(self.read(),self.record);self.assertEqual(self.path.read_bytes(),before)
        self.artifact.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.read()
    def test_receipt_source_command_and_outcome_types_are_held(self):
        for key,value in (('command',['other']),('source_sha256',{}),('returncode',True),('returncode',256),('stop_reason',{}),('artifact_sha256',{})):
            with self.subTest(key=key,value=value):
                original=self.record[key];self.record[key]=value;self.write()
                with self.assertRaises(ValueError):self.read()
                self.record[key]=original
    def test_escaping_artifact_is_refused_before_read(self):
        self.record['artifact_sha256']={str(self.root.parent/'outside'):'a'*64};self.write()
        with patch.object(c,'factor_digest',side_effect=AssertionError('outside artifact read')):
            with self.assertRaises(ValueError):self.read()
    def test_link_and_special_artifacts_are_held(self):
        link=self.root/'link';link.symlink_to(self.artifact)
        fifo=self.root/'fifo';os.mkfifo(fifo)
        for path in (link,fifo):
            self.record['artifact_sha256']={str(path):'a'*64};self.write()
            with self.assertRaises(ValueError):self.read()
    def test_expected_inventory_cannot_omit_or_add_artifacts(self):
        other=self.root/'other';other.write_bytes(b'other')
        self.write()
        with self.assertRaises(ValueError):c.reference_receipt(self.path,self.root,self.command,self.hashes,expected_paths=(self.artifact,other))
        self.record['artifact_sha256'][str(other)]=hashlib.sha256(b'other').hexdigest();self.write()
        with self.assertRaises(ValueError):self.read()
    def test_success_missing_outputs_is_retained_as_failure_and_failed_partial_is_readable(self):
        missing=self.root/'missing'
        c.complete_reference_receipt(self.record,self.root,(self.artifact,missing))
        self.assertEqual(self.record['diagnostic_failure']['kind'],'missing_required_artifacts')
        self.write();before=self.path.read_bytes()
        c.reference_receipt(self.path,self.root,self.command,self.hashes,expected_paths=(self.artifact,missing))
        self.assertEqual(self.path.read_bytes(),before)
        del self.record['diagnostic_failure'];self.write()
        with self.assertRaises(ValueError):c.reference_receipt(self.path,self.root,self.command,self.hashes,expected_paths=(self.artifact,missing))
        self.record['returncode']=2;self.write()
        c.reference_receipt(self.path,self.root,self.command,self.hashes,expected_paths=(self.artifact,missing))
    def test_aggregate_hash_budget_holds_before_any_artifact_read(self):
        other=self.root/'other';other.write_bytes(b'other')
        with patch.object(c,'factor_digest',side_effect=AssertionError('artifact content read')):
            with self.assertRaisesRegex(ValueError,'aggregate'):c.reference_artifact_hashes(self.root,(self.artifact,other),byte_cap=4)
        before={p:p.read_bytes() for p in (self.artifact,other)}
        result=c.reference_artifact_hashes(self.root,(self.artifact,other),byte_cap=sum(map(len,before.values())))
        self.assertEqual(result,{str(p):hashlib.sha256(data).hexdigest() for p,data in before.items()})
        self.assertEqual(before,{p:p.read_bytes() for p in before})
    def test_sparse_default_budget_and_late_special_input_hold_before_read(self):
        huge=[]
        for number in range(3):
            p=self.root/('huge'+str(number));huge.append(p)
            with p.open('wb') as f:f.truncate(8589934592)
        with patch.object(c,'factor_digest',side_effect=AssertionError('sparse content read')):
            with self.assertRaisesRegex(ValueError,'aggregate'):c.reference_artifact_hashes(self.root,huge)
        fifo=self.root/'z-fifo';os.mkfifo(fifo)
        with patch.object(c,'factor_digest',side_effect=AssertionError('earlier content read')):
            with self.assertRaises(ValueError):c.reference_artifact_hashes(self.root,(self.artifact,fifo))
    def test_hash_pass_holds_growth_and_changes_to_previously_read_files(self):
        other=self.root/'other';other.write_bytes(b'other')
        original=c.factor_digest
        def changed(name,limit):
            result=original(name,limit)
            if Path(name)==other:self.artifact.write_bytes(b'changed')
            return result
        with patch.object(c,'factor_digest',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'inventory changed'):c.reference_artifact_hashes(self.root,(self.artifact,other))
        self.artifact.write_bytes(b'known bytes')
        def grown(name,limit):
            Path(name).write_bytes(b'x'*(limit+1));return original(name,limit)
        with patch.object(c,'factor_digest',side_effect=grown):
            with self.assertRaises(ValueError):c.reference_artifact_hashes(self.root,(self.artifact,))
    def test_json_duplicates_overflow_depth_and_structure_are_held(self):
        for data in (b'{"a":1,"a":2}',b'{"a":NaN}',b'{"a":1e999}',b'{"a":'+b'['*65+b'0'+b']'*65+b'}'):
            with self.subTest(data=data[:30]),self.assertRaises(ValueError):c.admitted_json(data)
        with self.assertRaises(ValueError):c.admitted_json(b'{"a":[1,2,3]}',event_cap=2)
        self.assertEqual(c.admitted_json(b'{"quoted":"[\\\"{]"}')[0],{'quoted':'["{]'})
    def test_streamed_progress_preserves_diagnostics_and_rejects_bad_json(self):
        p=self.root/'progress.log';p.write_text('ordinary diagnostic\n{"phase":"solve","iteration":2}\n')
        self.assertEqual(c.reference_progress(p),[{'phase':'solve','iteration':2}])
        for data in ('{"phase":', '{"phase":"a","phase":"b"}', '{"phase":1e999}', 'x'*65537):
            p.write_text(data)
            with self.assertRaises(ValueError):c.reference_progress(p)
    def test_bounded_regular_json_and_hash_inputs_are_held(self):
        self.path.write_bytes(b'x'*65)
        with self.assertRaises(ValueError):c.factor_json(self.path,64)
        with self.assertRaises(ValueError):c.factor_digest(self.artifact,4)
        self.assertEqual(self.artifact.read_bytes(),b'known bytes')

if __name__=='__main__':unittest.main()
