import hashlib,os,subprocess,unittest
from pathlib import Path
class BackendStorageAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_BACKEND_STORAGE_TEST_BIN']).resolve()
  cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def case(self,mode,*args):
  p=subprocess.run([str(self.binary),mode,*map(str,args)],capture_output=True,text=True,timeout=5)
  self.assertEqual(p.returncode,0,p.stdout+p.stderr)
 def test_config_snapshot(self):self.case('config_snapshot')
 def test_valid(self):self.case('valid')
 def test_minimum(self):self.case('minimum')
 def test_rectangular(self):self.case('rectangular')
 def test_seeded(self):self.case('seeded')
 def test_zero(self):self.case('zero')
 def test_negative(self):self.case('negative')
 def test_one(self):self.case('one')
 def test_large(self):self.case('large')
 def test_int(self):self.case('int')
 def test_null(self):self.case('null')
 def test_plan(self):self.case('plan')
 def test_allocation_failure_01(self):self.case("fault",1)
 def test_allocation_failure_02(self):self.case("fault",2)
 def test_allocation_failure_03(self):self.case("fault",3)
 def test_allocation_failure_04(self):self.case("fault",4)
 def test_allocation_failure_05(self):self.case("fault",5)
 def test_allocation_failure_06(self):self.case("fault",6)
 def test_allocation_failure_07(self):self.case("fault",7)
 def test_allocation_failure_08(self):self.case("fault",8)
 def test_allocation_failure_09(self):self.case("fault",9)
 def test_allocation_failure_10(self):self.case("fault",10)
 def test_allocation_failure_11(self):self.case("fault",11)
 def test_allocation_failure_12(self):self.case("fault",12)
 def test_allocation_failure_13(self):self.case("fault",13)
 def test_allocation_failure_14(self):self.case("fault",14)
 def test_allocation_failure_15(self):self.case("fault",15)
if __name__=='__main__':unittest.main()
