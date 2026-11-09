"""Analytic transport and moving-boundary witnesses for the planar column."""
import json
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_reactor import digest
from chemistry_water_interface import REFERENCE_K, WaterInterface, example
from chemistry_workspace import execute, packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class InterfaceTests(unittest.TestCase):
    def model(self, cells=16, **changes):
        return WaterInterface(LIBRARY, dict(example(cells), **changes))

    def test_diffusion_and_heat_spatial_refinement(self):
        errors = {'diffusion': [], 'heat': []}
        for cells in (8, 16, 32):
            dt = .02 * (8 / cells)**2
            for kind in errors:
                config = example(cells)
                config.update(mass_transfer_velocity_m_s=0., bath=True, dt_s=dt)
                if kind == 'heat':
                    config.update(diffusivity_m2_s=0., vapor_density_kg_m3=[0.] * cells)
                else:
                    config['conductivity_w_m_k'] = 0.
                initial = WaterInterface(LIBRARY, config)
                position, width = initial.geometry(config['liquid_kg'])
                length = config['height_m'] - position
                wave = math.pi / length if kind == 'diffusion' else math.pi / (2 * length)
                average = math.sin(wave * width / 2) / (wave * width / 2)
                if kind == 'diffusion':
                    profile = [average * math.cos(wave * width * (i + .5)) for i in range(cells)]
                    config['vapor_density_kg_m3'] = [.012 + .004 * x for x in profile]
                    diffusivity = config['diffusivity_m2_s']
                else:
                    profile = [average * math.sin(wave * width * (i + .5)) for i in range(cells)]
                    config['gas_temperature_k'] = [REFERENCE_K + 3 * x for x in profile]
                    diffusivity = (config['conductivity_w_m_k'] * config['area_m2'] * length /
                                   config['carrier_heat_capacity_j_k'])
                model = WaterInterface(LIBRARY, config)
                model.advance(round(1 / dt))
                result = model.report()['water_interface']
                decay = math.exp(-diffusivity * wave**2)
                if kind == 'diffusion':
                    actual = [(value - .012) / .004 for value in result['vapor_density_kg_m3']]
                else:
                    actual = [(value - REFERENCE_K) / 3 for value in result['gas_temperature_k']]
                    self.assertLess(result['bath_heat_into_chamber_j'], 0)
                errors[kind].append(max(abs(value - decay * shape)
                                        for value, shape in zip(actual, profile)))
                self.assertEqual(result['position_m'], position)
                self.assertLess(abs(result['energy_residual_j']), 1e-9)
        for kind, values in errors.items():
            print(kind, 'normalized maximum errors', values)
            self.assertGreater(values[0] / values[1], 3.7)
            self.assertGreater(values[1] / values[2], 3.7)

    def test_moving_cell_independent_time_integral(self):
        # A single isothermal gas cell has a separable moving-volume ODE.
        # Integrate dt/dm by Simpson quadrature instead of stepping the ODE.
        errors = []
        for dt in (.1, .05, .025, .0125):
            model = self.model(1, bath=True, conductivity_w_m_k=0., dt_s=dt)
            initial_vapor = model.state[2]
            initial_liquid = model.state[0]
            saturation = model.properties(REFERENCE_K).vapor_density
            area = model.config['area_m2']
            model.advance(round(5 / dt))
            report = model.report()['water_interface']
            vapor = report['vapor_kg']
            increment = (vapor - initial_vapor) / 200
            terms = []
            for i in range(201):
                mass = initial_vapor + increment * i
                liquid = initial_liquid + initial_vapor - mass
                width = model.config['height_m'] - liquid / (model.density_liquid * area)
                resistance = (1 / model.config['mass_transfer_velocity_m_s'] +
                              width / (2 * model.config['diffusivity_m2_s']))
                weight = 1 if i in (0, 200) else (4 if i % 2 else 2)
                terms.append(weight * resistance / (area * (saturation - mass / (area * width))))
            errors.append(abs(increment * math.fsum(terms) / 3 - 5))
            self.assertAlmostEqual(report['displacement_m'],
                                   -(vapor - initial_vapor) / (area * model.density_liquid),
                                   delta=1e-17)
            self.assertAlmostEqual(report['bath_heat_into_chamber_j'],
                                   (vapor - initial_vapor) * model.latent, delta=1e-10)
        print('moving-cell elapsed-time errors (s)', errors)
        self.assertGreater(errors[0] / errors[1], 3.7)
        self.assertGreater(errors[1] / errors[2], 3.7)
        self.assertGreater(errors[2] / errors[3], 3.7)
        self.assertLess(errors[-1], 1e-6)

    def test_coupled_evaporation_and_condensation(self):
        for bath in (True, False):
            for density, direction in ((.008, 1), (.032, -1)):
                model = self.model(bath=bath, vapor_density_kg_m3=[density] * 16)
                model.advance(100)
                report = model.report()['water_interface']
                self.assertGreater(-direction * report['displacement_m'], 0)
                self.assertGreater(direction * (report['vapor_density_kg_m3'][0] -
                                                report['vapor_density_kg_m3'][-1]), 0)
                if bath:
                    self.assertGreater(direction * report['bath_heat_into_chamber_j'], 0)
                    self.assertAlmostEqual(report['liquid_temperature_k'], REFERENCE_K)
                else:
                    self.assertEqual(report['bath_heat_into_chamber_j'], 0)
                    self.assertGreater(direction * (REFERENCE_K - report['liquid_temperature_k']), 0)
                self.assertLess(abs(report['mass_residual_kg']), 1e-16)
                self.assertLess(abs(report['energy_residual_j']), 1e-10)
                edges = report['edges_m']
                self.assertEqual(edges[1], report['position_m'])
                self.assertEqual(edges[-1], model.config['height_m'])
                self.assertTrue(all(b > a for a, b in zip(edges, edges[1:])))

    def test_coupled_grid_refinement(self):
        for initial_density in (.008, .032):
            positions = []
            for cells in (8, 16, 32, 64):
                dt = .005 * (16 / cells)**2
                model = self.model(cells, dt_s=dt,
                                   vapor_density_kg_m3=[initial_density] * cells)
                model.advance(round(.5 / dt))
                positions.append(model.report()['water_interface']['displacement_m'])
            differences = [abs(a - b) for a, b in zip(positions, positions[1:])]
            print('coupled interface refinement (m)', initial_density, differences)
            self.assertGreater(differences[0] / differences[1], 3.5)
            self.assertGreater(differences[1] / differences[2], 3.5)

    def test_failed_batch_retains_original_state(self):
        model = self.model(liquid_temperature_k=283.151, gas_temperature_k=[283.151] * 16,
                           vapor_density_kg_m3=[0.] * 16, dt_s=.0001)
        before = model.checkpoint()
        with self.assertRaisesRegex(ValueError, 'temperature'):
            model.advance(5000)
        self.assertEqual(model.checkpoint(), before)

    def test_uniform_saturation_stationary(self):
        probe = self.model()
        model = self.model(vapor_density_kg_m3=[probe.properties(REFERENCE_K).vapor_density] * 16)
        before = model.state.copy()
        model.advance(100)
        self.assertEqual(before, model.state)

    def test_refusal_and_restart(self):
        with self.assertRaises(ValueError):
            self.model(dt_s=100)
        model = self.model()
        model.advance(12)
        with tempfile.TemporaryDirectory() as folder:
            path = str(Path(folder) / 'session.json')
            execute(LIBRARY, dict(token=1, action='save', args=dict(path=path, session=packet(model))))
            session = execute(LIBRARY, dict(token=2, action='open', args=dict(path=path)))['session']
        recovered = restore(LIBRARY, json.loads(json.dumps(session)))
        recovered.advance(30)
        model.advance(30)
        self.assertEqual(packet(model), packet(recovered))
        checkpoint = model.checkpoint()
        checkpoint['payload']['state'][2] += 1e-6
        checkpoint['sha256'] = digest(checkpoint['payload'])
        before = packet(model)
        with self.assertRaises(ValueError):
            model.restore(checkpoint)
        self.assertEqual(packet(model), before)


if __name__ == '__main__':
    unittest.main()
