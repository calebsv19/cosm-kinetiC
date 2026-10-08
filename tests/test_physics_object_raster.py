import hashlib, os, subprocess, unittest
from pathlib import Path
class PhysicsObjectRaster(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_OBJECT_RASTER_TEST_BIN']).resolve()
  cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def case(self,name,*args):
  row=subprocess.run([str(self.binary),name,*args],capture_output=True,text=True,timeout=5)
  self.assertEqual(row.returncode,0,row.stdout+row.stderr)
 def test_valid(self):self.case('valid')
 def test_closed(self):self.case('closed')
 def test_fit(self):self.case('fit')
 def test_fallback(self):self.case('fallback')
 def test_scale(self):self.case('scale')
 def test_stroke(self):self.case('stroke')
 def test_rotation(self):self.case('rotation')
 def test_coordinate(self):self.case('coordinate')
 def test_source_nan(self):self.case('source_nan')
 def test_grid(self):self.case('grid')
 def test_grid_int(self):self.case('grid_int')
 def test_grid_zero(self):self.case('grid_zero')
 def test_paths(self):self.case('paths')
 def test_points(self):self.case('points')
 def test_missing_paths(self):self.case('missing_paths')
 def test_missing_points(self):self.case('missing_points')
 def test_schema(self):self.case('schema')
 def test_empty(self):self.case('empty')
 def test_line_work(self):self.case('line_work')
 def test_aggregate(self):self.case('aggregate')
 def test_fill_work(self):self.case('fill_work')
 def test_allocation(self):self.case('allocation')
 def test_exact_grid(self):self.case('exact_grid')
 def test_owned_output(self):self.case('owned_output')
 def test_nonfinite_options(self):
  for i in range(7):
   with self.subTest(field=i):self.case('opts_nan_'+str(i))
 def test_nonfinite_base(self):
  for i in range(5):
   with self.subTest(field=i):self.case('base_nan_'+str(i))
 def test_checked_in_asset_masks(self):
  files=sorted((Path(__file__).resolve().parents[1]/'config/objects').glob('*.json'))
  self.assertEqual(len(files),8)
  before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
  for path in files:
   with self.subTest(asset=path.name):self.case('file',str(path))
  self.assertEqual(before,{p:hashlib.sha256(p.read_bytes()).hexdigest() for p in files})
if __name__=='__main__':unittest.main()
