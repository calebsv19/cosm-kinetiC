"""Contact-line geometry, force, dissipation and conservative restart."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_dynamic_wetting import DynamicWetting,example
from chemistry_workspace import packet,restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class DynamicWettingTests(unittest.TestCase):
    def test_young_force_matches_surface_gradient_and_heat(self):
        m = DynamicWetting(LIBRARY,dict(example(),exchange_enabled=False))
        m.advance(300)
        before = m.report()['dynamic_wetting']
        state = m.state
        radius = state[-2]
        epsilon = 1e-10
        def surface(a):
            sample = [*state[:-2],a,state[-1]]
            view = m.stage(sample)
            return view.cap(sample[m.j])[3]*m.wall.core.counts[0]
        gradient = (surface(radius+epsilon)-surface(radius-epsilon))/(2*epsilon)
        self.assertAlmostEqual(-gradient*before['contact_speed_m_s'],m.derivative(state)[-1],delta=1e-16)
        m.advance(1500)
        after = m.report()['dynamic_wetting']
        released = before['surface_energy_j']-after['surface_energy_j']
        self.assertGreater(released,1e-11)
        self.assertAlmostEqual(released,after['contact_line_heat_j']-before['contact_line_heat_j'],delta=1e-16)
        self.assertEqual(state[m.j],m.state[m.j])
        self.assertGreater(after['temperature_k'],before['temperature_k'])

    def test_spreading_receding_and_pinned(self):
        for initial,young,mobility,direction in ((115.,65.,.2,1),(65.,115.,.2,-1),(115.,65.,0.,0)):
            m = DynamicWetting(LIBRARY,dict(example(),initial_angle_deg=initial,
                                            equilibrium_angle_deg=young,mobility_m2_n_s=mobility))
            m.advance(300)
            start = m.report()['dynamic_wetting']
            m.advance(1200)
            result = m.report()
            end = result['dynamic_wetting']
            if direction:
                self.assertGreater(direction*(end['contact_radius_m']-start['contact_radius_m']),1e-6)
                self.assertLess(abs(end['contact_angle_deg']-young),abs(start['contact_angle_deg']-young))
                self.assertGreater(end['contact_line_heat_j'],0.)
            else:
                self.assertEqual(end['contact_radius_m'],start['contact_radius_m'])
                self.assertEqual(end['contact_line_heat_j'],0.)
                self.assertLess(end['contact_angle_deg'],start['contact_angle_deg'])
            self.assertGreater(end['vapor_flux_into_gas_kg_s'],0.)
            self.assertLess(abs(result['droplet_carrier']['water_residual_kg']),1e-22)
            self.assertLess(abs(result['droplet_carrier']['energy_residual_j']),1e-16)

    def test_hysteresis_holds_and_limits_contact_motion(self):
        pinned = DynamicWetting(LIBRARY,dict(example(),initial_angle_deg=70.,
                                             hysteresis_half_width_deg=10.,exchange_enabled=False))
        pinned.advance(1000)
        r = pinned.report()['dynamic_wetting']
        self.assertAlmostEqual(r['contact_angle_deg'],70.,delta=1e-12)
        self.assertEqual(r['contact_speed_m_s'],0.)
        self.assertEqual(r['contact_line_heat_j'],0.)
        advancing = DynamicWetting(LIBRARY,dict(example(),hysteresis_half_width_deg=10.,
                                                 exchange_enabled=False))
        advancing.advance(4000)
        angle = advancing.report()['dynamic_wetting']['contact_angle_deg']
        self.assertGreater(angle,75.)
        self.assertLess(angle,79.)

    def test_timestep_refinement_and_restart(self):
        radii = []
        for dt in (5e-7,2.5e-7,1.25e-7):
            c = example()
            c['wall']['carrier']['dt_s'] = dt
            m = DynamicWetting(LIBRARY,c)
            m.advance(round(.0004/dt))
            radii.append(m.report()['dynamic_wetting']['contact_radius_m'])
        print('successive contact radius differences (m)',[abs(a-b) for a,b in zip(radii,radii[1:])])
        self.assertGreater(abs(radii[0]-radii[1])/abs(radii[1]-radii[2]),3.5)
        other = restore(LIBRARY,packet(m))
        m.advance(20)
        other.advance(20)
        self.assertEqual(packet(m),packet(other))


if __name__=='__main__':
    unittest.main()
