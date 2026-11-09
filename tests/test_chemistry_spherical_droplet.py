"""Qualification of curvature, radial phase transport and the energy budget."""
import math
import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_spherical_droplet import R, REFERENCE_K, SphericalDroplet, example
from chemistry_workspace import packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class DropletTests(unittest.TestCase):
    def test_kelvin_equilibrium_and_laplace_pressure(self):
        config = example()
        seed = SphericalDroplet(LIBRARY,config)
        r = (3*config['liquid_kg']/(4*math.pi*seed.rho_liquid))**(1/3)
        jump = 2*config['tension_n_m']/r
        flat = seed.properties(REFERENCE_K).pressure_pa
        curved = flat*math.exp(jump/(seed.rho_liquid*R*REFERENCE_K))
        m = SphericalDroplet(LIBRARY,dict(config,gas_pressure_pa=[curved]*seed.cells))
        m.advance(500)
        result = m.report()
        vapor = result['stefan']
        self.assertAlmostEqual(vapor['liquid_pressure_pa']-vapor['vapor_pressure_pa'],jump,delta=1e-10)
        self.assertLess(max(map(abs,vapor['gas_velocity_m_s'])),1e-12)
        self.assertLess(vapor['evaporated_kg']+vapor['condensed_kg'],1e-24)
        self.assertEqual(result['water_interface']['displacement_m'],0.)
        curved_model = SphericalDroplet(LIBRARY,dict(config,gas_pressure_pa=[flat]*seed.cells))
        flat_model = SphericalDroplet(LIBRARY,dict(config,tension_n_m=0.,gas_pressure_pa=[flat]*seed.cells))
        self.assertGreater(curved_model.report()['stefan']['mass_flux_kg_m2_s'],1e-8)
        self.assertAlmostEqual(flat_model.report()['stefan']['mass_flux_kg_m2_s'],0.,delta=1e-15)

    def test_bidirectional_jump_radius_and_energy(self):
        for condense in (False,True):
            m = SphericalDroplet(LIBRARY,example(condense=condense))
            initial = m.report()
            m.advance(1000)
            result = m.report()
            gas,drop = result['stefan'],result['droplet']
            j,u,s,rho = (gas[k] for k in ('mass_flux_kg_m2_s','vapor_face_velocity_m_s',
                                         'interface_velocity_m_s','vapor_face_density_kg_m3'))
            sign = -1 if condense else 1
            self.assertGreater(sign*j,.002)
            self.assertAlmostEqual(rho*(u-s),j,delta=1e-16)
            self.assertAlmostEqual(-m.rho_liquid*s,j,delta=1e-16)
            self.assertAlmostEqual(gas['liquid_pressure_pa']-gas['vapor_pressure_pa'],
                                   drop['laplace_pressure_pa']+j*j*(1/rho-1/m.rho_liquid),delta=1e-12)
            transfer = gas['evaporated_kg']-gas['condensed_kg']
            expected = (initial['droplet']['radius_m']**3-3*transfer/(4*math.pi*m.rho_liquid))**(1/3)
            self.assertAlmostEqual(drop['radius_m'],expected,delta=2e-18)
            self.assertGreater(-sign*drop['surface_energy_change_j'],5e-14)
            self.assertGreater(max(gas['gas_pressure_pa'])-min(gas['gas_pressure_pa']),.01)
            self.assertLess(abs(gas['mass_residual_kg']),1e-19)
            self.assertLess(abs(gas['momentum_residual_kg_m_s']),1e-23)
            self.assertLess(abs(gas['energy_residual_j']),1e-16)
            gas_energy = math.fsum(m.state[4:-3:3])
            self.assertAlmostEqual(gas_energy+drop['liquid_internal_energy_j']+m.tension*drop['area_m2'],
                                   initial['droplet']['total_energy_j'],delta=1e-16)

    def test_radial_refinement(self):
        values = []
        for cells in (12,24,48,96):
            m = SphericalDroplet(LIBRARY,dict(example(cells),dt_s=4.8e-7/cells))
            m.advance(round(1.6e-5/m.dt))
            report = m.report()
            values.append((report['water_interface']['displacement_m'],
                           report['stefan']['evaporated_kg'],report['stefan']['vapor_pressure_pa']))
        for series in zip(*values):
            errors = [abs(a-b) for a,b in zip(series,series[1:])]
            print('radial refinement differences',errors)
            self.assertGreater(errors[0],errors[1])
            self.assertGreater(errors[1],errors[2])

    def test_exact_restart_and_acoustic_guard(self):
        m = SphericalDroplet(LIBRARY,example())
        m.advance(50)
        restarted = restore(LIBRARY,packet(m))
        restarted.advance(100)
        m.advance(100)
        self.assertEqual(packet(m),packet(restarted))
        with self.assertRaisesRegex(ValueError,'acoustic timestep'):
            SphericalDroplet(LIBRARY,dict(example(),dt_s=1e-6))


if __name__=='__main__':
    unittest.main()
