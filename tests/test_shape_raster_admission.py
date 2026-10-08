import hashlib,os,subprocess,unittest
from pathlib import Path
class ShapeRasterAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_SHAPE_RASTER_TEST_BIN']).resolve();cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def run_case(self,name):
  row=subprocess.run([str(self.binary),name],capture_output=True,text=True,timeout=3)
  self.assertEqual(row.returncode,0,row.stdout+row.stderr)
 def test_scale_conversion(self):self.run_case('scale')
 def test_stroke_conversion(self):self.run_case('stroke')
 def test_rotation_overflow(self):self.run_case('rotation')
 def test_nonfinite_options(self):self.run_case('nan')
 def test_line_work_budget(self):self.run_case('line_work')
 def test_cubic_preflatten_conversion(self):self.run_case('cubic')
 def test_cubic_offset_roundoff(self):self.run_case('cubic_offset')
 def test_nonfinite_source(self):self.run_case('source_nan')
 def test_mask_capacity_admission(self):self.run_case('grid')
 def test_finite_long_geometry(self):self.run_case('fill_work')
 def test_aggregate_segment_budget(self):self.run_case('aggregate')
 def test_path_inventory_budget(self):self.run_case('paths')
 def test_point_inventory_budget(self):self.run_case('points')
 def test_ordinary_raster(self):self.run_case('valid')
 def test_native_finite_default_fallbacks(self):self.run_case('fallback')
if __name__=='__main__':unittest.main()
