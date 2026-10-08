"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_packed_matched_profile.py."""
import unittest
import test_cfd_reference3d_packed_matched_profile as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class ProfileArchive(unittest.TestCase):

    def fixture(self):
        b = Balanced()
        f = SimpleNamespace(balanced=b, coarse_factor=b.coarse, input_unchanged=lambda: True)
        pre = lambda x: np.r_[b.solve(x[:3]), -x[3:]]
        return (f, pre)

    def test_exact_runner_transform_and_rejected_predecessor_archive(self):
        d = archive_root() / 'c3d-packed-matched-profile'
        t = json.loads((d / 'runner-transform.json').read_text())
        v = (R / t['parent']).read_text()
        for a, b in t['literal_replacements']:
            self.assertIn(a, v)
            v = v.replace(a, b)
        self.assertEqual(v, (R / t['output']).read_text())
        pre = json.loads((d / 'predecessor.json').read_text())
        self.assertEqual(hashlib.sha256(Path(pre['path']).read_bytes()).hexdigest(), pre['sha256'])
        self.assertFalse(json.loads(Path(pre['path']).read_text())['eligibility']['finer_trial_permitted'])

if __name__ == '__main__':
    unittest.main()
