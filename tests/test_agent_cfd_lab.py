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
class LabTests(unittest.TestCase):
    def test_obstacle_lifecycle(self):
        with tempfile.TemporaryDirectory(prefix='cfd-lab-') as tmp:
            s=Service(tmp)
            scene=call(s,'scene_create',{'scene_id':'block','template':'cfd_obstacle_2d'})
            rev=scene['scene_revision']
            options=dict(scene_id='block',scene_revision=rev,grid=[16,16,1],fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},dt=.01)
            v=call(s,'scene_validate',options);self.assertEqual(v['effective_grid'],[16,16,1])
            call(s,'run_start',dict(request_id='block-run',steps=30,**options))
            deadline=time.monotonic()+10
            while (snap:=s.run_inspect('block-run'))['state']=='starting' and time.monotonic()<deadline:time.sleep(.01)
            self.assertEqual(snap['state'],'paused')
            try:
                sample=s.run_sample('block-run','mask',field='solid',plane='XY',resolution=16,points=[[2,1,.25],[.5,.5,.25]],wait_ms=5000)['preview']
                self.assertEqual(sample['probes'][0]['values'][2],1)
                self.assertEqual(sample['probes'][1]['values'][2],0)
                s.run_control('block-run','step','step',rev,5000)
                snap=s.run_inspect('block-run',history=True);force=snap['boundary_force_budget']
                self.assertIn('exploratory',force['obstacle_status'])
                self.assertIn('not_qualified',force['drag_status'])
                self.assertGreater(force['obstacle_discrete_pressure_force_x_n'],0)
                self.assertTrue(force['control_volume_comparison']['available'])
                self.assertIn('surface_minus_control_volume_x_n',force['control_volume_comparison'])
                self.assertLess(abs(snap['health']['streamwise_momentum_balance_residual_n']),1e-8)
                self.assertIn('boundary_force_budget',snap['history'][-1])
                s.run_control('block-run','go','continue',rev,5000)
                deadline=time.monotonic()+20
                while (snap:=s.run_inspect('block-run'))['state'] not in TERMINAL and time.monotonic()<deadline:time.sleep(.01)
                self.assertEqual(snap['state'],'completed')
                artifact=next(x for x in s.run_result('block-run')['artifacts'] if x['path'].endswith('channel_fields.json'))
                payload=Path(artifact['path']).read_bytes();self.assertEqual(hashlib.sha256(payload).hexdigest(),artifact['sha256'])
                data=json.loads(payload);self.assertEqual(sum(data['staggered_fields']['solid_cells']),16)
            finally:
                if s.run_inspect('block-run')['state'] not in TERMINAL:s.run_control('block-run','cleanup','cancel',rev,5000)
    def test_refinement_comparison(self):
        with tempfile.TemporaryDirectory(prefix='cfd-compare-') as tmp:
            s=Service(tmp);scene=s.scene_create('shared',template='cfd_obstacle_2d');rev=scene['scene_revision']
            for run,n,steps,dt in [('coarse',16,20,.01),('fine',32,20,.01),('time',16,40,.005),('wrongtime',16,10,.01)]:
                s.run_start(request_id=run,scene_id='shared',scene_revision=rev,grid=[n,n,1],fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},steps=steps,dt=dt,start_paused=False)
                deadline=time.monotonic()+20
                while (snap:=s.run_inspect(run))['state'] not in TERMINAL and time.monotonic()<deadline:time.sleep(.01)
                self.assertEqual(snap['state'],'completed')
            result=call(s,'run_compare',{'run_ids':['coarse','fine']})
            self.assertIn('sensitivity_only',result['qualification'])
            self.assertIn('obstacle_discrete_viscous_force_x_n',result['comparisons'][0]['changes'])
            self.assertEqual(call(s,'run_compare',{'run_ids':['coarse','time'],'kind':'temporal'})['kind'],'temporal')
            for ids in (['coarse','wrongtime'],['fine','coarse'],['coarse','coarse']):
                with self.assertRaises(SessionError):s.run_compare(ids)

    def test_geometry_rejects_silent_snapping(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=Service(tmp);scene=s.scene_create('block',template='cfd_obstacle_2d')
            with self.assertRaises(SessionError):s.scene_validate('block',scene['scene_revision'],grid=[12,12,1],fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1})
            with self.assertRaises(SessionError):s.scene_create('perturb',template='cfd_obstacle_2d',channel={'initial_divergence_perturbation_m_s':.1})
            self.assertIn('cfd_obstacle_2d',s.capabilities()['templates'])
if __name__=='__main__':unittest.main(verbosity=2)
