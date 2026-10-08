"""Actual host asset admission; explicit native binary required, no skipped gate."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path
class AssetInput(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.binary=Path(os.environ['PHYSICS_SIM_ASSET_INPUT_TEST_BIN']).resolve()
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='physics-asset-input-',dir='/private/tmp');self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name);self.source=self.root/'input.json';self.good={'schema':1,'name':'unicode-λ','paths':[{'closed':True,'points':[{'x':1.25,'y':-2},{'x':3,'y':4}]}]};self.source.write_text(json.dumps(self.good))
 def invoke(self,path=None):return subprocess.run([str(self.binary),str(path or self.source)],capture_output=True,text=True,timeout=3)
 def refuse(self,path=None):
  row=self.invoke(path);self.assertEqual(row.returncode,1,row.stdout+row.stderr);self.assertEqual(row.stdout.strip(),'0 0')
 def test_ordinary_asset(self):
  row=self.invoke();self.assertEqual(row.returncode,0,row.stderr);self.assertEqual(row.stdout.strip(),'1 1')
 def test_fifo_refuses_without_blocking(self):
  self.source.unlink();os.mkfifo(self.source);self.refuse()
 def test_symlink_refuses(self):
  link=self.root/'link';link.symlink_to(self.source);self.refuse(link)
 def test_hardlink_refuses(self):
  link=self.root/'link';os.link(self.source,link);self.refuse(link)
 def test_linked_ancestor_refuses(self):
  real=self.root/'real';real.mkdir();(real/'input.json').write_bytes(self.source.read_bytes());link=self.root/'linked';link.symlink_to(real,target_is_directory=True);self.refuse(link/'input.json')
 def test_directory_refuses(self):self.refuse(self.root)
 def test_missing_refuses(self):self.refuse(self.root/'absent')
 def test_oversize_refuses(self):
  with self.source.open('r+b') as file:file.truncate(16*1024*1024+1)
  self.refuse()
 def test_trailing_and_nul_refuse(self):
  for suffix in [b' trailing',b' {}',b'\x00extra']:
   with self.subTest(suffix=suffix):self.source.write_bytes(json.dumps(self.good).encode()+suffix);self.refuse()
 def test_duplicate_decoded_keys_refuse(self):
  for text in ['{"schema":1,"schema":1,"paths":[]}','{"paths":[],"pa\\u0074hs":[]}','{"paths":[{"points":[{"x":1,"x":2,"y":0}]}]}']:
   with self.subTest(text=text):self.source.write_text(text);self.refuse()
 def test_wrong_types_and_consumed_case_refuse(self):
  cases=[{'paths':[],'schema':v} for v in [None,True,1.0,2,'1']]+[{'paths':[],'name':v} for v in [None,1,False]]
  cases += [{'paths':[],'Schema':1},{'Paths':[]},{'paths':None},{'paths':[{'Points':[]}]},{'paths':[{'closed':1,'points':[]}]},{'paths':[{'closed':None,'points':[]}]},{'paths':[{'points':[{'X':1,'y':2}]}]},{'paths':[{'points':[{'x':True,'y':2}]}]},{'paths':[{'points':[{'x':'1','y':2}]}]},{'paths':[{'points':[{'x':1}]}]}]
  for case in cases:
   with self.subTest(case=case):self.source.write_text(json.dumps(case));self.refuse()
 def test_overflow_and_nonfinite_coordinates_refuse(self):
  for value in [1e39,-1e39,float('inf'),float('nan')]:
   with self.subTest(value=value):self.source.write_text(json.dumps({'paths':[{'points':[{'x':value,'y':0}]}]}));self.refuse()
 def test_invalid_utf8_and_embedded_name_nul_refuse(self):
  for text in [b'{"name":"bad-\xff","paths":[]}',b'{"name":"bad\\u0000name","paths":[]}']:
   with self.subTest(text=text):self.source.write_bytes(text);self.refuse()
 def test_depth_refuses(self):
  self.source.write_text('{"paths":[],"meta":'+('['*65)+'0'+(']'*65)+'}');self.refuse()
 def test_path_count_bound(self):
  self.source.write_text(json.dumps({'paths':[{'points':[]}]*1025}));self.refuse()
  self.source.write_text(json.dumps({'paths':[{'points':[]}]*1024}));self.assertEqual(self.invoke().returncode,0)
 def test_total_point_count_bound(self):
  self.source.write_text(json.dumps({'paths':[{'points':[{'x':0,'y':0}]*5001}]*2}));self.refuse()
  self.source.write_text(json.dumps({'paths':[{'points':[{'x':0,'y':0}]*5000}]*2}));self.assertEqual(self.invoke().returncode,0)
 def test_optional_fields_empty_paths_and_unknown_metadata(self):
  for case in [{'paths':[]},{'paths':[{'points':[]}]},{'paths':[],'unknown':{'number':3,'string':'λ'}}]:
   with self.subTest(case=case):self.source.write_text(json.dumps(case));self.assertEqual(self.invoke().returncode,0)
if __name__=='__main__':unittest.main()
