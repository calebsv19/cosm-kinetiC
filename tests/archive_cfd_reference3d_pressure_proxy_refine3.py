"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_pressure_proxy_refine3.py."""
import unittest
import test_cfd_reference3d_pressure_proxy_refine3 as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class PressureRefineArchive(unittest.TestCase):

    def test_extended_complete_work_budget_and_source_transform_archive(self):
        t = json.loads((archive_root() / 'c3d-pressure-proxy-refine3/control-transform-control.json').read_text())
        s = (R / t['parent']).read_text()
        for x, y in t['literal_replacements']:
            self.assertIn(x, s)
            s = s.replace(x, y)
        self.assertEqual(s, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
