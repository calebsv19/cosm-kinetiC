"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_priority_fill1.py."""
import unittest
import test_cfd_reference3d_priority_fill1 as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class PriorityArchive(unittest.TestCase):

    def test_exact_source_transforms_prefix_and_storage_bound_archive(self):
        for t in json.loads((archive_root() / 'c3d-priority-fill1/transforms.json').read_text()):
            s = (R / t['parent']).read_text()
            if 'append' in t:
                s += t['append']
            else:
                for a, b in t['literal_replacements']:
                    self.assertIn(a, s)
                    s = s.replace(a, b)
            self.assertEqual(s, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
