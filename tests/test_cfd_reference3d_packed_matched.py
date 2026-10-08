import hashlib,json,sys,unittest
from pathlib import Path
from unittest.mock import patch
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_preconditioner import array_sha
import cfd_reference3d_packed_matched_l4_probe as wrapper
class Matched(unittest.TestCase):
    def test_exact_existing_mesh_identity_and_delegated_original_gates(self):
        bundle, g = wrapper.second_normal_tensor_mesh(4.0)
        self.assertEqual(bundle[0].nelements, 28416)
        self.assertTrue(g['body_surface_triangles_preserved'])
        original = wrapper.probe.domain_mesh

        def fake(**kwargs):
            self.assertIs(wrapper.probe.domain_mesh(), bundle)
            self.assertEqual(kwargs['target'], 1e-10)
            self.assertEqual(kwargs['retained_target'], 1e-11)
            self.assertEqual(kwargs['maxiter'], 3000)
            self.assertEqual(kwargs['restart'], 30)
            self.assertEqual(kwargs['coarse_pressure'], 'quadratic')
            self.assertEqual(kwargs['storage_mode'], 'auto')
            self.assertEqual(kwargs['chunk_size'], 512)
            return {'numerically_accepted': False}
        with patch.object(wrapper, 'second_normal_tensor_mesh', return_value=(bundle, g)), patch.object(wrapper.probe, 'run', side_effect=fake):
            row = wrapper.run()
        self.assertIs(wrapper.probe.domain_mesh, original)
        self.assertEqual(row['geometry_control'], g)
        with self.assertRaises(ValueError):
            wrapper.run(restart=6)
if __name__=='__main__':unittest.main()
