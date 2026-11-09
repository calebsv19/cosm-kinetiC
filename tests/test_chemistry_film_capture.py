"""Event conservation and single-run flight, capture and film evolution."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_film_capture import FilmCapture, example
from chemistry_wall_film import WallFilm
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class FilmCaptureTests(unittest.TestCase):
    def test_event_conserves_inventory_and_crossing_restarts(self):
        model = FilmCapture(LIBRARY, example())
        model.advance(750)
        self.assertGreater(model.clearance(model.state), 0.)
        self.assertLess(model.clearance(model.integrate(model.state, model.dt)), 0.)
        resumed = restore(LIBRARY, packet(model))
        # Inspect the state on both sides of the instantaneous transfer.
        low, high = 0., model.dt
        for _ in range(30):
            mid = .5 * (low + high)
            if model.clearance(model.integrate(model.state, mid)) > 0.:
                low = mid
            else:
                high = mid
        pre = model.integrate(model.state, high)
        post = model.capture(pre, model.tick * model.dt + high)
        self.assertEqual(pre[:model.gas_end], post[:model.gas_end])
        n = model.film.cells
        amount = pre[model.film_end]
        energy = pre[model.film_end+2] + amount * model.normal_specific_ke
        mass_change = math.fsum(b-a for a,b in zip(pre[model.gas_end:model.gas_end+n],
                                                 post[model.gas_end:model.gas_end+n]))
        energy_change = math.fsum(b-a for a,b in zip(pre[model.gas_end+n:model.film_end-3],
                                                   post[model.gas_end+n:model.film_end-3]))
        self.assertAlmostEqual(mass_change, amount, delta=1e-26)
        self.assertAlmostEqual(energy_change, energy, delta=1e-22)
        self.assertEqual(post[model.film_end:model.film_end+3], [0.,0.,0.])
        self.assertEqual(post[-2], pre[model.film_end+1])
        model.advance(30)
        resumed.advance(30)
        self.assertEqual(packet(resumed), packet(model))
        self.assertTrue(model.report()['film_capture']['captured'])

    def test_capture_spreading_and_evaporation(self):
        config = example()
        model = FilmCapture(LIBRARY, config)
        control = WallFilm(LIBRARY, config['film'])
        maximum = [0.,0.,0.]
        history = []
        for count in (500,250,1,29,220,2000):
            model.advance(count)
            report = model.report()
            event = report['film_capture']
            history.append(report)
            for i,field in enumerate(('water_residual_kg','energy_residual_j','tangential_momentum_residual_kg_m_s')):
                maximum[i] = max(maximum[i],abs(event[field]))
        before, impact, end = history[1], history[2], history[-1]
        self.assertFalse(before['film_capture']['captured'])
        self.assertTrue(impact['film_capture']['captured'])
        self.assertGreater(impact['film_capture']['airborne_evaporated_kg'],0.)
        self.assertEqual(end['film_capture']['captured_water_kg'],impact['film_capture']['captured_water_kg'])
        self.assertEqual(end['film_capture']['airborne_water_kg'],0.)
        self.assertGreater(end['film_capture']['film_evaporated_after_impact_kg'],0.)
        height0,height1 = (r['wall_film']['height_m'] for r in (impact,end))
        self.assertLess(height1[12],height0[12])
        self.assertGreater(height1[11],height0[11])
        self.assertGreater(height1[13],height0[13])
        control.advance(3000)
        self.assertGreater(end['wall_film']['evaporated_kg'],control.report()['wall_film']['evaporated_kg'])
        self.assertLess(maximum[0],1e-23)
        self.assertLess(maximum[1],1e-18)
        self.assertLess(maximum[2],1e-24)
        self.assertEqual(packet(restore(LIBRARY,packet(model))),packet(model))
        print('capture and final', impact['film_capture'], end['film_capture'], 'residual maxima', maximum)

    def test_impact_time_and_postimpact_height_under_refinement(self):
        times, peaks = [], []
        for dt in (1e-7,5e-8,2.5e-8):
            config = example()
            config['film']['carrier']['dt_s'] = dt
            model = FilmCapture(LIBRARY, config)
            model.advance(round(4e-5/dt))
            report = model.report()
            times.append(report['film_capture']['capture_time_s'])
            peaks.append(max(report['wall_film']['height_m']))
        print('capture times and postimpact peaks', times, peaks)
        # Event location is already near the time-bracket precision.
        self.assertLess(max(times)-min(times),1e-12)
        self.assertLess(abs(peaks[1]-peaks[2]),abs(peaks[0]-peaks[1]))
        self.assertLess(abs(peaks[1]-peaks[2]),1e-12)


if __name__ == '__main__':
    unittest.main()
