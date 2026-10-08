"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_adaptive_cg8.py."""
import unittest
import test_cfd_reference3d_adaptive_cg8 as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class AdaptiveArchive(unittest.TestCase):

    def test_exact_declared_factor_control_transforms_and_stats_histogram_archive(self):
        for fn in ('factor-transform-control.json', 'control-transform-control.json'):
            t = json.loads((archive_root() / 'c3d-adaptive-cg8' / fn).read_text())
            s = (R / t['parent']).read_text()
            for a, b in t['literal_replacements']:
                self.assertIn(a, s)
                s = s.replace(a, b)
            self.assertEqual(s, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
