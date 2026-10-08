import hashlib,os,subprocess,tempfile,unittest
from pathlib import Path
class GridIdentityAdmission(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.binary=Path(os.environ['PHYSICS_SIM_GRID_IDENTITY_TEST_BIN']).resolve()
  cls.digest=hashlib.sha256(cls.binary.read_bytes()).hexdigest()
 @classmethod
 def tearDownClass(cls):
  if hashlib.sha256(cls.binary.read_bytes()).hexdigest()!=cls.digest:raise AssertionError('Harness changed')
 def case(self,action,change,*args):
  p=subprocess.run([str(self.binary),action,change,*map(str,args)],capture_output=True,text=True,timeout=5)
  self.assertEqual(p.returncode,0,p.stdout+p.stderr)
 def mismatches(self,action):
  for change in ['grow','shrink','swap','zero','negative','int','fluid','metadata']:
   with self.subTest(change=change):self.case(action,change)
 def test_static(self):self.mismatches('static')
 def test_obstacles(self):self.mismatches('obstacles')
 def test_dynamic(self):self.mismatches('dynamic')
 def test_distance(self):self.mismatches('distance')
 def test_emitter_masks(self):self.mismatches('emitter_masks')
 def test_emitters(self):self.mismatches('emitters')
 def test_boundary(self):self.mismatches('boundary')
 def test_enforce_boundary(self):self.mismatches('enforce_boundary')
 def test_enforce_obstacles(self):self.mismatches('enforce_obstacles')
 def test_motion(self):self.mismatches('motion')
 def test_brush(self):self.mismatches('brush')
 def test_step(self):self.mismatches('step')
 def test_distinct_step_config(self):self.case('step','step_only')
 def test_restoring_config_resumes(self):self.case('restore','shrink')
 def test_views_describe_allocation(self):self.case('view_original','grow')
 def test_invalid_fluid_views_and_local_operations(self):
  for action in ['clear','seed','valid','fluid_view','obstacle_view','report','compatibility']:
   for change in ['fluid','metadata']:
    with self.subTest(action=action,change=change):self.case(action,change)
 def test_invalid_snapshot_preserves_predecessor(self):
  with tempfile.TemporaryDirectory(prefix='physics-grid-identity-',dir='/private/tmp') as root:
   path=Path(root)/'snapshot.bin';path.write_bytes(b'prior')
   for change in ['fluid','metadata']:
    self.case('snapshot',change,path);self.assertEqual(path.read_bytes(),b'prior')
 def test_matching_mutation_routes(self):
  for action in ['static','obstacles','dynamic','distance','emitter_masks','emitters','boundary','enforce_boundary','enforce_obstacles','motion','brush','step']:
   with self.subTest(action=action):self.case(action,'normal')
if __name__=='__main__':unittest.main()
