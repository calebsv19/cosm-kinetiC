import hashlib
import json
from pathlib import Path
import sys
import tempfile
import time
import unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,SessionError,TERMINAL
from protocol import call
FLUID={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1}
class MacTests(unittest.TestCase):
    def test_live_coupling_and_fields(self):
        with tempfile.TemporaryDirectory(prefix='mac2d-agent-') as tmp:
            s=Service(tmp);scene=call(s,'scene_create',dict(scene_id='mac',template='cfd_channel_2d',dimensions=[2,1,.5],channel={'pressure_gradient_pa_m':.1,'initial_divergence_perturbation_m_s':.1}))
            rev=scene['scene_revision'];options=dict(scene_id='mac',scene_revision=rev,grid=[16,16,1],fluid=FLUID,dt=.01)
            valid=call(s,'scene_validate',options);self.assertEqual(valid['effective_grid'],[16,16,1])
            call(s,'run_start',dict(request_id='run',steps=20,**options))
            try:
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    snap=s.run_inspect('run')
                    if snap['state']!='starting':break
                    time.sleep(.01)
                self.assertEqual(snap['state'],'paused');self.assertEqual(snap['model'],'incompressible_mac2d_v1')
                self.assertGreater(snap['health']['max_divergence_s_inv'],.01)
                receipt=s.run_control('run','one','step',rev,5000);self.assertEqual(receipt['status'],'applied')
                self.assertEqual(s.run_control('run','one','step',rev,5000),receipt)
                snap=s.run_inspect('run',history=True);h=snap['health']
                budget=snap['boundary_force_budget']
                self.assertIn('unsupported',budget['outlet_status'])
                self.assertIn('not_qualified',budget['drag_status'])
                self.assertAlmostEqual(sum(budget['periodic_cut_outward_advective_momentum_x_n']),0,places=12)
                self.assertAlmostEqual(budget['mean_pressure_drive_force_x_n'],.1,places=12)
                self.assertAlmostEqual(budget['instantaneous_drive_plus_wall_force_x_n'],.1-budget['bottom_fluid_on_wall_shear_force_x_n']-budget['top_fluid_on_wall_shear_force_x_n'],places=12)
                self.assertLess(h['max_divergence_s_inv'],1e-8);self.assertGreater(h['pressure_iterations'],0)
                self.assertEqual(h['projection_status'],'converged')
                self.assertLess(abs(h['streamwise_momentum_balance_residual_n']),1e-9)
                sample=s.run_sample('run','p',field='pressure_pa',plane='XY',resolution=16,points=[[1,0,.25],[1,1,.25]],wait_ms=5000)
                v=sample['preview'];self.assertEqual(v['grid'],[16,16,1]);self.assertEqual(v['units'][9],'Pa')
                self.assertGreater(max(abs(x[4]) for x in v['samples']),1e-5)
                for p in v['probes']:self.assertEqual(p['values'][3:5],[0,0])
                self.assertEqual(s.run_inspect('run')['tick'],1)
                with self.assertRaises(SessionError):s.run_sample('run','bad',field='pressure_proxy')
                s.run_control('run','go','continue',rev,5000)
                deadline=time.monotonic()+10
                while time.monotonic()<deadline:
                    end=s.run_inspect('run')
                    if end['state'] in TERMINAL:break
                    time.sleep(.01)
                self.assertEqual(end['state'],'completed')
                result=s.run_result('run');artifact=next(a for a in result['artifacts'] if a['path'].endswith('/channel_fields.json'))
                self.assertEqual(hashlib.sha256(Path(artifact['path']).read_bytes()).hexdigest(),artifact['sha256'])
                data=json.loads(Path(artifact['path']).read_text());fields=data['staggered_fields']
                self.assertEqual(data['schema'],'physics_sim_mac2d_fields_v1')
                self.assertEqual(len(fields['u_x_faces_m_s']),256);self.assertEqual(len(fields['v_y_faces_m_s']),272);self.assertEqual(len(fields['pressure_correction_pa']),256)
                self.assertAlmostEqual(sum(fields['pressure_correction_pa']),0,places=9)
            finally:
                if s.run_inspect('run')['state'] not in TERMINAL:s.run_control('run','cleanup','cancel',rev,5000)
    def test_validation(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=Service(tmp);scene=s.scene_create('m',template='cfd_channel_2d');rev=scene['scene_revision']
            for grid in ([1,32,1],[16,16,2],[128,16,1]):
                with self.assertRaises(SessionError):s.scene_validate('m',rev,grid=grid,fluid=FLUID)
            with self.assertRaises(SessionError):s.scene_validate('m',rev,fluid={'density_kg_m3':.001,'dynamic_viscosity_pa_s':1000},dt=.1)
            with self.assertRaises(SessionError):s.scene_create('bad',template='cfd_channel',channel={'initial_divergence_perturbation_m_s':.1})
            with self.assertRaises(SessionError):s.scene_create('bad',template='cfd_channel_2d',channel={'initial_divergence_perturbation_m_s':2})
            self.assertIn('incompressible_mac2d_v1',s.capabilities()['models'])
if __name__=='__main__':unittest.main(verbosity=2)
