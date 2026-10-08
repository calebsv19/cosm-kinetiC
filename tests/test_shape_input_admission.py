import hashlib,json,os,subprocess,tempfile,unittest
from pathlib import Path
class ShapeInputAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.bins=[Path(os.environ[k]).resolve() for k in ('PHYSICS_SIM_SHAPE_MASK_TEST_BIN','PHYSICS_SIM_SHAPE_ASSET_TEST_BIN')]
  cls.repo=Path(__file__).resolve().parents[1];cls.source=cls.repo/'import/u_shape.json';cls.good=cls.source.read_bytes();cls.digests={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in cls.bins+[cls.source]}
 @classmethod
 def tearDownClass(cls):
  for p,d in cls.digests.items():
   if hashlib.sha256(p.read_bytes()).hexdigest()!=d:raise AssertionError(f'Changed input {p}')
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='physics-shape-input-',dir='/private/tmp');self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.path=self.root/'input.json'
 def invoke(self,tool):
  return subprocess.run([str(self.bins[tool]),*(['--grid','32','32'] if tool==0 else []),'--out',str(self.root/f'out-{tool}'),str(self.path)],capture_output=True,text=True,timeout=3)
 def refused(self):
  for tool in (0,1):
   with self.subTest(tool=tool):
    out=self.root/f'out-{tool}';out.write_bytes(b'predecessor');row=self.invoke(tool);self.assertEqual(row.returncode,1,row.stdout+row.stderr);self.assertIn('Failed to load',row.stderr);self.assertEqual(out.read_bytes(),b'predecessor')
 def test_symlink_input(self):
  target=self.root/'real';target.write_bytes(self.good);self.path.symlink_to(target);self.refused();self.assertEqual(target.read_bytes(),self.good)
 def test_linked_ancestor(self):
  target=self.root/'real';target.mkdir();(target/'input').write_bytes(self.good);link=self.root/'link';link.symlink_to(target,target_is_directory=True);self.path=link/'input';self.refused()
 def test_hardlinked_input(self):
  target=self.root/'real';target.write_bytes(self.good);os.link(target,self.path);self.refused()
 def test_fifo_input(self):os.mkfifo(self.path);self.refused()
 def test_oversized_input(self):
  self.path.write_bytes(self.good)
  with self.path.open('r+b') as f:f.truncate(16*1024*1024+1)
  self.refused()
 def test_trailing_and_embedded_data(self):
  for tail in (b'junk',b'{}',b'\x00hidden'):
   with self.subTest(tail=tail):self.path.write_bytes(self.good+tail);self.refused()
 def test_duplicate_and_case_variant_consumed_keys(self):
  text=self.good.decode()
  for text2 in [text.replace('"version":','"version":1,"version":',1),text.replace('"version":','"VERSION":1,"version":',1)]:
   with self.subTest(text=text2[:60]):self.path.write_text(text2);self.refused()
 def test_semantic_types_and_coordinate_representation(self):
  for mutation in ('version','closed','segment','coordinate','coordinate_overflow'):
   data=json.loads(self.good);seg=data['shapes'][0]['paths'][0]['segments'][0]
   if mutation=='version':data['version']=1.9
   elif mutation=='closed':data['shapes'][0]['paths'][0]['closed']='true'
   elif mutation=='segment':seg['type']='mystery'
   elif mutation=='coordinate':seg['p0'][0]='5'
   else:seg['p0'][0]=1e300
   with self.subTest(mutation=mutation):self.path.write_text(json.dumps(data));self.refused()
 def test_depth_and_utf8(self):
  for extra in [b'{"unused":'+b'['*70+b'0'+b']'*70+b',"shapes":[]}',self.good.replace(b'config/u_shape.json',b'\xff')]:
   with self.subTest(data=extra[:20]):self.path.write_bytes(extra);self.refused()
 def test_consumed_case_aliases_below_root(self):
  for level in ('shape','path','segment'):
   data=json.loads(self.good);shape=data['shapes'][0];path=shape['paths'][0];segment=path['segments'][0]
   if level=='shape':shape['NAME']='alias'
   elif level=='path':path['CLOSED']=True
   else:segment['TYPE']='cubic'
   with self.subTest(level=level):self.path.write_text(json.dumps(data));self.refused()
 def test_geometry_admission_before_asset_flatten(self):
  for kind in ('curve','paths','shapes'):
   data=json.loads(self.good);shape=data['shapes'][0]
   if kind=='curve':
    segment=shape['paths'][0]['segments'][0];segment['type']='cubic';segment['c1']=[1e17,1e17];segment['c2']=[-1e17,1e17]
   elif kind=='paths':shape['paths']=[{'segments':[]} for _ in range(10001)]
   else:data['shapes']=[{'paths':[]} for _ in range(1025)]
   with self.subTest(kind=kind):self.path.write_text(json.dumps(data));self.refused()
 def test_valid_source_file_and_missing_optional_version(self):
  data=json.loads(self.good);del data['version'];self.path.write_text(json.dumps(data))
  for tool in (0,1):
   with self.subTest(tool=tool):row=self.invoke(tool);self.assertEqual(row.returncode,0,row.stderr);self.assertTrue((self.root/f'out-{tool}').is_file())
if __name__=='__main__':unittest.main()
