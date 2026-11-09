"""Independent pressure work, surface motion, and conservative phase tests."""
import math
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from chemistry_reactor import digest
from chemistry_water_surface_pressure import DRY_CV, DRY_R, GRAVITY, WaterInterface, WaterSurfacePressure, example
from chemistry_workspace import execute, packet, restore

LIBRARY = os.environ['CHEMISTRY_WATER_INTERFACE_LIBRARY']


class PressureSurfaceTests(unittest.TestCase):
    def dry_mode(self, dt, amplitude=1e-6):
        config = example(6, 3)
        config['damping_per_s'] = 0.
        column = WaterInterface(LIBRARY,config['columns'][0])
        depth = column.geometry(.12)[0]
        gap = column.height-depth
        for i,c in enumerate(config['columns']):
            mode = math.cos(math.pi*(i+.5)/6)
            temperature = 298.15*(gap/(gap-depth*amplitude*mode))**(DRY_R/DRY_CV)
            c.update(liquid_kg=.12*(1+amplitude*mode), carrier_heat_capacity_j_k=.15,
                     gas_temperature_k=[temperature]*3,
                     vapor_density_kg_m3=[0.]*3, dt_s=dt, diffusivity_m2_s=0.,
                     conductivity_w_m_k=0., mass_transfer_velocity_m_s=0.)
        return WaterSurfacePressure(LIBRARY,config)

    def test_linear_pressure_loaded_surface_mode(self):
        errors = []
        gamma = 1+DRY_R/DRY_CV
        for dt in (.001,.0005,.00025):
            model = self.dry_mode(dt)
            before = model.report()['hydraulic']['columns']
            gap = model.columns[0].height-model.depth
            pressure = .15*DRY_R*298.15/(DRY_CV*model.columns[0].area*gap)
            stiffness = GRAVITY+gamma*pressure/(model.rho*gap)
            omega = 2*math.sin(math.pi/12)*math.sqrt(stiffness*model.depth)/model.dx
            time = .08
            model.advance(round(time/dt))
            after = model.report()['hydraulic']['columns']
            expected = [model.depth+(p['position_m']-model.depth)*math.cos(omega*time) for p in before]
            errors.append(max(abs(p['position_m']-h) for p,h in zip(after,expected))/(model.depth*1e-6))
        print('pressure-loaded mode normalized errors',errors)
        self.assertGreater(errors[0]/errors[1],3.7)
        self.assertGreater(errors[1]/errors[2],3.7)
        self.assertLess(errors[-1],1e-4)

    def test_adiabatic_gas_work(self):
        model = self.dry_mode(.0002,.004)
        start = model.report()['hydraulic']
        model.advance(250)
        end = model.report()['hydraulic']
        gamma = 1+DRY_R/DRY_CV
        for a,b in zip(start['columns'],end['columns']):
            ga = model.columns[0].height-a['position_m']
            gb = model.columns[0].height-b['position_m']
            self.assertAlmostEqual(b['pressure_pa']/a['pressure_pa'],(ga/gb)**gamma,delta=2e-8)
            self.assertAlmostEqual(b['gas_temperature_k'][0]/a['gas_temperature_k'][0],(ga/gb)**(gamma-1),delta=1e-8)
        self.assertGreater(end['kinetic_energy_j'],1e-6)
        self.assertLess(abs(end['total_energy_residual_j']),1e-12)

    def test_pressure_drives_deformation_with_phase_transport(self):
        model = WaterSurfacePressure(LIBRARY,example())
        start = model.report()['hydraulic']
        self.assertEqual(len({p['position_m'] for p in start['columns']}),1)
        self.assertTrue(all(q==0. for q in start['face_flow_kg_s']))
        model.advance(200)
        end = model.report()['hydraulic']
        left,right = end['columns'][0],end['columns'][-1]
        self.assertGreater(left['position_m']-right['position_m'],.00015)
        self.assertGreater(left['evaporated_kg'],1e-8)
        self.assertGreater(right['condensed_kg'],1e-8)
        self.assertLess(start['columns'][0]['pressure_pa'],start['columns'][-1]['pressure_pa'])
        self.assertGreater(left['pressure_pa'],right['pressure_pa'])
        self.assertLess(max(map(abs,end['column_water_residual_kg'])),1e-15)
        self.assertLess(max(map(abs,end['phase_residual_kg'])),1e-15)
        self.assertLess(abs(end['total_energy_residual_j']),1e-12)

    def test_coupled_refinement(self):
        states = []
        for dt in (.0008,.0004,.0002,.0001):
            config = example()
            for c in config['columns']:
                c['dt_s'] = dt
            model = WaterSurfacePressure(LIBRARY,config)
            model.advance(round(.04/dt))
            states.append(model.report()['hydraulic']['columns'][0]['vapor_density_kg_m3'])
        errors = [max(abs(a-b) for a,b in zip(x,y)) for x,y in zip(states,states[1:])]
        print('coupled vapor refinement differences',errors)
        self.assertGreater(errors[0]/errors[1],3.8)
        self.assertGreater(errors[1]/errors[2],3.8)

    def test_phase_extent_convergence_on_moving_gas_mesh(self):
        extents = []
        for cells in (4,8,16,32):
            config = example(6,cells)
            for c in config['columns']:
                c['dt_s'] = .0001
            model = WaterSurfacePressure(LIBRARY,config)
            model.advance(400)
            columns = model.report()['hydraulic']['columns']
            extents.append((columns[0]['evaporated_kg'],columns[-1]['condensed_kg']))
        for extent in zip(*extents):
            errors = [abs(a-b) for a,b in zip(extent,extent[1:])]
            print('moving mesh extent refinement (kg)',errors)
            self.assertGreater(errors[0]/errors[1],1.35)
            self.assertGreater(errors[1]/errors[2],2.6)

    def test_workspace_restart_and_refusal(self):
        model = WaterSurfacePressure(LIBRARY,example())
        model.advance(40)
        before = packet(model)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'session.json'
            execute(LIBRARY,dict(token=1,action='save',args=dict(path=path,session=before)))
            reopened = execute(LIBRARY,dict(token=2,action='open',args=dict(path=path)))['session']
        self.assertEqual(before,reopened)
        resumed = restore(LIBRARY,reopened)
        resumed.advance(30)
        model.advance(30)
        self.assertEqual(packet(resumed),packet(model))
        checkpoint = resumed.checkpoint()
        checkpoint['payload']['state'][resumed.end] += .001
        checkpoint['sha256'] = digest(checkpoint['payload'])
        with self.assertRaisesRegex(ValueError,'energy ledger'):
            resumed.restore(checkpoint)
        self.assertEqual(packet(resumed),packet(model))
        config = example()
        for c in config['columns']:
            c['dt_s'] = .01
        with self.assertRaisesRegex(ValueError,'elastic timestep'):
            WaterSurfacePressure(LIBRARY,config)
        forced = example()
        for i,c in enumerate(forced['columns']):
            c['carrier_heat_capacity_j_k'] = .15*(1+.15*math.cos(math.pi*(i+.5)/6))
        model = WaterSurfacePressure(LIBRARY,forced)
        model.advance(5)
        accepted = packet(model)
        with self.assertRaisesRegex(ValueError,'displacement'):
            model.advance(200)
        self.assertEqual(accepted,packet(model))


if __name__ == '__main__':
    unittest.main()
