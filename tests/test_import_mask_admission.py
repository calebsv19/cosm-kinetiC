import hashlib, os, subprocess, tempfile, unittest
from pathlib import Path
class ImportMaskAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_IMPORT_MASK_TEST_BIN']).resolve()
  cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
  cls.repo=Path(__file__).resolve().parents[1]
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def case(self,mode,*args):
  row=subprocess.run([str(self.binary),mode,*map(str,args)],capture_output=True,text=True,timeout=5)
  self.assertEqual(row.returncode,0,row.stdout+row.stderr)
 def test_valid(self):self.case('valid')
 def test_short(self):self.case('short')
 def test_long(self):self.case('long')
 def test_zero(self):self.case('zero')
 def test_size_max(self):self.case('size_max')
 def test_grid_large(self):self.case('grid_large')
 def test_grid_int(self):self.case('grid_int')
 def test_grid_zero(self):self.case('grid_zero')
 def test_grid_negative(self):self.case('grid_negative')
 def test_grid_one(self):self.case('grid_one')
 def test_path_unterminated(self):self.case('path_unterminated')
 def test_path_empty(self):self.case('path_empty')
 def test_library_missing(self):self.case('library_missing')
 def test_library_count(self):self.case('library_count')
 def test_missing(self):self.case('missing')
 def test_asset_refused(self):self.case('asset_refused')
 def test_empty_document(self):self.case('empty_document')
 def test_nonfinite_transforms(self):
  for i in range(4):
   with self.subTest(field=i):self.case('nonfinite_'+str(i))
 def test_raw_parity(self):self.case('raw',self.repo/'import/u_shape.json')
 def test_partial_raster_preserves_output(self):self.case('partial_raster',self.repo/'import/u_shape.json')
 def test_allocation_failure_preserves_output(self):self.case('allocation',self.repo/'import/u_shape.json')
 def test_malformed_source_preserves_output(self):
  with tempfile.TemporaryDirectory(prefix='physics-import-mask-',dir='/private/tmp') as root:
   path=Path(root)/'bad.json';path.write_text('{')
   self.case('malformed',path)
if __name__=='__main__':unittest.main()
