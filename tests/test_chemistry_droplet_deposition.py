"""Fixed physical source support and conservative absorbing wall tests."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_droplet_deposition import DropletDeposition, example
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class DepositionTests(unittest.TestCase):
    def test_fixed_width_and_wall_clipping(self):
        for cells in (12,24,48,96):
            c = example(cells,False)
            c['carrier']['dt_s'] = 1e-7
            m = DropletDeposition(LIBRARY,c).core
            weights = dict(m.stencil(0.))
            self.assertAlmostEqual(math.fsum(weights.values()),1.)
            for i,w in weights.items():
                self.assertAlmostEqual(w,weights[cells-1-i],delta=1e-14)
                self.assertAlmostEqual(w*c['source_width_m'],
                    min(m.dx,max(0.,.5*c['source_width_m']-min(i,cells-1-i)*m.dx)),delta=1e-15)
        wall = DropletDeposition(LIBRARY,example()).core
        clipped = dict(wall.stencil(0.))
        self.assertEqual(set(clipped),{0,1})
        self.assertAlmostEqual(clipped[0],2/3)

    def test_capture_conserves_and_freezes_inventory(self):
        m = DropletDeposition(LIBRARY,example())
        m.advance(200)
        self.assertFalse(m.report()['wall_deposition']['captured'])
        pre = restore(LIBRARY,packet(m))
        m.advance(100)
        pre.advance(100)
        self.assertEqual(packet(m),packet(pre))
        result = m.report()
        d = result['wall_deposition']
        self.assertTrue(d['captured'])
        self.assertEqual(d['side'],1)
        self.assertEqual(result['droplet_carrier']['parcels'],[])
        self.assertGreater(d['deposited_water_kg'],1e-11)
        self.assertGreater(d['impact_impulse_kg_m_s'],0.)
        self.assertAlmostEqual(d['impact_heat_j'],.5*d['impact_impulse_kg_m_s']**2/d['deposited_water_kg'],
                               delta=1e-27)
        self.assertGreater(d['capture_time_s'],.0001)
        self.assertLess(d['capture_time_s'],.00015)
        self.assertEqual(packet(m),packet(restore(LIBRARY,packet(m))))
        m.advance(500)
        after = m.report()['wall_deposition']
        for key in ('deposited_water_kg','deposited_total_energy_j','impact_heat_j','capture_time_s'):
            self.assertEqual(after[key],d[key])
        for residual,bound in zip(m.residuals(m.state),(1e-21,1e-22,1e-22,1e-16)):
            self.assertLess(abs(residual),bound)

    def test_reflected_impact_and_timestep(self):
        times = []
        for dt in (5e-7,2.5e-7,1.25e-7):
            c = example()
            c['carrier']['dt_s'] = dt
            c['carrier']['droplets'][0].update(position_m=11e-6,velocity_m_s=-.008)
            m = DropletDeposition(LIBRARY,c)
            m.advance(round(.00015/dt))
            d = m.report()['wall_deposition']
            self.assertEqual(d['side'],-1)
            self.assertLess(d['impact_impulse_kg_m_s'],0.)
            times.append(d['capture_time_s'])
        print('left capture times (s)',times)
        self.assertLess(max(times)-min(times),1e-8)


if __name__=='__main__':
    unittest.main()
