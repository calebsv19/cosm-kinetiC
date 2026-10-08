"""Bounded ancestor package reservations hold cleanup before metadata reads."""
import hashlib,json,uuid
from pathlib import Path
import sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import check_clean_root as roots
import clean_outputs as clean
import build_outputs as outputs

class ReservationAdmission(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve();self.build=self.repo/'build/profiles/group/proof';self.build.mkdir(parents=True)
    def reservation(self,name,ancestor=None):
        directory=(ancestor or self.repo/'build/profiles/group')/'.package-reservations';directory.mkdir(exist_ok=True)
        output=directory.parent/'packages'/name;filename=hashlib.sha256(str(output).encode()).hexdigest()+('.json' if name.endswith('.json') else '.pending');path=directory/filename;path.write_text(json.dumps({'schema':'physics_sim_artifact_owner_v1','artifact_class':'local_package_staging','attempt_id':str(uuid.uuid4()),'state':'transaction_reserved','output':str(output)}));return path
    def check(self):roots.check(self.repo,self.build,[])
    def budget(self,name,value):
        with patch.object(roots,name,value,create=True),patch.object(roots,'read_json',wraps=roots.read_json) as read:
            with self.assertRaisesRegex(ValueError,'reservation.*bound|reservation.*budget'):self.check()
            self.assertEqual(read.call_count,0)
    def test_count_before_any_receipt_read(self):
        self.reservation('a.json');self.reservation('b.json');self.budget('RESERVATION_MAX_ENTRIES',1)
    def test_total_bytes_across_ancestors_before_reads(self):
        a=self.reservation('a.json');b=self.reservation('b.json',self.repo/'build/profiles');self.budget('RESERVATION_MAX_TOTAL_BYTES',a.stat().st_size+b.stat().st_size-1)
    def test_late_oversized_receipt_before_any_read(self):
        self.reservation('a.json');self.reservation('z.json').write_bytes(b' '*1048577)
        with patch.object(roots,'read_json',wraps=roots.read_json) as read:
            with self.assertRaises(ValueError):self.check()
            self.assertEqual(read.call_count,0)
    def test_non_json_reservation_entry_holds(self):
        p=self.reservation('attempt.pending')
        with self.assertRaisesRegex(ValueError,'reservation'):self.check()
        self.assertTrue(p.exists())
    def test_non_directory_reservation_root_holds(self):
        p=self.repo/'build/.package-reservations';p.write_bytes(b'valuable')
        with self.assertRaisesRegex(ValueError,'reservation'):self.check()
        self.assertEqual(p.read_bytes(),b'valuable')
    def test_same_byte_rewrite_to_consumed_receipt_holds(self):
        first=self.reservation('a.json');self.reservation('b.json');original=roots.read_json;calls=0
        def change(path,*args):
            nonlocal calls
            row=original(path,*args);calls+=1
            if calls==2:first.write_bytes(first.read_bytes())
            return row
        with patch.object(roots,'read_json',side_effect=change):
            with self.assertRaisesRegex(ValueError,'changed'):self.check()
    def test_new_reservation_during_read_holds(self):
        self.reservation('a.json');original=roots.read_json
        def change(path,*args):
            row=original(path,*args);self.reservation('new.json');return row
        with patch.object(roots,'read_json',side_effect=change):
            with self.assertRaisesRegex(ValueError,'changed'):self.check()
    def test_previously_absent_ancestor_directory_creation_holds(self):
        self.reservation('a.json');original=roots.read_json
        def change(path,*args):
            row=original(path,*args);self.reservation('new.json',self.repo/'build/profiles');return row
        with patch.object(roots,'read_json',side_effect=change):
            with self.assertRaisesRegex(ValueError,'changed'):self.check()
    def test_inclusive_bounds_allow_unrelated_and_overlap_still_holds(self):
        p=self.reservation('a.json')
        with patch.object(roots,'RESERVATION_MAX_ENTRIES',1,create=True),patch.object(roots,'RESERVATION_MAX_TOTAL_BYTES',p.stat().st_size,create=True):self.check()
        row=json.loads(p.read_text());row['output']=str(self.build);p.unlink();p=p.parent/(hashlib.sha256(str(self.build).encode()).hexdigest()+'.json');p.write_text(json.dumps(row))
        with self.assertRaisesRegex(ValueError,'overlaps reserved'):self.check()
    def test_sampled_time_before_receipt_reads(self):
        self.reservation('a.json')
        with patch.object(roots,'RESERVATION_MAX_SECONDS',-1,create=True),patch.object(roots,'read_json',wraps=roots.read_json) as read:
            with self.assertRaisesRegex(ValueError,'reservation.*bound|reservation.*budget'):self.check()
            self.assertEqual(read.call_count,0)

    def test_reservation_change_during_output_hash_holds_plan(self):
        receipt=self.reservation('a.json');output=self.build/'object.o';output.write_bytes(b'abc');outputs.record(self.repo,output,'compiler');original=outputs.hashlib.file_digest
        def change(stream,*args,**kwargs):
            value=original(stream,*args,**kwargs);receipt.write_bytes(receipt.read_bytes());return value
        with patch.object(outputs.hashlib,'file_digest',side_effect=change):
            with self.assertRaisesRegex(ValueError,'changed'):clean.plan(self.repo,self.build,[],[])
        self.assertEqual(output.read_bytes(),b'abc')

if __name__=='__main__':unittest.main()
