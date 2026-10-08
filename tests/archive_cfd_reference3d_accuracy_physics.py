"""Explicit historical archive assertions; current tests live in test_cfd_reference3d_accuracy_physics.py."""
import unittest
import test_cfd_reference3d_accuracy_physics as current
from cfd_reference_test_support import archive_root
globals().update({key:value for key,value in vars(current).items() if not key.startswith("__") and not (isinstance(value,type) and issubclass(value,unittest.TestCase))})

class PhysicsArchive(unittest.TestCase):

    def field(self):
        return dict(pressure_force_n=[0.6, 0, 0], raw_symmetric_viscous_force_n=[0.4, 0, 0], reaction_force_n=[1.0, 0, 0], inlet_pressure_pa=1.0, physical_dissipation_w=2.0)

    def test_observer_transforms_leave_stress_formulas_unchanged_archive(self):
        for t in json.loads((archive_root() / 'c3d-accuracy-first/observer-transforms.json').read_text()):
            v = (R / t['parent']).read_text()
            for a, b in t['literal_replacements']:
                self.assertIn(a, v)
                v = v.replace(a, b)
            self.assertEqual(v, (R / t['output']).read_text())

if __name__ == '__main__':
    unittest.main()
