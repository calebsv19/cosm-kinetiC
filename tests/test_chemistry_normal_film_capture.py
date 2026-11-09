"""Normal drag feedback, wall impulse and conservation through film capture."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_normal_film_capture import NormalFilmCapture, example
from chemistry_reactor import digest
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class NormalFilmCaptureTests(unittest.TestCase):
    def test_local_normal_exchange_and_gas_feedback(self):
        model = NormalFilmCapture(LIBRARY,example())
        state = model.state
        rate = model.derivative(state)
        dz = model.drop_normal
        m = state[model.film_end]
        radius = model.config['droplet']['radius_m']
        beta = 6*math.pi*model.flight.viscosity*radius
        speed = state[dz]/m
        expected_force = -beta*speed
        self.assertAlmostEqual(-rate[dz+3],expected_force,delta=1e-24)
        self.assertAlmostEqual(rate[dz],expected_force+speed*rate[model.film_end],delta=1e-24)
        self.assertAlmostEqual(math.fsum(rate[model.normal_start:dz]),-rate[dz],delta=1e-24)
        self.assertAlmostEqual(rate[model.film_end]+math.fsum(rate[1:model.gas_end:4])+
                               math.fsum(rate[model.gas_end:model.gas_end+model.film.cells]),0.,delta=1e-25)
        opposing = NormalFilmCapture(LIBRARY,dict(example(),gas_normal_velocity_m_s=.04))
        following = NormalFilmCapture(LIBRARY,dict(example(),gas_normal_velocity_m_s=-.04))
        for item in (opposing, model, following):
            item.advance(1000)
        times = [item.report()['film_capture']['capture_time_s'] for item in (opposing, model, following)]
        self.assertGreater(times[0], times[1]+1e-6)
        self.assertGreater(times[1], times[2]+1e-6)
        print('opposing, still and following gas impact times', times)

    def test_capture_transfer_conservation_and_restart(self):
        model = NormalFilmCapture(LIBRARY,example())
        model.advance(833)
        self.assertGreater(model.clearance(model.state),0.)
        end = model.integrate(model.state,model.dt)
        self.assertLess(model.clearance(end),0.)
        resumed = restore(LIBRARY,packet(model))
        # Audit the instantaneous transaction on the bracketed step state.
        capture = model.capture(end,834*model.dt)
        pz = end[model.drop_normal]
        normal_ke = .5*pz*pz/end[model.film_end]
        self.assertEqual(capture[:model.gas_end],end[:model.gas_end])
        self.assertEqual(capture[model.normal_start:model.drop_normal],end[model.normal_start:model.drop_normal])
        self.assertEqual(capture[model.drop_normal],0.)
        self.assertEqual(capture[model.drop_normal+1],pz)
        energy = end[model.film_end+2]+normal_ke
        energy_start = model.gas_end+model.film.cells
        self.assertAlmostEqual(math.fsum(b-a for a,b in zip(end[energy_start:model.film_end-3],
                               capture[energy_start:model.film_end-3])),energy,delta=1e-23)
        model.advance(1)
        resumed.advance(1)
        self.assertEqual(packet(resumed),packet(model))
        impact = model.report()
        model.advance(2166)
        report = model.report()
        normal = report['normal_momentum']
        event = report['film_capture']
        self.assertLess(normal['impact_impulse_kg_m_s'],0.)
        self.assertLess(normal['drag_impulse_into_gas_kg_m_s'],0.)
        self.assertLess(normal['phase_impulse_into_gas_kg_m_s'],0.)
        self.assertLess(normal['wall_traction_impulse_kg_m_s'],impact['normal_momentum']['wall_traction_impulse_kg_m_s'])
        self.assertLess(abs(normal['gas_momentum_kg_m_s']),abs(impact['normal_momentum']['gas_momentum_kg_m_s']))
        self.assertLess(abs(normal['residual_kg_m_s']),1e-25)
        self.assertLess(abs(normal['gas_exchange_residual_kg_m_s']),1e-25)
        self.assertLess(abs(event['water_residual_kg']),1e-23)
        self.assertLess(abs(event['energy_residual_j']),1e-18)
        self.assertGreater(event['film_evaporated_after_impact_kg'],5e-16)
        first,final = (r['wall_film']['height_m'] for r in (impact,report))
        self.assertLess(final[12],first[12])
        self.assertGreater(final[11],first[11])
        self.assertGreater(final[13],first[13])
        saved = packet(model)
        self.assertEqual(packet(restore(LIBRARY,saved)),saved)
        bad = packet(model)['checkpoint']
        bad['payload']['state'][model.drop_normal+2] += 1e-15
        bad['sha256'] = digest(bad['payload'])
        with self.assertRaisesRegex(ValueError,'conservation ledger'):
            model.restore(bad)
        self.assertEqual(packet(model),saved)

    def test_timestep_refinement(self):
        times = []
        for dt in (1e-7,5e-8,2.5e-8):
            config = example()
            config['film']['carrier']['dt_s'] = dt
            model = NormalFilmCapture(LIBRARY,config)
            model.advance(round(45e-6/dt))
            times.append(model.report()['film_capture']['capture_time_s'])
        errors = [abs(a-b) for a,b in zip(times,times[1:])]
        print('impact time step differences (s)',errors)
        self.assertGreater(errors[0],3.5*errors[1])
        self.assertLess(errors[1],1e-11)


if __name__ == '__main__':
    unittest.main()
