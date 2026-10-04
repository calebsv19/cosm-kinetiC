import hashlib,json,sys,unittest
from pathlib import Path
from unittest.mock import patch
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_preconditioner import array_sha
import cfd_reference3d_packed_matched_l4_probe as wrapper
class Matched(unittest.TestCase):
    def test_exact_existing_mesh_identity_and_delegated_original_gates(self):
        p=next((R/'build/c3d-force-resume/runs').glob('*/L4-body6-second-normal-complement10-receipt.json'));r=json.loads(p.read_text());old=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());bundle,g=wrapper.second_normal_tensor_mesh(4.)
        self.assertEqual(bundle[0].nelements,28416);self.assertEqual(array_sha(bundle[0].p,bundle[0].t),old['identity']['mesh_sha256']);self.assertTrue(g['body_surface_triangles_preserved'])
        original=wrapper.probe.domain_mesh
        def fake(**kwargs):
            self.assertIs(wrapper.probe.domain_mesh(),bundle);self.assertEqual(kwargs['target'],1e-10);self.assertEqual(kwargs['retained_target'],1e-11);self.assertEqual(kwargs['maxiter'],3000);self.assertEqual(kwargs['restart'],30);self.assertEqual(kwargs['coarse_pressure'],'quadratic');self.assertEqual(kwargs['storage_mode'],'auto');self.assertEqual(kwargs['chunk_size'],512);return {'numerically_accepted':False}
        with patch.object(wrapper,'second_normal_tensor_mesh',return_value=(bundle,g)),patch.object(wrapper.probe,'run',side_effect=fake):row=wrapper.run()
        self.assertIs(wrapper.probe.domain_mesh,original);self.assertEqual(row['geometry_control'],g)
        with self.assertRaises(ValueError):wrapper.run(restart=6)
    def test_exact_runner_transform_and_earned_small_receipt(self):
        t=json.loads((R/'build/c3d-packed-matched/runner-transform.json').read_text());v=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:self.assertIn(a,v);v=v.replace(a,b)
        self.assertEqual(v,(R/t['output']).read_text());p=R/'build/c3d-packed-inner8/checkpoint-audit.json';self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),'d75debbdc57fa2dedeb7cb2a3191afc0c24b62c0731994d4fd11421023a5f08c');self.assertTrue(json.loads(p.read_text())['matched_trial_permitted'])
if __name__=='__main__':unittest.main()
