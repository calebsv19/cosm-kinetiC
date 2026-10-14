"""Measured-reference units, immutable bytes and model-domain incompatibility."""
import copy,json,shutil,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.methanol_reference import inspect,table
REFERENCE=Path(sys.argv.pop(1)).resolve();CONFIG=json.loads(Path(sys.argv.pop(1)).read_text())

class ReferenceTests(unittest.TestCase):
    def test_si_conversion_and_physical_regime_refusal(self):
        v=inspect(REFERENCE,CONFIG)
        self.assertEqual(v['steady_hrr_w'],18600);self.assertEqual(v['steady_mass_loss_kg_s'],.000932)
        self.assertEqual(v['si_profiles']['upward_velocity'][0]['z_m'],.04)
        self.assertEqual(v['si_profiles']['surroundings_heat_flux'][0]['flux_w_m2'],3300)
        self.assertEqual(v['peak_reference_gas_temperature_k'],1290)
        self.assertFalse(v['physical_calibration_qualified']);self.assertFalse(v['current_model_compatible'])
        self.assertTrue(any('temperature-contrast' in s for s in v['compatibility_gaps']))

    def test_modified_measurement_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'reference';shutil.copytree(REFERENCE,path)
            p=path/'Methanol_30_cm_HRR_Sung_2024.csv';p.write_text(p.read_text().replace('18.6','1.86'))
            with self.assertRaisesRegex(ValueError,'bytes changed'):inspect(path,CONFIG)

    def test_units_and_nonfinite_values_refused(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp)/'bad.csv';p.write_text('x\ncm\n1\n')
            with self.assertRaisesRegex(ValueError,'units'):table(p,('x',),('m',),(1,))
            p.write_text('x\nm\nnan\n')
            with self.assertRaises(ValueError):table(p,('x',),('m',),(1,))

if __name__=='__main__':unittest.main()
