import hashlib,json,sys,unittest
from pathlib import Path
from unittest.mock import patch
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
import cfd_reference3d_packed_physical_matched as w
from cfd_reference3d_preconditioner import array_sha
class Matched(unittest.TestCase):
 def test_exact_geometry_and_original_full_recipe(self):
  p=next((R/'build/c3d-force-resume/runs').glob('*/*-receipt.json'));r=json.loads(p.read_text());old=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());bundle,g=w.second_normal_tensor_mesh(4.);self.assertEqual(bundle[0].nelements,28416);self.assertEqual(array_sha(bundle[0].p,bundle[0].t),old['identity']['mesh_sha256']);self.assertTrue(g['body_surface_triangles_preserved']);original=w.probe.domain_mesh
  def fake(**k):
   self.assertIs(w.probe.domain_mesh(),bundle);self.assertEqual((k['target'],k['retained_target'],k['maxiter'],k['restart'],k['coarse_pressure'],k['chunk_size'],k['storage_mode']),(1e-10,1e-11,3000,30,'quadratic',512,'auto'));return {}
  with patch.object(w,'second_normal_tensor_mesh',return_value=(bundle,g)),patch.object(w.probe,'run',side_effect=fake):w.run()
  self.assertIs(w.probe.domain_mesh,original)
  with self.assertRaises(ValueError):w.run(restart=6)
 def test_transforms_and_earned_strict_small_identity(self):
  d=R/'build/c3d-packed-physical'
  for t in json.loads((d/'matched-transforms.json').read_text()):
   v=(R/t['parent']).read_text()
   for a,b in t['literal_replacements']:self.assertIn(a,v);v=v.replace(a,b)
   self.assertEqual(v,(R/t['output']).read_text());self.assertEqual(hashlib.sha256((R/t['parent']).read_bytes()).hexdigest(),t['parent_sha256'])
  e=json.loads((d/'small-eligibility.json').read_text());self.assertTrue(e['matched_trial_permitted']);self.assertEqual(hashlib.sha256(Path(e['receipt']).read_bytes()).hexdigest(),e['receipt_sha256']);self.assertLessEqual(e['whole_wall_s'],23.5018023327);self.assertLessEqual(e['full_residual']['true_residual'],1e-10)
if __name__=='__main__':unittest.main()
