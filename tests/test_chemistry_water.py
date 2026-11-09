"""Published property witnesses and end-to-end finite chamber tests."""
import json
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_reactor import digest
from chemistry_water import Properties, WaterChamber, example
from chemistry_workspace import execute, packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_LIBRARY']
# NISTIR 5078 Table 1, pressure Pa, densities kg/m3, enthalpies J/kg.
TABLE = [
    (283.15, 1228.2, 999.65, .009407, 42020, 2519200),
    (298.15, 3169.9, 997.00, .023075, 104830, 2546500),
    (303.15, 4247.0, 995.61, .030415, 125730, 2555500),
    (323.15, 12352, 988.00, .083147, 209340, 2591300),
]


class WaterTests(unittest.TestCase):
    def chamber(self, **changes):
        return WaterChamber(LIBRARY, dict(example(), **changes))

    def test_reference_properties(self):
        lib = self.chamber().lib
        for temperature, pressure, liquid, vapor, h_liquid, h_vapor in TABLE:
            props = Properties()
            self.assertTrue(lib.chem_water_properties(temperature, props))
            calculated = (props.pressure_pa, props.liquid_density, props.vapor_density,
                          props.liquid_u + props.pressure_pa / props.liquid_density,
                          props.vapor_u + props.pressure_pa / props.vapor_density)
            for actual, reference in zip(calculated, (pressure, liquid, vapor, h_liquid, h_vapor)):
                self.assertLess(abs(actual / reference - 1), .001)
            latent_u = h_vapor - h_liquid - pressure * (1 / vapor - 1 / liquid)
            self.assertLess(abs((props.vapor_u - props.liquid_u) / latent_u - 1), .001)

    def test_bath_equilibrium_and_energy_sign(self):
        _, p, rho_l, rho_v, h_l, h_v = TABLE[1]
        expected_vapor = rho_v * (.01 - .01 / rho_l) / (1 - rho_v / rho_l)
        latent_u = h_v - h_l - p * (1 / rho_v - 1 / rho_l)
        for initial, sign in [(.00005, 1), (.00035, -1)]:
            chamber = self.chamber(insulated=False, initial_vapor_kg=initial, dt_s=1000.)
            chamber.advance(1)
            report = chamber.report()['water']
            self.assertAlmostEqual(report['vapor_kg'], expected_vapor, delta=2e-7)
            self.assertAlmostEqual(report['relative_saturation'], 1, places=13)
            self.assertAlmostEqual(report['bath_heat_into_chamber_j'],
                                   (expected_vapor - initial) * latent_u, delta=.5)
            self.assertGreater(sign * report['bath_heat_into_chamber_j'], 0)
            self.assertLess(abs(report['energy_residual_j']), 1e-9)

    def test_insulated_endpoint_against_two_table_rows(self):
        # Use table energies to choose a carrier capacity that brings a 30 C
        # initial chamber to wet equilibrium at 25 C.
        _, p, rho_l, rho_v, h_l, h_v = TABLE[1]
        target_vapor = rho_v * (.01 - .01 / rho_l) / (1 - rho_v / rho_l)
        target_energy = (.01 - target_vapor) * (h_l - p / rho_l) + target_vapor * (h_v - p / rho_v)
        _, p, rho_l, rho_v, h_l, h_v = TABLE[2]
        initial_vapor = .00005
        initial_energy = (.01 - initial_vapor) * (h_l - p / rho_l) + initial_vapor * (h_v - p / rho_v)
        chamber = self.chamber(temperature_k=303.15, initial_vapor_kg=initial_vapor,
                               thermal_capacity_j_k=(target_energy - initial_energy) / 5,
                               dt_s=1000.)
        chamber.advance(1)
        report = chamber.report()['water']
        self.assertAlmostEqual(report['temperature_k'], 298.15, delta=.01)
        self.assertAlmostEqual(report['vapor_kg'], target_vapor, delta=2e-7)
        self.assertEqual(report['bath_heat_into_chamber_j'], 0)
        self.assertLess(abs(report['energy_residual_j']), 1e-8)

    def test_insulated_monotonic_exchange_and_conservation(self):
        for initial, sign in [(.00005, 1), (.00035, -1)]:
            chamber = self.chamber(initial_vapor_kg=initial)
            initial_energy = chamber.state.energy_j
            for _ in range(100):
                old_vapor, old_temperature = chamber.state.vapor_kg, chamber.state.temperature_k
                chamber.advance(1)
                report = chamber.report()['water']
                self.assertGreaterEqual(sign * (report['vapor_kg'] - old_vapor), 0)
                self.assertGreaterEqual(sign * (old_temperature - report['temperature_k']), 0)
                self.assertAlmostEqual(report['liquid_kg'] + report['vapor_kg'], .01, places=16)
                self.assertEqual(report['internal_energy_j'], initial_energy)
                self.assertLess(abs(report['energy_residual_j']), 1e-8)

    def test_finite_dryout_and_stationarity(self):
        for insulated in (False, True):
            chamber = self.chamber(insulated=insulated, water_kg=.00005,
                                   initial_vapor_kg=0., dt_s=1000.)
            chamber.advance(1)
            report = chamber.report()['water']
            self.assertEqual(report['liquid_kg'], 0)
            self.assertEqual(report['vapor_kg'], .00005)
            self.assertLess(report['relative_saturation'], 1)
            chamber.advance(1)
            self.assertEqual(report, chamber.report()['water'])

    def test_exact_relaxation_and_checkpoint(self):
        for insulated in (False, True):
            chamber = self.chamber(insulated=insulated)
            single = self.chamber(insulated=insulated, dt_s=5)
            chamber.advance(10)
            single.advance(1)
            self.assertAlmostEqual(chamber.state.vapor_kg, single.state.vapor_kg, places=16)
            session = json.loads(json.dumps(packet(chamber)))
            reopened = restore(LIBRARY, session)
            reopened.advance(10)
            chamber.advance(10)
            self.assertEqual(packet(reopened), packet(chamber))
            checkpoint = chamber.checkpoint()
            checkpoint['payload']['bath_heat'] += 1
            checkpoint['sha256'] = digest(checkpoint['payload'])
            before = packet(chamber)
            with self.assertRaises(ValueError):
                chamber.restore(checkpoint)
            self.assertEqual(before, packet(chamber))

    def test_bounds_and_workspace_save(self):
        for config in ({'temperature_k': 280}, {'initial_vapor_kg': -.1},
                       {'initial_vapor_kg': .008}, {'water_kg': 5}, {'relaxation_s': 0},
                       {'temperature_k': 283.15, 'initial_vapor_kg': 0}):
            with self.assertRaises(ValueError):
                self.chamber(**config)
        session = packet(self.chamber())
        with tempfile.TemporaryDirectory() as root:
            path = str(Path(root) / 'saved.json')
            execute(LIBRARY, dict(token=1, action='save', args=dict(path=path, session=session)))
            reopened = execute(LIBRARY, dict(token=2, action='open', args=dict(path=path)))
            self.assertEqual(reopened['session'], session)


if __name__ == '__main__':
    unittest.main()
