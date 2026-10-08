import hashlib,os,subprocess,unittest
from pathlib import Path
class MotionAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_MOTION_TEST_BIN']).resolve();cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def case(self,name):
  p=subprocess.run([str(self.binary),name],capture_output=True,text=True,timeout=5)
  self.assertEqual(p.returncode,0,p.stdout+p.stderr)
 def test_valid(self):self.case('valid')
 def test_minimum(self):self.case('minimum')
 def test_order(self):self.case('order')
 def test_extreme(self):self.case('extreme')
 def test_negative(self):self.case('negative')
 def test_offscreen(self):self.case('offscreen')
 def test_static_ignored(self):self.case('static_ignored')
 def test_locked_ignored(self):self.case('locked_ignored')
 def test_empty(self):self.case('empty')
 def test_all_static(self):self.case('all_static')
 def test_exact_count(self):self.case('exact_count')
 def test_negative_count(self):self.case('negative_count')
 def test_excess_count(self):self.case('excess_count')
 def test_capacity(self):self.case('capacity')
 def test_negative_capacity(self):self.case('negative_capacity')
 def test_null_objects(self):self.case('null_objects')
 def test_window_zero(self):self.case('window_zero')
 def test_window_negative(self):self.case('window_negative')
 def test_grid_mismatch(self):self.case('grid_mismatch')
 def test_allocation(self):self.case('allocation')
 def test_late_nan_position(self):self.case('late_nan_position')
 def test_late_inf_position(self):self.case('late_inf_position')
 def test_late_nan_velocity(self):self.case('late_nan_velocity')
 def test_late_inf_velocity(self):self.case('late_inf_velocity')
 def test_aggregate_overflow(self):self.case('aggregate_overflow')
 def test_intermediate_overflow(self):self.case('intermediate_overflow')
 def test_target_nan(self):self.case('target_nan')
 def test_target_inf(self):self.case('target_inf')
if __name__=='__main__':unittest.main()
