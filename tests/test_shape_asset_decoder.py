import json,os,subprocess,tempfile,unittest
from pathlib import Path
class AssetDecoder(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.binary=Path(os.environ['PHYSICS_SIM_ASSET_DECODER_TEST_BIN']).resolve()
 def check(self,text,accepted=True):
  if not isinstance(text,str):text=json.dumps(text,ensure_ascii=False)
  with tempfile.TemporaryDirectory(prefix='physics-asset-decode-') as td:
   p=Path(td)/'asset.json';p.write_text(text);row=subprocess.run([str(self.binary),str(p),text],capture_output=True,text=True,timeout=3)
   self.assertEqual(row.returncode,0,row.stderr);self.assertEqual(row.stdout.strip(),'1 1' if accepted else '0 0')
 def test_normal(self):self.check({'schema':1,'name':'simple','paths':[{'closed':True,'points':[{'x':1.25,'y':-2}]}]})
 def test_unicode(self):self.check({'name':'λ 🐕','paths':[]})
 def test_empty(self):self.check({'paths':[{'points':[]}]})
 def test_legacy_metadata_semantics(self):self.check({'schema':2,'name':False,'paths':[{'closed':1,'points':[]}],'unknown':'retained'})
 def test_legacy_case_semantics(self):self.check({'Paths':[],'Name':'legacy'})
 def test_invalid(self):
  for value in ['invalid',{'paths':None},{'paths':[{'points':[{'x':1}]}]}]:
   with self.subTest(value=value):self.check(value,False)
if __name__=='__main__':unittest.main()
