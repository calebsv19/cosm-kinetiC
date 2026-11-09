"""Independent acoustic, interface jump and conservation checks."""
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_reactor import digest
from chemistry_stefan_interface import GAMMA, R, REFERENCE_K, StefanInterface, example
from chemistry_workspace import execute, packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class StefanTests(unittest.TestCase):
    def test_saturated_equilibrium(self):
        config = example()
        seed = StefanInterface(LIBRARY,config)
        p_sat = seed.properties(REFERENCE_K).pressure_pa
        config['gas_pressure_pa'] = [p_sat]*seed.cells
        model = StefanInterface(LIBRARY,config)
        model.advance(200)
        result = model.report()['stefan']
        self.assertLess(max(map(abs,result['gas_velocity_m_s'])),1e-12)
        self.assertLess(result['evaporated_kg']+result['condensed_kg'],1e-18)

    def test_sealed_acoustic_wave_refinement(self):
        errors = []
        for cells in (16,32,64):
            config = example(cells)
            seed = StefanInterface(LIBRARY,config)
            _,dx = seed.geometry(config['liquid_kg'])
            width = cells*dx
            sound = math.sqrt(GAMMA*R*REFERENCE_K)
            duration = width/sound
            steps = 4*cells
            config.update(dt_s=duration/steps,permeance_kg_m2_s_pa=0.)
            p0,amp = 3170.,.1
            scale = math.sin(math.pi/(2*cells))/(math.pi/(2*cells))
            wave = [amp*scale*math.cos(math.pi*(i+.5)/cells) for i in range(cells)]
            config['gas_pressure_pa'] = [p0+d for d in wave]
            config['gas_temperature_k'] = [REFERENCE_K*((p0+d)/p0)**(1-1/GAMMA) for d in wave]
            model = StefanInterface(LIBRARY,config)
            model.advance(steps)
            result = model.report()['stefan']
            errors.append(max(abs(a-(p0-d)) for a,d in zip(result['gas_pressure_pa'],wave))/amp)
            self.assertLess(abs(result['momentum_residual_kg_m_s']),1e-15)
        print('acoustic standing-wave normalized pressure errors',errors)
        self.assertGreater(errors[0]/errors[1],1.7)
        self.assertGreater(errors[1]/errors[2],1.7)
        self.assertLess(errors[-1],.09)

    def test_phase_transport_and_jump_ledgers(self):
        for condense in (False,True):
            m = StefanInterface(LIBRARY,example(condense=condense))
            before = m.report()['stefan']
            m.advance(1000)
            report = m.report()
            result = report['stefan']
            direction = -1 if condense else 1
            j,u,s,rho = (result[k] for k in ('mass_flux_kg_m2_s','vapor_face_velocity_m_s',
                                            'interface_velocity_m_s','vapor_face_density_kg_m3'))
            self.assertGreater(direction*j,.002)
            self.assertGreater(direction*(result['vapor_kg']-before['vapor_kg']),5e-9)
            self.assertGreater(-direction*report['water_interface']['displacement_m'],5e-10)
            self.assertAlmostEqual(rho*(u-s),j,delta=1e-16)
            self.assertAlmostEqual(-m.rho_liquid*s,j,delta=1e-16)
            self.assertAlmostEqual(result['liquid_pressure_pa']-result['vapor_pressure_pa'],
                                   j*j*(1/rho-1/m.rho_liquid),delta=5e-13)
            self.assertAlmostEqual(j,m.permeance*(report['water_interface']['saturation_pressure_pa']-
                                                   result['vapor_pressure_pa']),delta=1e-16)
            self.assertGreater(direction*result['interface_energy_flux_w_m2'],1000.)
            self.assertGreater(direction*result['momentum_kg_m_s'],1e-7)
            self.assertGreater(max(result['gas_pressure_pa'])-min(result['gas_pressure_pa']),.01)
            self.assertLess(abs(result['mass_residual_kg']),2e-16)
            self.assertLess(abs(result['energy_residual_j']),1e-12)
            self.assertLess(abs(result['momentum_residual_kg_m_s']),1e-15)

    def test_coupled_transfer_and_pressure_spatial_refinement(self):
        samples = []
        for cells in (12,24,48,96):
            config = dict(example(cells),dt_s=4.8e-6/cells)
            m = StefanInterface(LIBRARY,config)
            m.advance(round(.00016/m.dt))
            r = m.report()['stefan']
            samples.append((r['evaporated_kg'],r['vapor_pressure_pa']))
        for values in zip(*samples):
            differences = [abs(a-b) for a,b in zip(values,values[1:])]
            print('coupled spatial differences',differences)
            self.assertGreater(differences[0],differences[1])
            self.assertGreater(differences[1],differences[2])

    def test_checkpoint_and_atomic_refusal(self):
        m = StefanInterface(LIBRARY,example())
        m.advance(10)
        accepted = packet(m)
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)/'saved.json'
            execute(LIBRARY,dict(token=1,action='save',args=dict(path=path,session=accepted)))
            reopened = execute(LIBRARY,dict(token=2,action='open',args=dict(path=path)))['session']
        self.assertEqual(accepted,reopened)
        other = restore(LIBRARY,reopened)
        other.advance(100)
        m.advance(100)
        self.assertEqual(packet(m),packet(other))
        bad = m.checkpoint()
        bad['payload']['state'][-1] += .01
        bad['sha256'] = digest(bad['payload'])
        with self.assertRaisesRegex(ValueError,'ledger'):
            m.restore(bad)
        self.assertEqual(packet(m),packet(other))
        with self.assertRaisesRegex(ValueError,'acoustic timestep'):
            StefanInterface(LIBRARY,dict(example(),dt_s=.00001))


if __name__=='__main__':
    unittest.main()
