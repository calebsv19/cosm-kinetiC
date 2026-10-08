"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_packed_matched.py."""
import unittest
import test_cfd_reference3d_packed_matched as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class MatchedArchive(unittest.TestCase):

    def test_exact_existing_mesh_identity_and_delegated_original_gates_archive(self):
        p = next((archive_root() / 'c3d-force-resume/runs').glob('*/L4-body6-second-normal-complement10-receipt.json'))
        r = json.loads(p.read_text())
        old = json.loads(Path(r['command'][r['command'].index('--output') + 1]).read_text())
        self.assertEqual(array_sha(bundle[0].p, bundle[0].t), old['identity']['mesh_sha256'])

    def test_exact_runner_transform_and_earned_small_receipt_archive(self):
        t = json.loads((archive_root() / 'c3d-packed-matched/runner-transform.json').read_text())
        v = (R / t['parent']).read_text()
        for a, b in t['literal_replacements']:
            self.assertIn(a, v)
            v = v.replace(a, b)
        self.assertEqual(v, (R / t['output']).read_text())
        p = archive_root() / 'c3d-packed-inner8/checkpoint-audit.json'
        self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(), 'd75debbdc57fa2dedeb7cb2a3191afc0c24b62c0731994d4fd11421023a5f08c')
        self.assertTrue(json.loads(p.read_text())['matched_trial_permitted'])

if __name__ == '__main__':
    unittest.main()
