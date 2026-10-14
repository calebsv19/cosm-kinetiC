import os
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from fire_liquid_qualification import qualify

class MaterialQualification(unittest.TestCase):
    def test_native_evidence_and_unfitted_comparison(self):
        a, e = qualify(os.environ['GROWTH_ROOT'], os.environ['GROWTH_LIQUID_WORKER'],
                       os.environ['METHANOL_REFERENCE'])
        self.assertTrue(a['surface_numerics_qualified'])
        self.assertTrue(a['exact_restart_passed'])
        self.assertLess(a['cadence_mass_difference_kg'], 1e-12)
        self.assertEqual(len(e['receipts']), 60)
        self.assertFalse(a['parameters_fitted'])
        self.assertFalse(a['loss_diagnostic_used_as_input'])
        self.assertFalse(a['physical_calibration_qualified'])
        self.assertFalse(a['coupled_thermal_feedback_qualified'])
        self.assertGreater(a['relative_mass_loss_error'], .3)
        self.assertGreater(a['unmodeled_loss_implied_by_measured_rate_w'], 300)
        self.assertLess(e['synthetic_heat_cool']['cooled']['temperature_k'],
                        e['synthetic_heat_cool']['heated']['temperature_k'])

if __name__ == '__main__': unittest.main()
