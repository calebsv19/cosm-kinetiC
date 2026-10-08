"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_pressure_coverage16.py."""
import unittest
import test_cfd_reference3d_pressure_coverage16 as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class Coverage16Archive(unittest.TestCase):

    def fixture(self):
        rng = np.random.default_rng(816)
        centers = rng.random((150, 3)) * np.array([4.0, 2.0, 2.0])
        volumes = rng.random(150) + 1.0
        lo = np.array([1.5, 0.5, 0.5])
        hi = lo + 1.0
        return (rng, centers, volumes, lo, hi)

    def test_exact_control_and_full_probe_transforms_archive(self):
        t = json.loads((archive_root() / 'c3d-pressure-coverage16/control-transform-control.json').read_text())
        s = (R / t['parent']).read_text()
        for a, b in t['literal_replacements']:
            self.assertIn(a, s)
            s = s.replace(a, b)
        self.assertEqual(s, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
