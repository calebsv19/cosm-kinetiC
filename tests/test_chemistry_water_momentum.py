"""U-tube momentum, moving mesh transport, and conservation witnesses."""
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_reactor import digest
from chemistry_water_momentum import GRAVITY, WaterMomentum, example
from chemistry_workspace import execute, packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


def isolate(config, dt, damping=0.):
    config['damping_per_s'] = damping
    for column in config['columns']:
        column.update(dt_s=dt, mass_transfer_velocity_m_s=0., conductivity_w_m_k=0.,
                      diffusivity_m2_s=0.)
    return config


class MomentumTests(unittest.TestCase):
    def test_independent_damped_oscillator_and_refinement(self):
        for damping in (0., 1.5):
            errors = []
            for dt in (.008, .004, .002):
                config = isolate(example(3), dt, damping)
                model = WaterMomentum(LIBRARY, config)
                initial = model.report()['hydraulic']
                amplitude = (initial['columns'][0]['position_m'] -
                             initial['columns'][1]['position_m']) / 2
                omega = math.sqrt(2*GRAVITY/config['inertial_length_m'])
                damped = math.sqrt(omega**2 - (.5*damping)**2)
                t = .24
                model.advance(round(t/dt))
                r = model.report()['hydraulic']
                exact = amplitude*math.exp(-.5*damping*t)*(
                    math.cos(damped*t)+.5*damping/damped*math.sin(damped*t))
                exact_flow = (model.mass_per_m*amplitude*math.exp(-.5*damping*t) *
                              omega**2/damped*math.sin(damped*t))
                actual = (r['columns'][0]['position_m']-r['columns'][1]['position_m'])/2
                errors.append(max(abs(actual-exact)/amplitude,
                                  abs(r['flow_kg_s']-exact_flow)/(model.mass_per_m*amplitude*omega)))
                self.assertLess(abs(r['total_energy_residual_j']), 1e-12)
                self.assertLess(abs(r['total_water_residual_kg']), 1e-15)
                self.assertAlmostEqual(r['internal_energy_j']-initial['internal_energy_j'],
                                       r['drag_heat_j'], delta=1e-12)
            print('oscillator normalized errors', damping, errors)
            self.assertGreater(errors[0]/errors[1], 3.9)
            self.assertGreater(errors[1]/errors[2], 3.9)
            self.assertLess(errors[-1], .0002)

    def test_moving_mesh_and_liquid_donor_transport(self):
        config = isolate(example(3), .001)
        config['columns'][0]['liquid_temperature_k'] = 300.15
        model = WaterMomentum(LIBRARY, config)
        start = model.report()['hydraulic']
        model.advance(100)
        end = model.report()['hydraulic']
        for before, after in zip(start['columns'],end['columns']):
            initial_gap = before['edges_m'][-1]-before['position_m']
            final_gap = after['edges_m'][-1]-after['position_m']
            for initial_density, final_density in zip(before['vapor_density_kg_m3'],
                                                       after['vapor_density_kg_m3']):
                self.assertAlmostEqual(initial_gap*initial_density, final_gap*final_density, delta=1e-16)
        right_mass = end['columns'][1]['liquid_kg']
        self.assertAlmostEqual(end['columns'][1]['liquid_temperature_k'],
                               298.15 + 2*end['transfer_kg']/right_mass, delta=1e-11)
        self.assertAlmostEqual(end['columns'][0]['liquid_temperature_k'],300.15, delta=1e-11)

    def test_simultaneous_exchange_and_flow_closure(self):
        model = WaterMomentum(LIBRARY, example())
        initial = model.report()['hydraulic']
        model.advance(500)
        r = model.report()['hydraulic']
        left, right = r['columns']
        self.assertGreater(left['evaporated_kg'], 4e-7)
        self.assertGreater(right['condensed_kg'], 3e-7)
        self.assertGreater(abs(left['position_m']-initial['columns'][0]['position_m']), 1e-4)
        self.assertGreater(r['drag_heat_j'], 0.)
        control_config = example()
        for column in control_config['columns']:
            column['mass_transfer_velocity_m_s'] = 0.
        control = WaterMomentum(LIBRARY, control_config)
        control.advance(500)
        feedback = r['flow_kg_s']-control.report()['hydraulic']['flow_kg_s']
        self.assertGreater(abs(feedback), 1e-8)
        print('phase feedback on hydraulic flow (kg/s)', feedback)
        for i, column in enumerate(r['columns']):
            transfer = r['transfer_kg'] * (-1 if i==0 else 1)
            change = column['liquid_kg']-initial['columns'][i]['liquid_kg']
            self.assertAlmostEqual(change, transfer-column['evaporated_kg']+column['condensed_kg'],
                                   delta=1e-15)
            self.assertLess(abs(r['column_water_residual_kg'][i]), 1e-15)
        self.assertLess(abs(r['total_energy_residual_j']), 1e-12)
        print('coupled 0.5 s report', {k:v for k,v in r.items() if k!='columns'})

    def test_coupled_time_refinement(self):
        solutions = []
        for dt in (.004, .002, .001, .0005):
            config = example()
            for column in config['columns']:
                column['dt_s'] = dt
            model = WaterMomentum(LIBRARY, config)
            model.advance(round(.2/dt))
            solutions.append(model.report()['hydraulic']['columns'][0]['vapor_density_kg_m3'])
        errors = [max(abs(a-b) for a,b in zip(first,second))
                  for first,second in zip(solutions,solutions[1:])]
        print('coupled vapor refinement differences', errors)
        self.assertGreater(errors[0]/errors[1], 3.8)
        self.assertGreater(errors[1]/errors[2], 3.8)

    def test_spatial_phase_transport_refinement(self):
        extents = []
        for cells in (4, 8, 16, 32):
            config = example(cells)
            for column in config['columns']:
                column['dt_s'] = .00025
            model = WaterMomentum(LIBRARY, config)
            model.advance(400)
            left, right = model.report()['hydraulic']['columns']
            extents.append((left['evaporated_kg'], right['condensed_kg']))
        for direction in zip(*extents):
            differences = [abs(a-b) for a,b in zip(direction,direction[1:])]
            print('moving mesh extent refinement (kg)', differences)
            self.assertGreater(differences[0]/differences[1], 2.)
            self.assertGreater(differences[1]/differences[2], 3.5)

    def test_workspace_restart_and_atomic_refusal(self):
        model = WaterMomentum(LIBRARY, example())
        model.advance(25)
        before = packet(model)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'session.json'
            execute(LIBRARY,dict(token=1,action='save',args=dict(path=path,session=before)))
            reopened = execute(LIBRARY,dict(token=2,action='open',args=dict(path=path)))['session']
        self.assertEqual(before,reopened)
        model.advance(50)
        restored = restore(LIBRARY,reopened)
        restored.advance(50)
        self.assertEqual(packet(model),packet(restored))
        bad = restored.checkpoint()
        bad['payload']['state'][-3] *= 1.1
        bad['sha256'] = digest(bad['payload'])
        with self.assertRaisesRegex(ValueError,'energy ledger'):
            restored.restore(bad)
        self.assertEqual(packet(model),packet(restored))
        with self.assertRaisesRegex(ValueError,'hydraulic timestep'):
            WaterMomentum(LIBRARY,isolate(example(3),.02))
        # The fast initial flow will push the right surface above its admitted ceiling.
        config = isolate(example(3),.001)
        config['columns'][0]['liquid_kg'] = .18
        config['columns'][1]['liquid_kg'] = .24
        config['initial_flow_kg_s'] = 3.
        for column in config['columns']:
            column['vapor_density_kg_m3'] = [0.] * 3
        unstable = WaterMomentum(LIBRARY,config)
        unstable.advance(1)
        accepted = packet(unstable)
        with self.assertRaisesRegex(ValueError,'headspace'):
            unstable.advance(35)
        self.assertEqual(accepted,packet(unstable))


if __name__=='__main__':
    unittest.main()
