import hashlib,json,os,subprocess,tempfile,unittest
from pathlib import Path
class ShapeTextCompatibility(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_SHAPE_TEXT_TEST_BIN']).resolve();cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def check(self,text,valid=True):
  with tempfile.TemporaryDirectory(prefix='physics-shape-text-') as td:
   root=Path(td);source=root/'source';source.write_text(text);out1=root/'file';out2=root/'text'
   row=subprocess.run([str(self.binary),str(source),str(out1),str(out2)],capture_output=True,text=True,timeout=3)
   self.assertEqual(row.returncode,0,row.stderr)
   if valid:self.assertEqual(row.stdout,'loaded\n');self.assertEqual(out1.read_bytes(),out2.read_bytes())
   else:self.assertEqual(row.stdout,'rejected\n');self.assertFalse(out1.exists());self.assertFalse(out2.exists())
 def test_valid_line(self):self.check('{"version":1,"shapes":[{"name":"line","paths":[{"closed":false,"segments":[{"type":"line","p0":[-5,-5],"p1":[5,5]}]}]}]}')
 def test_valid_cubic(self):self.check('{"shapes":[{"name":"curve","paths":[{"segments":[{"type":"cubic","p0":[0,0],"p1":[1,1],"c1":[0,1],"c2":[1,0]}]}]}]}')
 def test_unicode_empty_shape(self):self.check(json.dumps({'version':1,'shapes':[{'name':'shape 🐕','paths':[]}]}))
 def test_legacy_unknown_type_behavior_remains(self):self.check('{"shapes":[{"paths":[{"segments":[{"type":"legacy","p0":[0,0],"p1":[1,1]}]}]}]}')
 def test_empty_document(self):self.check('{"shapes":[]}')
 def test_invalid_version(self):self.check('{"version":2,"shapes":[]}',False)
if __name__=='__main__':unittest.main()
