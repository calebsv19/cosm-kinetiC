"""Named file and unique-link binding for cleanup inputs."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_outputs
import check_clean_root
from clean_outputs import plan

class OutputReadAdmission(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup)
        self.repo=Path(self.t.name).resolve();self.build=self.repo/'build/profile';self.build.mkdir(parents=True)
    def test_hardlinked_output_cannot_be_recorded_or_admitted_for_cleanup(self):
        output=self.build/'fixture';output.write_bytes(b'valuable shared file');alias=self.repo/'outside';os.link(output,alias)
        with self.assertRaisesRegex(ValueError,'regular|link'):
            build_outputs.record(self.repo,output,'compiler')
        self.assertFalse(build_outputs.receipt_path(self.repo,output).exists())
        self.assertEqual(alias.read_bytes(),b'valuable shared file')
    def test_named_output_swap_during_hash_is_rejected_even_with_identical_bytes(self):
        output=self.build/'fixture';output.write_bytes(b'owned');build_outputs.record(self.repo,output,'compiler')
        original=build_outputs.hashlib.file_digest
        def swap(stream,*args,**kwargs):
            digest=original(stream,*args,**kwargs)
            moved=self.repo/'saved';output.rename(moved);output.write_bytes(b'owned')
            return digest
        with patch.object(build_outputs.hashlib,'file_digest',side_effect=swap):
            with self.assertRaisesRegex(ValueError,'changed|named'):
                build_outputs.verified(self.repo,output)
        self.assertEqual(output.read_bytes(),b'owned');self.assertEqual((self.repo/'saved').read_bytes(),b'owned')
    def test_replaced_receipt_during_decode_holds_whole_cleanup_plan(self):
        output=self.build/'fixture';output.write_bytes(b'owned');build_outputs.record(self.repo,output,'compiler')
        receipt=build_outputs.receipt_path(self.repo,output);original=check_clean_root.json.loads
        def swap(content,*args,**kwargs):
            row=original(content,*args,**kwargs)
            receipt.rename(self.repo/'saved-receipt');receipt.write_text(content)
            return row
        with patch.object(check_clean_root.json,'loads',side_effect=swap):
            with self.assertRaisesRegex(ValueError,'changed|named'):
                plan(self.repo,self.build,[self.repo/'data'],[])
        self.assertEqual(output.read_bytes(),b'owned')
    def test_hardlinked_json_is_not_admitted_as_owned_metadata(self):
        path=self.build/'meta.json';path.write_text('{"value":1}');os.link(path,self.repo/'alias')
        with self.assertRaisesRegex(ValueError,'regular|link'):
            check_clean_root.read_json(path,1024)
    def test_json_content_change_during_decode_is_held(self):
        path=self.build/'meta.json';path.write_text('{"value":1}');original=check_clean_root.json.loads
        def change(content,*args,**kwargs):
            row=original(content,*args,**kwargs);path.write_text('{"value":2}');return row
        with patch.object(check_clean_root.json,'loads',side_effect=change):
            with self.assertRaisesRegex(ValueError,'changed'):
                check_clean_root.read_json(path,1024)
    def test_ordinary_output_and_metadata_remain_admitted(self):
        output=self.build/'fixture';output.write_bytes(b'owned');build_outputs.record(self.repo,output,'compiler')
        self.assertEqual(build_outputs.verified(self.repo,output)['path'],str(output))
        path=self.repo/'meta.json';path.write_text('{"value":1}');self.assertEqual(check_clean_root.read_json(path,1024),{'value':1})
        self.assertEqual(len(plan(self.repo,self.build,[self.repo/'data'],[])['owned_files']),1)

if __name__=='__main__':unittest.main()
