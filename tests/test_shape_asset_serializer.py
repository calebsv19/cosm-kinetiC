import hashlib,json,os,resource,signal,subprocess,tempfile,unittest
from pathlib import Path
class AssetSerializer(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.binary=Path(os.environ['PHYSICS_SIM_ASSET_SERIALIZER_TEST_BIN']).resolve();cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def run_case(self,mode):
  with tempfile.TemporaryDirectory(prefix='physics-asset-serializer-') as td:
   out=Path(td)/'asset.json';out.write_bytes(b'predecessor');row=subprocess.run([str(self.binary),mode,str(out)],capture_output=True,text=True,timeout=5);self.assertEqual(row.returncode,0,row.stdout+row.stderr)
   if mode=='compat':self.assertEqual(json.loads(out.read_text())['name'],'quoted "shape" 🐕')
   else:self.assertEqual(out.read_bytes(),b'predecessor')
   return row
 def test_legacy_file_reports_late_close_failure(self):
  with tempfile.TemporaryDirectory(prefix='physics-asset-close-') as td:
   out=Path(td)/'asset';out.write_bytes(b'predecessor')
   def constrain():signal.signal(signal.SIGXFSZ,signal.SIG_IGN);resource.setrlimit(resource.RLIMIT_FSIZE,(64,64))
   row=subprocess.run([str(self.binary),'file_failure',str(out)],capture_output=True,text=True,timeout=5,preexec_fn=constrain)
   self.assertEqual(row.returncode,0,row.stdout+row.stderr);self.assertEqual(out.stat().st_size,64)
 def test_file_text_byte_compatibility(self):self.run_case('compat')
 def test_every_reached_allocation_failure_refuses_complete_serialization(self):self.assertIn('allocation boundaries=',self.run_case('faults').stdout)
 def test_nonfinite_native_point_refuses_before_legacy_file_open(self):self.run_case('invalid')
if __name__=='__main__':unittest.main()
