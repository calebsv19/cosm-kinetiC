import hashlib,json,sys,unittest
from pathlib import Path
from unittest.mock import patch
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
import cfd_reference3d_packed_physical_matched as w
from cfd_reference3d_preconditioner import array_sha
class Matched(unittest.TestCase):
 def test_exact_geometry_and_original_full_recipe(self):
     bundle, g = w.second_normal_tensor_mesh(4.0)
     self.assertEqual(bundle[0].nelements, 28416)
     self.assertTrue(g['body_surface_triangles_preserved'])
     original = w.probe.domain_mesh

     def fake(**k):
         self.assertIs(w.probe.domain_mesh(), bundle)
         self.assertEqual((k['target'], k['retained_target'], k['maxiter'], k['restart'], k['coarse_pressure'], k['chunk_size'], k['storage_mode']), (1e-10, 1e-11, 3000, 30, 'quadratic', 512, 'auto'))
         return {}
     with patch.object(w, 'second_normal_tensor_mesh', return_value=(bundle, g)), patch.object(w.probe, 'run', side_effect=fake):
         w.run()
     self.assertIs(w.probe.domain_mesh, original)
     with self.assertRaises(ValueError):
         w.run(restart=6)
if __name__=='__main__':unittest.main()
