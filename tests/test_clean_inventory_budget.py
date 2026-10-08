"""Whole-tree cleanup preflight bounds precede output hashing."""
import hashlib
from pathlib import Path
import sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import build_outputs as outputs
import check_clean_root as roots
import clean_outputs as clean

class CleanInventoryBudget(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve();self.build=self.repo/'build/profile';self.build.mkdir(parents=True)
    def output(self,name,data=b'abc'):
        p=self.build/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);outputs.record(self.repo,p,'compiler');return p
    def held_before_hash(self,name,limit):
        with patch.object(roots,name,limit,create=True),patch.object(outputs.hashlib,'file_digest',wraps=hashlib.file_digest) as digest:
            with self.assertRaisesRegex(ValueError,'bound|budget'):outputs.inventory(self.repo,self.build)
            self.assertEqual(digest.call_count,0)
    def test_aggregate_bytes_hold_before_any_output_hash(self):
        self.output('a');self.output('b');self.held_before_hash('BUILD_MAX_TOTAL_BYTES',5)
    def test_late_oversized_output_holds_before_any_output_hash(self):
        self.output('a');self.output('z',b'123456789');self.held_before_hash('BUILD_MAX_FILE_BYTES',8)
    def test_entry_count_holds_before_any_output_hash(self):
        for name in ('a','b','c'):self.output(name)
        self.held_before_hash('BUILD_MAX_ENTRIES',2)
    def test_tree_depth_holds_before_any_output_hash(self):
        self.output('a/b/file');self.held_before_hash('BUILD_MAX_DEPTH',2)
    def test_metadata_budget_includes_ownership_receipts_before_reads(self):
        self.output('a')
        with patch.object(roots,'BUILD_MAX_METADATA_BYTES',1,create=True),patch.object(outputs,'read_json',wraps=outputs.read_json) as read:
            with self.assertRaisesRegex(ValueError,'metadata.*bound|metadata.*budget'):outputs.inventory(self.repo,self.build)
            self.assertEqual(read.call_count,0)
    def test_inclusive_boundaries_and_ordinary_inventory(self):
        self.output('a');self.output('b')
        with patch.object(roots,'BUILD_MAX_ENTRIES',2,create=True),patch.object(roots,'BUILD_MAX_TOTAL_BYTES',6,create=True),patch.object(roots,'BUILD_MAX_FILE_BYTES',3,create=True),patch.object(roots,'BUILD_MAX_DEPTH',1,create=True):
            self.assertEqual(len(outputs.inventory(self.repo,self.build)),2)
    def test_change_to_already_hashed_file_holds_entire_inventory(self):
        first=self.output('a');self.output('b');original=outputs.hashlib.file_digest;calls=0
        def changed(stream,*args,**kwargs):
            nonlocal calls
            value=original(stream,*args,**kwargs);calls+=1
            if calls==2:first.write_bytes(b'new')
            return value
        with patch.object(outputs.hashlib,'file_digest',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'changed'):outputs.inventory(self.repo,self.build)
    def test_growth_during_hash_does_not_stream_beyond_original_size(self):
        p=self.output('a');original=outputs.hashlib.file_digest;seen=0
        class Tracked:
            def __init__(self,stream):self.stream=stream
            def readable(self):return True
            def readinto(self,buffer):
                nonlocal seen
                n=self.stream.readinto(buffer);seen+=n;return n
        def growth(stream,*args,**kwargs):
            with p.open('ab') as file:file.write(b'def')
            return original(Tracked(stream),*args,**kwargs)
        with patch.object(outputs.hashlib,'file_digest',side_effect=growth):
            with self.assertRaises(ValueError):outputs.verified(self.repo,p)
        self.assertLessEqual(seen,4)

    def test_root_executable_and_build_share_total_budget(self):
        self.output('a');exe=self.repo/'physics_sim_headless';exe.write_bytes(b'abc');outputs.record(self.repo,exe,'compiler')
        with patch.object(roots,'BUILD_MAX_TOTAL_BYTES',5),patch.object(outputs.hashlib,'file_digest',wraps=hashlib.file_digest) as digest:
            with self.assertRaisesRegex(ValueError,'bound|budget'):clean.plan(self.repo,self.build,[],[exe])
            self.assertEqual(digest.call_count,0)
    def test_new_entry_during_hash_holds_entire_inventory(self):
        self.output('a');original=outputs.hashlib.file_digest
        def changed(stream,*args,**kwargs):
            value=original(stream,*args,**kwargs);(self.build/'new').write_bytes(b'new');return value
        with patch.object(outputs.hashlib,'file_digest',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'changed'):outputs.inventory(self.repo,self.build)
    def test_already_read_receipt_rewrite_holds_entire_inventory(self):
        first=self.output('a');self.output('b');receipt=outputs.receipt_path(self.repo,first);original=outputs.hashlib.file_digest;calls=0
        def changed(stream,*args,**kwargs):
            nonlocal calls
            value=original(stream,*args,**kwargs);calls+=1
            if calls==2:receipt.write_bytes(receipt.read_bytes())
            return value
        with patch.object(outputs.hashlib,'file_digest',side_effect=changed):
            with self.assertRaisesRegex(ValueError,'changed'):outputs.inventory(self.repo,self.build)
    def test_elapsed_budget_holds_before_hash(self):
        self.output('a')
        with patch.object(roots.time,'monotonic',side_effect=[0,0,121]),patch.object(outputs.hashlib,'file_digest',wraps=hashlib.file_digest) as digest:
            with self.assertRaisesRegex(ValueError,'bound|budget'):outputs.inventory(self.repo,self.build)
            self.assertEqual(digest.call_count,0)

if __name__=='__main__':unittest.main()
