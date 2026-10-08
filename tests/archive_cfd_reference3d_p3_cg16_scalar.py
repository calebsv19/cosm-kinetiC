"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_p3_cg16_scalar.py."""
import unittest
import test_cfd_reference3d_p3_cg16_scalar as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class Inner16Archive(unittest.TestCase):

    def test_exact_declared_loop_transform_archive(self):
        t = json.loads((archive_root() / 'c3d-p3-cg16-scalar/factor-transform-control.json').read_text())
        s = (R / t['parent']).read_text()
        for a, b in t['literal_replacements']:
            self.assertIn(a, s)
            s = s.replace(a, b)
        self.assertEqual(s, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
