"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_four_interval.py."""
import unittest
import test_cfd_reference3d_four_interval as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class FourIntervalArchive(unittest.TestCase):

    def test_only_declared_projection_and_routing_transform_archive(self):
        for t in json.loads((archive_root() / 'c3d-pressure-four-interval/transforms.json').read_text()):
            parent = (R / t['parent']).read_bytes()
            output = (R / t['output']).read_bytes()
            self.assertEqual(hashlib.sha256(parent).hexdigest(), t['parent_sha256'])
            self.assertEqual(hashlib.sha256(output).hexdigest(), t['output_sha256'])
            s = parent.decode()
            for a, b in t['literal_replacements']:
                self.assertIn(a, s)
                s = s.replace(a, b)
            self.assertEqual(s, output.decode())

if __name__ == '__main__':
    unittest.main()
