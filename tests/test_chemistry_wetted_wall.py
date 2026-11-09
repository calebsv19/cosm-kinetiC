"""Independent hemisphere, signed wall exchange and wetting geometry checks."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_droplet_deposition import DropletDeposition
from chemistry_humid_droplet import AIR_R
from chemistry_stefan_interface import R
from chemistry_wetted_wall import WettedWall, cap_diffusion_factor, example
from chemistry_workspace import packet,restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class WettedWallTests(unittest.TestCase):
    def test_hemisphere_flux_and_energy_source(self):
        m = WettedWall(LIBRARY,example())
        m.advance(300)
        self.assertTrue(m.report()['wall_deposition']['captured'])
        j = m.core.gas_end
        data = m.report()['wetted_wall']
        weights = m.core.stencil(m.core.length)
        gas = m.core.gas_fields(m.state)
        rho = math.fsum(gas[i][0]*w for i,w in weights)
        p = math.fsum(gas[i][3]*w for i,w in weights)
        y = math.fsum(gas[i][4]*w for i,w in weights)
        pv = data['saturation_pressure_pa']
        ys = (pv/R)/(pv/R+(p-pv)/AIR_R)
        expected = (2*math.pi*data['contact_radius_m']*m.core.exchange.config['diffusivity_m2_s']
                    *rho*math.log((1-y)/(1-ys)))
        rates = m.derivative(m.state)
        base = DropletDeposition.derivative(m,m.state)
        count = m.core.counts[0]
        self.assertAlmostEqual(-rates[j],expected,delta=1e-23)
        self.assertAlmostEqual(math.fsum(a-b for a,b in zip(rates[1:j:4],base[1:j:4])),
                               count*expected,delta=1e-22)
        self.assertAlmostEqual(math.fsum(a-b for a,b in zip(rates[3:j:4],base[3:j:4])),
                               -count*rates[j+2],delta=1e-15)
        self.assertEqual(rates[2:j:4],base[2:j:4])

    def test_exchange_both_directions_and_restart(self):
        for humidity,sign in ((.6,-1),(1.15,1)):
            c = example()
            c['carrier']['gas_relative_humidity'] = [humidity]*c['n'][0]
            m = WettedWall(LIBRARY,c)
            m.advance(300)
            before = m.report()
            clone = restore(LIBRARY,packet(m))
            m.advance(1500)
            clone.advance(1500)
            self.assertEqual(packet(m),packet(clone))
            after = m.report()
            delta = after['wall_deposition']['deposited_water_kg']-before['wall_deposition']['deposited_water_kg']
            self.assertGreater(sign*delta,1e-14)
            gas0 = before['droplet_carrier']['gas_vapor_kg']
            gas1 = after['droplet_carrier']['gas_vapor_kg']
            self.assertAlmostEqual(math.fsum(b-a for a,b in zip(gas0,gas1)),-delta,delta=1e-23)
            self.assertLess(abs(after['droplet_carrier']['energy_residual_j']),1e-16)
            self.assertEqual(after['wall_deposition']['impact_heat_j'],before['wall_deposition']['impact_heat_j'])

    def test_prescribed_angle_geometry_and_comparison(self):
        self.assertAlmostEqual(cap_diffusion_factor(math.radians(89.999)),2.,delta=2e-5)
        footprints = []
        evaporated = []
        for angle in (60.,90.,120.):
            m = WettedWall(LIBRARY,example(angle))
            m.advance(300)
            before = m.report()['wall_deposition']['deposited_water_kg']
            m.advance(1000)
            r = m.report()
            cap = r['wetted_wall']
            a = cap['contact_radius_m']
            h = a*math.tan(math.radians(angle)/2)
            volume = math.pi*h*(3*a*a+h*h)/6
            mass = r['wall_deposition']['deposited_water_kg']/m.core.counts[0]
            self.assertAlmostEqual(volume,mass/m.core.exchange.rho,delta=1e-28)
            footprints.append(cap['footprint_m2'])
            evaporated.append(before-r['wall_deposition']['deposited_water_kg'])
        self.assertGreater(footprints[0],footprints[1])
        self.assertGreater(footprints[1],footprints[2])
        self.assertGreater(max(evaporated)-min(evaporated),1e-16)


if __name__=='__main__':
    unittest.main()
