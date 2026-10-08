"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_packed_physical_matched.py."""
import unittest
import test_cfd_reference3d_packed_physical_matched as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class MatchedArchive(unittest.TestCase):

    def test_exact_geometry_and_original_full_recipe_archive(self):
        p = next((archive_root() / 'c3d-force-resume/runs').glob('*/*-receipt.json'))
        r = json.loads(p.read_text())
        old = json.loads(Path(r['command'][r['command'].index('--output') + 1]).read_text())
        self.assertEqual(array_sha(bundle[0].p, bundle[0].t), old['identity']['mesh_sha256'])

    def test_transforms_and_earned_strict_small_identity_archive(self):
        d = archive_root() / 'c3d-packed-physical'
        for t in json.loads((d / 'matched-transforms.json').read_text()):
            v = (R / t['parent']).read_text()
            for a, b in t['literal_replacements']:
                self.assertIn(a, v)
                v = v.replace(a, b)
            self.assertEqual(v, (R / t['output']).read_text())
            self.assertEqual(hashlib.sha256((R / t['parent']).read_bytes()).hexdigest(), t['parent_sha256'])
        e = json.loads((d / 'small-eligibility.json').read_text())
        self.assertTrue(e['matched_trial_permitted'])
        self.assertEqual(hashlib.sha256(Path(e['receipt']).read_bytes()).hexdigest(), e['receipt_sha256'])
        self.assertLessEqual(e['whole_wall_s'], 23.5018023327)
        self.assertLessEqual(e['full_residual']['true_residual'], 1e-10)

if __name__ == '__main__':
    unittest.main()
