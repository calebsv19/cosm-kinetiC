"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_local_cost_split.py."""
import unittest
import test_cfd_reference3d_local_cost_split as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class SplitArchive(unittest.TestCase):

    def test_exact_source_transform_and_additional_reservation_archive(self):
        d = archive_root() / 'c3d-local-cost-split'
        t = json.loads((d / 'native-transform.json').read_text())
        self.assertEqual((R / t['output']).read_text(), (R / t['parent']).read_text() + t['appended_function'])
        self.assertEqual(hashlib.sha256((R / t['output']).read_bytes()).hexdigest(), t['output_sha256'])
        t = json.loads((d / 'runner-transform.json').read_text())
        v = (R / t['parent']).read_text()
        for a, b in t['literal_replacements']:
            self.assertIn(a, v)
            v = v.replace(a, b)
        self.assertEqual(v, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
