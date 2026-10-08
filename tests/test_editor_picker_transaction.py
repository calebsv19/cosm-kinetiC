"""Actual picker state transitions; UI effects stubbed, native load/conversion retained."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class PickerTransaction(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.binary=Path(os.environ['PHYSICS_SIM_PICKER_TEST_BIN']).resolve()
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(prefix='physics-picker-',dir='/private/tmp');self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
  (self.root/'.git').mkdir();(self.root/'data/runtime').mkdir(parents=True);(self.root/'config/import').mkdir(parents=True)
  self.asset=self.root/'data/runtime/fresh.asset.json';self.old=self.root/'old.json';self.write_asset(self.asset,2);self.write_asset(self.old,1)
 def write_asset(self,path,x,name='fresh'):
  path.write_text(json.dumps({'schema':1,'name':name,'paths':[{'closed':False,'points':[{'x':x,'y':0},{'x':3,'y':4}]}]}))
 def invoke(self,mode='normal',path='data/runtime/fresh.asset.json'):
  env=dict(os.environ);env.pop('SHAPE_ASSET_DIR',None)
  row=subprocess.run([str(self.binary),mode,path,str(self.old),'config'],cwd=self.root,env=env,capture_output=True,text=True,timeout=3)
  self.assertEqual(row.returncode,0,row.stderr);return json.loads(row.stdout.splitlines()[-1])
 def unchanged(self,row,cached=False,full=False):
  self.assertTrue(row['working_unchanged']);self.assertEqual(row['ok'],0);self.assertEqual(row['count'],64 if full else 0)
  self.assertEqual(row['library_count'],1 if cached else 0);self.assertFalse(row['dirty']);self.assertTrue(row['picker']);self.assertEqual(row['selected'],-1);self.assertEqual(row['selections'],0);self.assertEqual(row['refreshes'],0)
  if cached:self.assertEqual(row['x'],1)
 def test_missing_asset_does_not_add_unresolved_entry(self):self.unchanged(self.invoke(path='data/runtime/missing.asset.json'))
 def test_malformed_asset_does_not_add(self):
  self.asset.write_text('invalid');self.unchanged(self.invoke())
 def test_cached_asset_refreshes_geometry_in_stable_slot(self):
  row=self.invoke('cached');self.assertEqual(row['ok'],1);self.assertEqual(row['count'],1);self.assertEqual(row['library_count'],1);self.assertEqual(row['shape_id'],0);self.assertEqual(row['x'],2)
 def test_bad_reimport_preserves_previous_library_and_scene(self):
  self.asset.write_text('invalid');self.unchanged(self.invoke('cached'),cached=True)
 def test_missing_reimport_preserves_previous_library_and_scene(self):
  self.asset.unlink();self.unchanged(self.invoke('cached'),cached=True)
 def test_full_scene_refuses_before_conversion(self):
  source=self.root/'config/import/u_shape.json';source.write_bytes((ROOT/'import/u_shape.json').read_bytes())
  row=self.invoke('full','config/import/u_shape.json');self.unchanged(row,full=True);self.assertFalse((self.root/'data/runtime/u_shape.asset.json').exists())
 def test_no_library_refuses_before_conversion(self):
  source=self.root/'config/import/u_shape.json';source.write_bytes((ROOT/'import/u_shape.json').read_bytes())
  self.unchanged(self.invoke('nolibrary','config/import/u_shape.json'));self.assertFalse((self.root/'data/runtime/u_shape.asset.json').exists())
 def test_conversion_failure_does_not_add_raw_fallback(self):
  (self.root/'config/import/invalid.json').write_text('invalid');self.unchanged(self.invoke(path='config/import/invalid.json'))
 def test_conversion_success_adds_resolved_asset_and_refreshes_once(self):
  source=self.root/'config/import/u_shape.json';source.write_bytes((ROOT/'import/u_shape.json').read_bytes())
  row=self.invoke(path='config/import/u_shape.json');self.assertEqual(row['ok'],1);self.assertEqual(row['count'],1);self.assertEqual(row['library_count'],1);self.assertEqual(row['shape_id'],0);self.assertEqual(row['refreshes'],1);self.assertEqual(row['selections'],1);self.assertTrue(row['dirty']);self.assertFalse(row['picker'])
 def test_identity_mismatch_does_not_add_unresolved_entry(self):
  self.write_asset(self.asset,2,'different');self.unchanged(self.invoke())
 def test_direct_asset_success_commits_once(self):
  row=self.invoke();self.assertEqual(row['ok'],1);self.assertEqual(row['count'],1);self.assertEqual(row['library_count'],1);self.assertEqual(row['shape_id'],0);self.assertEqual(row['x'],2);self.assertTrue(row['dirty']);self.assertFalse(row['picker']);self.assertEqual(row['refreshes'],0);self.assertEqual(row['selections'],1)
 def test_library_growth_failure_preserves_scene_and_library(self):self.unchanged(self.invoke('growthfail'))
 def test_library_slot_bound_refuses_append(self):
  row=self.invoke('libraryfull');self.assertEqual(row['ok'],0);self.assertEqual(row['library_count'],1024);self.assertEqual(row['count'],0);self.assertTrue(row['working_unchanged']);self.assertTrue(row['picker']);self.assertFalse(row['dirty'])
 def test_reimport_preserves_existing_scene_slot_reference(self):
  row=self.invoke('existing');self.assertEqual(row['ok'],1);self.assertEqual(row['count'],2);self.assertEqual(row['library_count'],1);self.assertEqual(row['shape_id'],0);self.assertEqual(row['x'],2)
 def test_empty_path_refuses(self):self.unchanged(self.invoke('empty'))
 def test_unterminated_picker_path_refuses(self):self.unchanged(self.invoke('unterminated'))
 def test_invalid_picker_count_refuses(self):self.unchanged(self.invoke('badcount'))
 def test_negative_row_refuses(self):self.unchanged(self.invoke('badrow'))
 def test_mismatched_reimport_preserves_old_library(self):
  self.write_asset(self.asset,2,'different');self.unchanged(self.invoke('cached'),cached=True)
 def test_drop_load_failure_does_not_add(self):self.unchanged(self.invoke('drop',path='data/runtime/missing.asset.json'))
 def test_drop_conversion_failure_does_not_add_raw_fallback(self):
  (self.root/'config/import/invalid.json').write_text('invalid');self.unchanged(self.invoke('drop',path='config/import/invalid.json'))
 def test_drop_success_commits_resolved_asset_at_position(self):
  row=self.invoke('drop');self.assertEqual(row['ok'],1);self.assertEqual(row['count'],1);self.assertEqual(row['shape_id'],0);self.assertEqual(row['position_x'],0.25);self.assertEqual(row['position_y'],0.75)
 def test_nonfinite_drop_position_refuses(self):self.unchanged(self.invoke('dropnan'))
 def test_drop_existing_entry_selects_without_duplicate_or_reload(self):
  row=self.invoke('drop-existing');self.assertEqual(row['ok'],1);self.assertEqual(row['count'],1);self.assertEqual(row['library_count'],1);self.assertTrue(row['working_unchanged']);self.assertFalse(row['dirty']);self.assertEqual(row['selections'],1);self.assertEqual(row['selected'],0);self.assertEqual(row['x'],1)
 def test_asset_fifo_refuses_without_block_or_scene_change(self):
  self.asset.unlink();os.mkfifo(self.asset);self.unchanged(self.invoke())
 def test_linked_reimport_refuses_and_preserves_cache(self):
  self.asset.unlink();self.asset.symlink_to(self.old);self.unchanged(self.invoke('cached'),cached=True)
if __name__=='__main__':unittest.main()
