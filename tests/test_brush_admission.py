import hashlib,os,subprocess,unittest
from pathlib import Path
class BrushAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_BRUSH_TEST_BIN']).resolve();cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def case(self,name):
  p=subprocess.run([str(self.binary),name],capture_output=True,text=True,timeout=5)
  self.assertEqual(p.returncode,0,p.stdout+p.stderr)
 def test_density(self):self.case('density')
 def test_velocity(self):self.case('velocity')
 def test_negative(self):self.case('negative')
 def test_edge(self):self.case('edge')
 def test_offscreen(self):self.case('offscreen')
 def test_extreme_int(self):self.case('extreme_int')
 def test_minimum(self):self.case('minimum')
 def test_nan_vx(self):self.case('nan_vx')
 def test_inf_vy(self):self.case('inf_vy')
 def test_scale_overflow(self):self.case('scale_overflow')
 def test_sum_overflow(self):self.case('sum_overflow')
 def test_bad_mode(self):self.case('bad_mode')
 def test_negative_mode(self):self.case('negative_mode')
 def test_window_zero(self):self.case('window_zero')
 def test_window_negative(self):self.case('window_negative')
 def test_grid_mismatch(self):self.case('grid_mismatch')
 def test_nan_density(self):self.case('nan_density')
 def test_inf_velx(self):self.case('inf_velx')
 def test_nan_vely(self):self.case('nan_vely')
 def test_null_sample(self):self.case('null_sample')
if __name__=='__main__':unittest.main()
