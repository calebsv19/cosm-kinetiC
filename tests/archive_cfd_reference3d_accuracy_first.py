"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_accuracy_first.py."""
import unittest
import test_cfd_reference3d_accuracy_first as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class AccuracyArchive(unittest.TestCase):

    def row(self):
        return dict(info=0, target=1e-10, final_residual={'true_residual': 9e-12}, flux_error=1e-12, volume_divergence_max_s_inv=1e-12, physical_energy_imbalance=0.01, physical_dissipation_w=1.0, tetrahedra=43008, iterations=100, peak_rss_bytes=2200 * 2 ** 20, wall_s=250.0)

    def test_saved_both_domain_geometry_and_exact_solver_recipe_archive(self):
        geometry = next((archive_root() / 'c3d-floor-balanced8').rglob('paired-floor-balanced8-geometry.npz'))
        self.assertEqual(w.sha(geometry), w.GEOMETRY_SHA)
        original = w.probe.domain_mesh

        def fake(**k):
            bundle = w.probe.domain_mesh()
            self.assertEqual(bundle[0].nelements, 43008 if k['length'] == 4.0 else 49920)
            self.assertEqual((k['target'], k['retained_target'], k['maxiter'], k['restart'], k['coarse_pressure'], k['chunk_size']), (1e-10, 1e-11, 3000, 6, 'quadratic', 512))
            return {'numerically_accepted': False}
        with patch.object(w.probe, 'run', side_effect=fake):
            for length in (4.0, 8.0):
                result = w.run(geometry, None, None, length)
                self.assertFalse(result['accuracy_contract']['performance_threshold_applied'])
        self.assertIs(w.probe.domain_mesh, original)
        with self.assertRaises(ValueError):
            w.run(geometry, None, None, 5.0)

    def test_exact_engine_and_supervisor_transforms_archive(self):
        for t in json.loads((archive_root() / 'c3d-accuracy-first/transforms.json').read_text()):
            v = (R / t['parent']).read_text()
            for a, c in t['literal_replacements']:
                self.assertIn(a, v)
                v = v.replace(a, c)
            v += t.get('append', '')
            self.assertEqual(v, (R / t['output']).read_text())
            self.assertEqual(hashlib.sha256((R / t['parent']).read_bytes()).hexdigest(), t['parent_sha256'])

if __name__ == '__main__':
    unittest.main()
