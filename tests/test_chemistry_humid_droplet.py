"""Independent spherical diffusion and creeping-flow settling controls."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_humid_droplet import HumidDroplet, example
from chemistry_settling_droplet import SettlingDroplet, example as settling_example
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class HumidDropletTests(unittest.TestCase):
    def test_isothermal_finite_shell_analytic_time(self):
        c = dict(example(),isothermal=True,tension_n_m=0.,outer_radius_m=1e-4)
        m = HumidDroplet(LIBRARY,c)
        m.advance(1000)
        r = m.report()['humid_droplet']
        def integral(radius):
            return .5*radius**2-radius**3/(3*c['outer_radius_m'])
        y_surface = m.mass_fraction(m.properties(c['liquid_temperature_k']).pressure_pa)
        coefficient = c['diffusivity_m2_s']*m.gas_density/m.rho*math.log(
            (1-m.y_outer)/(1-y_surface))
        measured = (integral(c['initial_radius_m'])-integral(r['radius_m']))/coefficient
        self.assertAlmostEqual(measured,.1,delta=1e-7)
        self.assertEqual(r['temperature_k'],c['liquid_temperature_k'])
        self.assertGreater(r['bath_heat_into_drop_j'],0.)

    def test_equilibrium_evaporation_condensation_and_conservation(self):
        for rh,direction in ((.5,-1),(1.,0),(1.05,1)):
            config = dict(example(rh),tension_n_m=.072 if direction else 0.)
            m = HumidDroplet(LIBRARY,config)
            m.advance(2000)
            r = m.report()['humid_droplet']
            if direction:
                self.assertGreater(direction*(r['radius_m']-m.radius0),5e-8)
                self.assertGreater(direction*(r['temperature_k']-m.ambient),.5)
                self.assertGreater(-direction*r['net_mass_into_air_kg'],1e-13)
                self.assertGreater(-direction*(r['vapor_mass_fraction'][0]-m.y_outer),0.)
            else:
                self.assertEqual(r['radius_m'],m.radius0)
                self.assertEqual(r['temperature_k'],m.ambient)
            self.assertLess(abs(r['mass_residual_kg']),1e-23)
            self.assertLess(abs(r['energy_residual_j']),1e-18)

    def test_temporal_convergence_and_exact_restart(self):
        radii = []
        for dt in (.0008,.0004,.0002,.0001):
            m = HumidDroplet(LIBRARY,dict(example(),dt_s=dt))
            m.advance(round(.04/dt))
            radii.append(m.report()['humid_droplet']['radius_m'])
        errors = [abs(a-b) for a,b in zip(radii,radii[1:])]
        print('successive radius differences (m)',errors)
        self.assertGreater(errors[0]/errors[1],3.8)
        self.assertGreater(errors[1]/errors[2],3.8)
        restarted = restore(LIBRARY,packet(m))
        m.advance(50)
        restarted.advance(50)
        self.assertEqual(packet(m),packet(restarted))

    def test_settling_analytic_limit_and_coupled_ledgers(self):
        c = settling_example()
        c['exchange'].update(relative_humidity=1.,tension_n_m=0.)
        m = SettlingDroplet(LIBRARY,c)
        elapsed = .01
        m.advance(round(elapsed/m.dt))
        r = m.report()['settling_droplet']
        radius = c['exchange']['initial_radius_m']
        tau = 2*m.exchange.rho*radius**2/(9*m.viscosity)
        terminal = tau*m.gravity*(1-m.exchange.gas_density/m.exchange.rho)
        exact_v = terminal*(-math.expm1(-elapsed/tau))
        exact_z = terminal*(elapsed+tau*math.expm1(-elapsed/tau))
        self.assertAlmostEqual(r['velocity_m_s'],exact_v,delta=2e-8)
        self.assertAlmostEqual(r['downward_displacement_m'],exact_z,delta=2e-10)

        moving = SettlingDroplet(LIBRARY,settling_example())
        moving.advance(500)
        result = moving.report()
        motion, exchange = result['settling_droplet'], result['humid_droplet']
        self.assertGreater(motion['downward_displacement_m'],.0005)
        self.assertLess(motion['velocity_m_s'],terminal)
        self.assertGreater(exchange['net_mass_into_air_kg'],1e-13)
        self.assertGreater(motion['drag_dissipation_j'],0.)
        self.assertLess(abs(motion['momentum_residual_kg_m_s']),1e-24)
        self.assertLess(abs(motion['mechanical_energy_residual_j']),1e-26)
        self.assertEqual(packet(moving),packet(restore(LIBRARY,packet(moving))))
        with self.assertRaisesRegex(ValueError,'creeping-flow'):
            SettlingDroplet(LIBRARY,dict(settling_example(),air_velocity_m_s=.5))


if __name__=='__main__':
    unittest.main()
