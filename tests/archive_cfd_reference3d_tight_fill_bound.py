"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_tight_fill_bound.py."""
import unittest
import test_cfd_reference3d_tight_fill_bound as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class BoundArchive(unittest.TestCase):

    def test_exact_transform_and_encoded_operator_prefix_archive(self):
        records = json.loads((archive_root() / 'c3d-tight-fill-bound/transforms.json').read_text())
        for t in records:
            s = (R / t['parent']).read_text()
            if t.get('prefix_preserved'):
                prefix, tail = s.split('void *cfd_fill1_factor_create', 1)
                for a, b in t['tail_replacements']:
                    self.assertIn(a, tail)
                    tail = tail.replace(a, b)
                s = prefix + t['helper'] + 'void *cfd_fill1_factor_create' + tail
            else:
                for a, b in t['literal_replacements']:
                    self.assertIn(a, s)
                    s = s.replace(a, b)
            self.assertEqual(s, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
