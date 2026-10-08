"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_native_inner8.py."""
import unittest
import test_cfd_reference3d_native_inner8 as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class NativeArchive(unittest.TestCase):

    def test_exact_transforms_prefix_and_work_budget_archive(self):
        for t in json.loads((archive_root() / 'c3d-native-inner8/transforms.json').read_text()):
            v = (R / t['parent']).read_text()
            for a, b in t['literal_replacements']:
                self.assertIn(a, v)
                v = v.replace(a, b)
            v += t['append']
            self.assertEqual(v, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
