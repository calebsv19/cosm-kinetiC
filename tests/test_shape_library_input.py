"""Native startup library inventory/admission in isolated directories."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class LibraryInput(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.binary=Path(os.environ['PHYSICS_SIM_LIBRARY_INPUT_TEST_BIN']).resolve()
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='physics-library-',dir='/private/tmp');self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name);self.assets=self.root/'assets';self.assets.mkdir()
 def asset(self,file,name=None,points=0,paths=1):
  document={'schema':1,'name':name if name is not None else file,'paths':[{'points':[{'x':0,'y':1}]*points}]+[{'points':[]}]*(paths-1)}
  p=self.assets/(file+'.json');p.write_text(json.dumps(document));return p
 def invoke(self,path=None,mode='normal'):
  row=subprocess.run([str(self.binary),str(path or self.assets),mode],capture_output=True,text=True,timeout=15);self.assertEqual(row.returncode,0,row.stderr);return json.loads(row.stdout.splitlines()[-1]),row.stderr
 def refuse(self,path=None,mode='normal',diagnostic=None):
  data,stderr=self.invoke(path,mode);self.assertFalse(data['ok']);self.assertEqual(data['count'],0)
  if diagnostic:self.assertIn(diagnostic,stderr)
 def test_filename_order_is_deterministic(self):
  self.asset('z');self.asset('a');self.asset('m');data,_=self.invoke();self.assertTrue(data['ok']);self.assertEqual(data['names'],['a','m','z'])
 def test_invalid_selected_file_holds_whole_library(self):
  self.asset('valid');(self.assets/'invalid.json').write_text('invalid');self.refuse()
 def test_fifo_selected_entry_refuses_without_block(self):self.asset('valid');os.mkfifo(self.assets/'fifo.json');self.refuse()
 def test_symlink_selected_entry_refuses(self):
  p=self.asset('valid');(self.assets/'linked.json').symlink_to(p);self.refuse()
 def test_hardlink_selected_entry_refuses(self):
  p=self.asset('valid');os.link(p,self.assets/'linked.json');self.refuse()
 def test_directory_selected_entry_refuses(self):self.asset('valid');(self.assets/'nested.json').mkdir();self.refuse()
 def test_symlink_root_refuses(self):
  self.asset('valid');link=self.root/'linked';link.symlink_to(self.assets,target_is_directory=True);self.refuse(link)
 def test_linked_ancestor_refuses(self):
  self.asset('valid');link=self.root/'linked';link.symlink_to(self.root,target_is_directory=True);self.refuse(link/'assets')
 def test_missing_directory_refuses(self):self.refuse(self.root/'missing')
 def test_empty_directory_refuses(self):self.refuse()
 def test_ignored_and_hidden_entries_do_not_decode(self):
  self.asset('valid');(self.assets/'notes.txt').write_text('ignored');(self.assets/'.hidden.json').write_text('ignored');data,_=self.invoke();self.assertTrue(data['ok']);self.assertEqual(data['names'],['valid'])
 def test_duplicate_names_refuse(self):self.asset('a','same');self.asset('b','same');self.refuse()
 def test_missing_operational_name_refuses(self):
  self.asset('valid');(self.assets/'unnamed.json').write_text('{"paths":[]}');self.refuse()
 def test_asset_count_budget_refuses(self):
  for i in range(1025):self.asset(str(i))
  self.refuse(diagnostic='Asset count budget exceeded')
 def test_directory_entry_budget_refuses(self):
  self.asset('valid')
  for i in range(4097):(self.assets/(str(i)+'.txt')).touch()
  self.refuse(diagnostic='Entry budget exceeded')
 def test_aggregate_byte_budget_precedes_decode(self):
  for i in range(5):
   p=self.asset(str(i))
   with p.open('r+b') as file:file.truncate(16*1024*1024)
  self.refuse(diagnostic='File byte budget exceeded')
 def test_aggregate_point_budget_refuses(self):
  for i in range(11):self.asset(str(i),points=10000)
  self.refuse(diagnostic='Point budget exceeded')
 def test_aggregate_path_budget_refuses(self):
  for i in range(10):self.asset(str(i),paths=1024)
  self.refuse(diagnostic='Path budget exceeded')
 def test_nonempty_caller_library_is_preserved(self):
  self.asset('valid');data,_=self.invoke(mode='nonempty');self.assertFalse(data['ok']);self.assertEqual(data['names'],['existing'])
 def test_allocation_failure_does_not_publish(self):self.asset('valid');self.refuse(mode='allocfail')
 def test_enumeration_failure_does_not_publish(self):self.asset('valid');self.refuse(mode='enumfail')
 def test_changed_file_during_loading_refuses(self):self.asset('a');self.refuse(mode='mutate')
 def test_earlier_file_changed_by_later_load_refuses(self):self.asset('a');self.asset('b');self.refuse(mode='latemutate')
 def test_directory_addition_during_load_refuses(self):self.asset('a');self.refuse(mode='addentry')
 def test_checked_in_library_is_admitted(self):
  data,_=self.invoke(ROOT/'config/objects');self.assertTrue(data['ok']);self.assertEqual(data['count'],8);self.assertEqual(data['names'],['Hexagon','airfoil','bubble_m','circle','gap','single_curve','small_waist','u_shape'])
 def test_exact_asset_count_is_admitted(self):
  for i in range(1024):self.asset(str(i))
  data,_=self.invoke();self.assertTrue(data['ok']);self.assertEqual(data['count'],1024)
 def test_exact_entry_count_is_admitted(self):
  self.asset('valid')
  for i in range(4095):(self.assets/(str(i)+'.txt')).touch()
  data,_=self.invoke();self.assertTrue(data['ok']);self.assertEqual(data['count'],1)
 def test_exact_aggregate_point_budget_is_admitted(self):
  for i in range(10):self.asset(str(i),points=10000)
  data,_=self.invoke();self.assertTrue(data['ok']);self.assertEqual(data['count'],10)
 def test_exact_aggregate_path_budget_is_admitted(self):
  for i in range(10):self.asset(str(i),paths=1000)
  data,_=self.invoke();self.assertTrue(data['ok']);self.assertEqual(data['count'],10)
 def test_protected_directory_refuses(self):
  protected=self.root/'.git';protected.mkdir();(protected/'asset.json').write_text('{"name":"valid","paths":[]}');self.refuse(protected)
 def test_known_tmp_alias_is_admitted(self):
  self.asset('valid');alias=Path(str(self.assets).replace('/private/tmp/','/tmp/',1));data,_=self.invoke(alias);self.assertTrue(data['ok']);self.assertEqual(data['names'],['valid'])
 def test_checked_in_geometry_matches_legacy_by_name(self):
  import hashlib
  paths=sorted((ROOT/'config/objects').glob('*.json'));before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
  current,_=self.invoke(ROOT/'config/objects',mode='dump')
  row=subprocess.run([os.environ['PHYSICS_SIM_LIBRARY_LEGACY_TEST_BIN'],str(ROOT/'config/objects'),'dump'],capture_output=True,text=True,timeout=15);self.assertEqual(row.returncode,0,row.stderr);old=json.loads(row.stdout.splitlines()[-1])
  self.assertEqual(dict(zip(current['names'],current['serialized'])),dict(zip(old['names'],old['serialized'])))
  self.assertEqual(before,{p:hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
 def test_file_replacement_after_read_refuses(self):self.asset('a');self.refuse(mode='replacefile')
 def test_directory_replacement_after_read_refuses(self):self.asset('a');self.refuse(mode='swaproot')
if __name__=='__main__':unittest.main()
