"""Independent dispersion, bulk fields, interface work and energy checks."""
import math
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_capillary_interface import CapillaryInterface, example
from chemistry_reactor import digest
from chemistry_workspace import execute, packet, restore


def continuum_omega(config, harmonic):
    k = math.tau*harmonic/config['length_m']
    liquid,gas = config['density_kg_m3']
    h_l,h_g = config['depth_m']
    return math.sqrt(((liquid-gas)*config['gravity_m_s2']*k+config['tension_n_m']*k**3)/
                     (liquid/math.tanh(k*h_l)+gas/math.tanh(k*h_g)))


class CapillaryTests(unittest.TestCase):
    def test_dispersion_vertical_convergence_and_capillary_limit(self):
        for gravity,tension in ((9.81,.072),(0.,.072),(9.81,0.)):
            errors = []
            for layers in (8,16,32):
                config = dict(example(layers),gravity_m_s2=gravity,tension_n_m=tension)
                model = CapillaryInterface(config)
                calculated = model.report()['capillary']['omega_rad_s']
                errors.append(max(abs(a/continuum_omega(config,i)-1) for i,a in enumerate(calculated,1)))
            print('dispersion relative errors',gravity,tension,errors)
            self.assertGreater(errors[0]/errors[1],3.8)
            self.assertGreater(errors[1]/errors[2],3.8)
            self.assertLess(errors[-1],.0015)

    def test_time_convergence_against_independent_modal_reference(self):
        errors = []
        for dt in (.002,.001,.0005):
            config = dict(example(256),dt_s=dt)
            config['modes'] = config['modes'][:1]
            m = CapillaryInterface(config)
            m.advance(round(.06/dt))
            r = m.report()['capillary']
            eta = 8e-5*math.cos(.06*continuum_omega(config,1))
            self.assertLess(abs(r['total_energy_residual_j']),1e-22)
            errors.append(abs(m.state[0][0]-eta)/8e-5)
        print('elevation temporal errors',errors)
        self.assertGreater(errors[0]/errors[1],3.9)
        self.assertGreater(errors[1]/errors[2],3.7)

    def test_bulk_velocity_and_wall_interface_conditions(self):
        errors = []
        for layers in (8,16,32):
            m = CapillaryInterface(example(layers))
            m.advance(100)
            r = m.report()['capillary']
            x = .5*m.length/m.nx
            speed = r['normal_velocity_m_s'][0]
            error = []
            for j,depth in enumerate(m.depth):
                self.assertGreater(abs(m.sample(j,x,depth/2)[0]),1e-6)
                error.extend((abs(m.sample(j,x,0.)[1]),abs(m.sample(j,x,depth)[1]-speed)))
            errors.append(max(error))
        print('strong wall/interface gradient errors (m/s)',errors)
        self.assertGreater(errors[0]/errors[1],1.8)
        self.assertGreater(errors[1]/errors[2],1.8)

    def test_normal_flux_and_bulk_momentum(self):
        model = CapillaryInterface(example())
        for k,profiles,_,_,_ in model.modes:
            for depth,profile in zip(model.depth,profiles):
                # Integrate continuity from the rigid wall to the interface.
                dz = depth/model.layers
                integrated_flux = k*k*dz*(.5*(profile[0]+profile[-1])+math.fsum(profile[1:-1]))
                self.assertAlmostEqual(integrated_flux,1.,delta=2e-13)
        model.advance(100)
        for phase,depth in enumerate(model.depth):
            x = .0073
            z = 10.3*depth/model.layers
            epsilon = 1e-7
            sign = 1. if phase==0 else -1.
            previous = model.sample(phase,x,z)
            def gradient():
                px = (model.sample(phase,x+epsilon,z)[2]-model.sample(phase,x-epsilon,z)[2])/(2*epsilon)
                pz = sign*(model.sample(phase,x,z+epsilon)[2]-model.sample(phase,x,z-epsilon)[2])/(2*epsilon)
                return px,pz
            before = gradient()
            model.advance(1)
            after = gradient()
            for u0,u1,g0,g1 in zip(previous,model.sample(phase,x,z),before,after):
                self.assertAlmostEqual((u1-u0)/model.dt,-.5*(g0+g1)/model.density[phase],delta=1e-9)

    def test_pressure_traction_power_and_independent_bulk_energy(self):
        m = CapillaryInterface(example())
        m.advance(100)
        r = m.report()['capillary']
        self.assertLess(r['max_jump_residual_pa'],1e-12)
        self.assertGreater(max(map(abs,r['capillary_jump_pa'])),.01)
        self.assertGreater(max(abs(pair[1]) for pair in r['interface_pressure_pa']),1e-4)
        total_pressure_power = 0.
        for phase,depth in enumerate(m.depth):
            sign = 1 if phase==0 else -1
            pressure_power = 0.
            bulk_energy = 0.
            dz = depth/m.layers
            for ix in range(m.nx):
                x = (ix+.5)*m.length/m.nx
                pressure_power -= sign*m.sample(phase,x,depth)[2]*r['normal_velocity_m_s'][ix]
                for iz in range(m.layers):
                    for node in (-1/math.sqrt(3),1/math.sqrt(3)):
                        u,w,_ = m.sample(phase,x,(iz+.5+.5*node)*dz)
                        bulk_energy += u*u+w*w
            power = pressure_power*m.area/m.nx
            total_pressure_power += power
            self.assertAlmostEqual(power,r['kinetic_power_w'][phase],delta=1e-19)
            self.assertAlmostEqual(.25*m.density[phase]*bulk_energy*dz*m.area/m.nx,
                                   r['kinetic_j'][phase],delta=1e-20)
        potential_power = m.area/m.nx*math.fsum(
            ((m.density[0]-m.density[1])*m.gravity*h+cap)*w
            for h,cap,w in zip(r['elevation_m'],r['capillary_jump_pa'],r['normal_velocity_m_s']))
        self.assertAlmostEqual(total_pressure_power+potential_power,0.,delta=1e-19)
        print('pressure power and potential power (W)',r['kinetic_power_w'],potential_power)

    def test_mass_energy_and_exact_workspace_restart(self):
        m = CapillaryInterface(example())
        for steps in (1,100,1000,9000):
            m.advance(steps)
            r = m.report()['capillary']
            self.assertLess(max(map(abs,r['mass_residual_kg'])),1e-16)
            self.assertLess(abs(r['total_energy_residual_j']),1e-22)
        before = packet(m)
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)/'saved.json'
            execute(None,dict(token=1,action='save',args=dict(path=path,session=before)))
            reopened = execute(None,dict(token=2,action='open',args=dict(path=path)))['session']
        self.assertEqual(before,reopened)
        restarted = restore(None,reopened)
        restarted.advance(100)
        m.advance(100)
        self.assertEqual(packet(m),packet(restarted))
        bad = m.checkpoint()
        bad['payload']['tick'] = -1
        bad['sha256'] = digest(bad['payload'])
        with self.assertRaisesRegex(ValueError,'identity'):
            m.restore(bad)
        with self.assertRaisesRegex(ValueError,'step count'):
            m.advance(10001)
        self.assertEqual(packet(m),packet(restarted))
        with self.assertRaisesRegex(ValueError,'timestep'):
            CapillaryInterface(dict(example(),dt_s=.02))


if __name__=='__main__':
    unittest.main()
