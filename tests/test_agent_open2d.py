import hashlib,json,sys,tempfile,time,unittest
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,SessionError,TERMINAL
from protocol import call

def wait(s,run,terminal=False):
    end=time.monotonic()+30
    while time.monotonic()<end:
        row=s.run_inspect(run)
        if (row['state'] in TERMINAL if terminal else row['state']!='starting'):return row
        time.sleep(.01)
    raise AssertionError('worker did not reach expected state')

class OpenTests(unittest.TestCase):
    def test_channel_lifecycle_and_physical_pressure(self):
        with tempfile.TemporaryDirectory() as d:
            s=Service(d);scene=call(s,'scene_create',{'scene_id':'channel','template':'cfd_open_channel_2d'})
            opts=dict(scene_id='channel',scene_revision=scene['scene_revision'],grid=[16,16,1],dt=.001,fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1})
            valid=call(s,'scene_validate',opts);self.assertTrue(valid['valid'])
            self.assertEqual(valid['effective_grid'],[16,16,1])
            self.assertEqual(valid['voxel_size_m'],1/16)
            call(s,'run_start',dict(opts,request_id='channel',steps=20));self.assertEqual(wait(s,'channel')['state'],'paused')
            try:
                s.run_control('channel','step','step',scene['scene_revision'],5000)
                row=s.run_inspect('channel');self.assertAlmostEqual(row['physics']['mean_inlet_pressure_pa'],.12,places=7)
                self.assertLess(abs(row['health']['mass_balance_residual_kg_s']),1e-8)
                sample=s.run_sample('channel','pressure',field='pressure_pa',points=[[0,.5,.5],[2,.5,.5]],wait_ms=5000)['preview']
                self.assertAlmostEqual(sample['probes'][1]['values'][9],0)
                self.assertGreater(sample['probes'][0]['values'][9],.1)
                self.assertEqual(s.run_inspect('channel')['tick'],row['tick'])
                s.run_control('channel','go','continue',scene['scene_revision'],5000)
                self.assertEqual(wait(s,'channel',True)['state'],'completed')
                artifact=next(x for x in s.run_result('channel')['artifacts'] if x['path'].endswith('channel_fields.json'))
                payload=Path(artifact['path']).read_bytes();self.assertEqual(hashlib.sha256(payload).hexdigest(),artifact['sha256'])
                fields=json.loads(payload);self.assertEqual(fields['schema'],'physics_sim_open2d_fields_v1')
                self.assertEqual(len(fields['staggered_fields']['u_x_faces_m_s']),16*17)
                self.assertIn('pressure_pa',fields['staggered_fields'])
            finally:
                if s.run_inspect('channel')['state'] not in TERMINAL:s.run_control('channel','cleanup','cancel',scene['scene_revision'],5000)

    def test_obstacle_force_history_and_comparison(self):
        with tempfile.TemporaryDirectory() as d:
            s=Service(d);scene=s.scene_create('body',template='cfd_open_obstacle_2d');rev=scene['scene_revision']
            for run,n,dt,steps in [('coarse',16,.001,20),('fine',32,.001,20),('time',16,.0005,40)]:
                s.run_start(request_id=run,scene_id='body',scene_revision=rev,grid=[n,n,1],dt=dt,steps=steps,fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1})
                self.assertEqual(wait(s,run)['state'],'paused')
                if run=='coarse':
                    initial=s.run_inspect(run)
                    self.assertGreater(initial['health']['volume_flux_m3_s'],0)
                    self.assertGreater(initial['health']['max_divergence_s_inv'],0)
                    self.assertEqual(initial['health']['projection_status'],'not_started')
                    sample=s.run_sample(run,'mask',field='solid',points=[[2,1,.25],[.5,.5,.25]],wait_ms=5000)['preview']
                    self.assertEqual(sample['probes'][0]['values'][2],1);self.assertEqual(sample['probes'][1]['values'][2],0)
                s.run_control(run,'step','step',rev,5000)
                row=s.run_inspect(run,history=True);force=row['boundary_force_budget']
                self.assertTrue(force['available']);self.assertGreater(force['surface_total_force_x_n'],0)
                self.assertIn('control_volume_force_x_n',force)
                self.assertIn('not_qualified_for_this_run',force['drag_status'])
                self.assertIn('boundary_force_budget',row['history'][-1])
                self.assertIn('current_run_not_automatically_qualified',row['qualification']['status'])
                s.run_control(run,'go','continue',rev,5000);self.assertEqual(wait(s,run,True)['state'],'completed')
            comp=call(s,'run_compare',{'run_ids':['coarse','fine']})
            self.assertIn('surface_total_force_x_n',comp['comparisons'][0]['changes'])
            self.assertEqual(s.run_compare(['coarse','time'],'temporal')['kind'],'temporal')
            assessment=call(s,'run_assess',{'run_id':'fine','spatial_coarse_run':'coarse'})
            self.assertEqual(assessment['gates']['steady_velocity']['status'],'not_established')
            self.assertEqual(assessment['gates']['temporal_refinement']['status'],'not_established')
            self.assertFalse(assessment['physical_accuracy_certified'])
            with self.assertRaises(SessionError):s.run_compare(['fine','coarse'])

    def test_validation_and_discovery(self):
        with tempfile.TemporaryDirectory() as d:
            s=Service(d);caps=s.capabilities();self.assertIn('incompressible_open2d_v1',caps['models'])
            scene=s.scene_create('body',template='cfd_open_obstacle_2d')
            for grid,dt in [([12,12,1],.001),([64,64,1],.1)]:
                with self.assertRaises(SessionError):s.scene_validate('body',scene['scene_revision'],grid=grid,dt=dt,fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1})
            with self.assertRaises(SessionError):s.scene_create('wrong',template='cfd_open_obstacle_2d',channel={'pressure_gradient_pa_m':1})
if __name__=='__main__':unittest.main(verbosity=2)
