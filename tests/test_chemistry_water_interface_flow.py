"""Independent momentum, transport, and exchange witnesses for the RC7 channel."""
import json
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from chemistry_reactor import digest
from chemistry_water_interface_flow import FlowingWaterInterface, example
from chemistry_workspace import execute, packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class ChannelTests(unittest.TestCase):
    def fixture(self, case):
        return json.loads((ROOT / 'data/chemistry' / f'water_interface_flow_{case}_v1.json').read_text())

    def test_stokes_velocity_flow_rate_and_mechanical_power(self):
        errors = []
        for layers in (4, 8, 16):
            config = example(4, layers)
            config['dt_s'] = 1e-5
            model = FlowingWaterInterface(LIBRARY, config)
            _, width = model.geometry(config['liquid_kg'])
            gap = width * layers
            u, _, dissipation, power, residual = model.flow(width)
            exact = [model.drive * ((i+.5)*width) * (gap-(i+.5)*width) / (2*model.viscosity)
                     for i in range(layers)]
            mean = model.drive * gap**2 / (12 * model.viscosity)
            continuum_power = model.drive * config['area_m2'] * gap * mean
            errors.append(abs(power / continuum_power - 1))
            self.assertLess(max(abs(a-b) for a,b in zip(u,exact)) / mean, 2 / layers**2)
            self.assertLess(abs(residual), power * 1e-12)
            self.assertAlmostEqual(math.fsum(dissipation), power, delta=power * 1e-12)
        print('Stokes continuum power relative errors', errors)
        self.assertAlmostEqual(errors[0] / errors[1], 4., places=8)
        self.assertAlmostEqual(errors[1] / errors[2], 4., places=8)

    def test_periodic_transport_converges_to_translated_cell_averages(self):
        errors = []
        for axial in (12, 24, 48):
            config = example(axial, 4)
            config.update(mass_transfer_velocity_m_s=0., diffusivity_m2_s=0.,
                          conductivity_w_m_k=0., dt_s=.001 * 12 / axial)
            sinc = math.sin(math.pi/axial)/(math.pi/axial)
            def wave(row, shift):
                return sinc * math.cos(math.tau * ((row+.5)/axial - shift))
            config['vapor_density_kg_m3'] = [.012 + .004*wave(row,0)
                for row in range(axial) for _ in range(4)]
            config['gas_temperature_k'] = [298.15 + wave(row,0)
                for row in range(axial) for _ in range(4)]
            model = FlowingWaterInterface(LIBRARY, config)
            model.advance(round(.2/config['dt_s']))
            result = model.report()
            flow = result['channel_flow']
            u = flow['velocity_m_s']
            interface = result['water_interface']
            expected = [wave(row, speed*.2/model.length) for row in range(axial) for speed in u]
            mass_error = max(abs((a-.012)/.004-b)
                             for a,b in zip(interface['vapor_density_kg_m3'],expected))
            heat_error = max(abs(a-298.15-b)
                             for a,b in zip(interface['gas_temperature_k'],expected))
            errors.append(mass_error)
            self.assertLess(heat_error, 1.08*mass_error)
            self.assertLess(abs(interface['energy_residual_j']), 1e-11)
            self.assertGreater(flow['drive_work_j'], 0.)
        print('Axial advection normalized maximum errors', errors)
        self.assertGreater(errors[0]/errors[1], 1.6)
        self.assertGreater(errors[1]/errors[2], 1.7)
        self.assertLess(errors[-1], .18)

    def test_flow_and_phase_exchange_share_conserved_state(self):
        for case, direction in (('evaporation', -1), ('condensation', 1)):
            for bath in (False, True):
                config = dict(self.fixture(case), bath=bath)
                model = FlowingWaterInterface(LIBRARY, config)
                no_flow = FlowingWaterInterface(LIBRARY, dict(config, pressure_gradient_pa_m=0.))
                initial = model.report()
                model.advance(200)
                no_flow.advance(200)
                result = model.report()
                flow = result['channel_flow']
                water = result['water_interface']
                unforced = no_flow.report()['water_interface']
                self.assertGreater(direction * water['displacement_m'], 4e-9)
                self.assertGreater(max(abs(a-b) for a,b in zip(water['vapor_density_kg_m3'],
                                                               unforced['vapor_density_kg_m3'])), 2e-4)
                self.assertNotEqual(flow['velocity_m_s'], initial['channel_flow']['velocity_m_s'])
                self.assertAlmostEqual(water['liquid_kg'] - initial['water_interface']['liquid_kg'],
                                       -(water['vapor_kg'] - initial['water_interface']['vapor_kg']),
                                       delta=1e-17)
                self.assertAlmostEqual(water['internal_energy_j'] - initial['water_interface']['internal_energy_j'],
                                       flow['drive_work_j'] + water['bath_heat_into_chamber_j'],
                                       delta=1e-12)
                self.assertLess(abs(water['mass_residual_kg']), 1e-17)
                self.assertLess(abs(water['energy_residual_j']), 1e-12)
                if bath:
                    self.assertGreater(-direction*water['bath_heat_into_chamber_j'], 0.)
                else:
                    self.assertGreater(direction*(water['liquid_temperature_k'] - 298.15), 0.)
                print(case, 'bath', bath, 'displacement, mass residual, energy residual',
                      water['displacement_m'], water['mass_residual_kg'], water['energy_residual_j'])

    def test_coupled_time_refinement(self):
        results = []
        for dt in (.0012, .0006, .0003, .00015):
            model = FlowingWaterInterface(LIBRARY, dict(self.fixture('evaporation'), dt_s=dt))
            model.advance(round(.03/dt))
            results.append(model.report()['water_interface']['vapor_density_kg_m3'])
        errors = [max(abs(a-b) for a,b in zip(first, second))
                  for first, second in zip(results, results[1:])]
        print('Coupled temporal differences (kg/m³)', errors)
        self.assertGreater(errors[0]/errors[1], 3.8)
        self.assertGreater(errors[1]/errors[2], 3.8)

    def test_reverse_drive_and_periodic_seam(self):
        config = example(12, 4)
        config.update(diffusivity_m2_s=0., conductivity_w_m_k=0., dt_s=.001)
        config['vapor_density_kg_m3'][-4:] = [.016]*4
        for direction in (-1, 1):
            model = FlowingWaterInterface(LIBRARY, dict(config, pressure_gradient_pa_m=direction*.5))
            model.advance(1)
            density = model.report()['water_interface']['vapor_density_kg_m3']
            target = 0 if direction > 0 else -8
            other = -8 if direction > 0 else 0
            self.assertGreater(density[target], .008)
            self.assertAlmostEqual(density[other], .008, delta=1e-16)

    def test_workspace_persistence_and_refusal(self):
        config = self.fixture('evaporation')
        model = FlowingWaterInterface(LIBRARY, config)
        model.advance(10)
        before = packet(model)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'saved.json'
            execute(LIBRARY, dict(token=1, action='save', args=dict(path=path, session=before)))
            reopened = execute(LIBRARY, dict(token=2, action='open', args=dict(path=path)))['session']
        self.assertEqual(before, reopened)
        model.advance(8)
        restored = restore(LIBRARY, reopened)
        restored.advance(8)
        self.assertEqual(packet(model), packet(restored))
        bad = model.checkpoint()
        bad['payload']['state'][-4] += .01
        bad['sha256'] = digest(bad['payload'])
        with self.assertRaisesRegex(ValueError, 'ledger'):
            restored.restore(bad)
        self.assertEqual(packet(model), packet(restored))
        with self.assertRaisesRegex(ValueError, 'axial transport'):
            FlowingWaterInterface(LIBRARY, dict(config, length_m=.001))
        with self.assertRaisesRegex(ValueError, 'Reynolds'):
            FlowingWaterInterface(LIBRARY, dict(config, pressure_gradient_pa_m=4.))

        # Evaporation increases the gap and crosses the Reynolds limit mid-batch.
        seed = FlowingWaterInterface(LIBRARY, config)
        start_re = seed.report()['channel_flow']['reynolds']
        seed.advance(20)
        end_re = seed.report()['channel_flow']['reynolds']
        drive = .5 * 20 / ((start_re + end_re)/2)
        near_limit = FlowingWaterInterface(LIBRARY, dict(config, pressure_gradient_pa_m=drive))
        near_limit.advance(1)
        accepted = packet(near_limit)
        with self.assertRaisesRegex(ValueError, 'Reynolds'):
            near_limit.advance(20)
        self.assertEqual(packet(near_limit), accepted)


if __name__ == '__main__':
    unittest.main()
