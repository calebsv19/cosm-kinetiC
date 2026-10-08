"""Reservation schemas bind declared output paths before cleanup eligibility."""
import hashlib,json,sys,tempfile,unittest,uuid
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import check_clean_root as roots
import package_outputs as packages

class ReservationSchema(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.repo=Path(self.t.name).resolve();self.root=self.repo/'build/packages';self.build=self.root/'compiler-proof';self.build.mkdir(parents=True)
        self.output=self.root/'worker';self.directory=self.root/'.package-reservations';self.directory.mkdir()
        self.receipt=self.directory/(hashlib.sha256(str(self.output).encode()).hexdigest()+'.json')
        self.row={'schema':'physics_sim_artifact_owner_v1','artifact_class':'local_package_staging','attempt_id':str(uuid.uuid4()),'output':str(self.output),'state':'transaction_reserved'}
    def check(self,row):
        self.receipt.write_text(json.dumps(row));return roots.check(self.repo,self.build,[])
    def test_missing_wrong_and_extra_ownership_fields_hold(self):
        variants=[]
        for key in self.row:
            row=dict(self.row);row.pop(key);variants.append(row)
        for key,value in [('schema','foreign'),('artifact_class','disposable'),('state','done'),('attempt_id',''),('attempt_id',True),('attempt_id','../../outside'),('attempt_id','A'*36),('unexpected',True)]:
            row=dict(self.row);row[key]=value;variants.append(row)
        for row in variants:
            with self.subTest(row=row),self.assertRaisesRegex(ValueError,'reservation'):self.check(row)
    def test_output_paths_must_be_canonical_absolute_contained_and_name_bound(self):
        alias=self.root/'alias';alias.symlink_to(self.root/'target')
        for output in ['relative/worker',str(self.repo/'elsewhere/worker'),str(self.root/'missing/../worker'),str(self.root/'alias/worker'),str(self.output)+'\x00',str(self.root/'different')]:
            row=dict(self.row,output=output)
            with self.subTest(output=output),self.assertRaisesRegex(ValueError,'reservation'):self.check(row)
    def test_fresh_reservation_requires_exact_root_and_false_authority(self):
        base=dict(self.row,state='fresh_reserved_attempt',root=str(self.root),release_authority_granted=False)
        self.check(base)
        for key,value in [('root',str(self.repo/'build')),('root',True),('release_authority_granted',True),('release_authority_granted',0)]:
            row=dict(base);row[key]=value
            with self.subTest(key=key,value=value),self.assertRaisesRegex(ValueError,'reservation'):self.check(row)
        for key in ('root','release_authority_granted'):
            row=dict(base);row.pop(key)
            with self.assertRaisesRegex(ValueError,'reservation'):self.check(row)
    def test_actual_fresh_producer_admits_unrelated_and_protects_selected(self):
        self.receipt.unlink(missing_ok=True);validate=lambda:packages.plan(self.repo,self.root,[self.output],[]);packages.declare(validate(),validate)
        roots.check(self.repo,self.build,[])
        with self.assertRaisesRegex(ValueError,'overlaps reserved'):roots.check(self.repo,self.output,[])
    def test_transaction_schema_admits_unrelated_and_protects_selected(self):
        self.check(self.row)
        with self.assertRaisesRegex(ValueError,'overlaps reserved'):roots.check(self.repo,self.output,[])

if __name__=='__main__':unittest.main()
