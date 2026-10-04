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
from test_agent_refined import wait
from cartesian3d import assess_3d

class CartesianTests(unittest.TestCase):
    def test_duct_refinement_and_reference(self):
        with tempfile.TemporaryDirectory() as root:
            s=Service(root)
            scene=call(s,'scene_create',dict(scene_id='duct',template='cfd_duct_3d',dimensions=[4,2,2]))
            options=dict(scene_id='duct',scene_revision=scene['scene_revision'],steps=1,dt=.005,start_paused=False,
                         fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=64)
            self.assertTrue(s.scene_validate(**dict(options,grid=[16,8,8]))['valid'])
            for name,n in [('coarse',8),('mid',16),('fine',32)]:
                s.run_start(name,grid=[2*n,n,n],**options);row=wait(s,name,True)
                self.assertEqual(row['state'],'completed',row);self.assertEqual(row['simulation_time'],0)
                self.assertEqual(s.run_assess(name)['reference_accuracy']['status'],'passed' if n==32 else 'failed')
            report=s.run_compare(['coarse','mid','fine'])
            self.assertIn('wall_drag_n_3',report['comparisons'][0]['changes'])
            self.assertIn('pressure_drop_pa',report['comparisons'][0]['changes'])
            self.assertEqual(s.run_assess('fine',spatial_coarse_run='mid')['reference_accuracy']['status'],'passed')
            with self.assertRaises(SessionError):s.run_compare(['mid','fine'],'temporal')
            artifact=next(a for a in s.run_result('fine')['artifacts'] if a['path'].endswith('channel_fields.json'))
            data=Path(artifact['path']).read_bytes();self.assertEqual(hashlib.sha256(data).hexdigest(),artifact['sha256'])
            fields=json.loads(data);self.assertEqual(fields['schema'],'physics_sim_cartesian3d_fields_v1')
            self.assertEqual(len(fields['cartesian_fields']['velocity_faces_m_s']),64*32*32)

    def test_transient_controls_slices_probes(self):
        with tempfile.TemporaryDirectory() as root:
            s=Service(root);scene=s.scene_create('flow','cfd_manufactured_3d',dimensions=[2,2.5,3])
            options=dict(scene_id='flow',scene_revision=scene['scene_revision'],grid=[8,8,8],steps=6,dt=.01,
                         fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=16)
            s.run_start('flow',**options);self.assertEqual(wait(s,'flow')['state'],'paused')
            try:
                s.run_control('flow','step','step',scene['scene_revision'],5000)
                before=s.run_inspect('flow');self.assertEqual(before['tick'],1)
                for plane in ('XY','XZ','YZ'):
                    sample=s.run_sample('flow','slice'+plane,plane=plane,resolution=8,field='pressure_pa',points=[[1,1,1],[3,1,1]],wait_ms=5000)['preview']
                    self.assertIsNone(sample['statistics']['pressure_proxy']);self.assertEqual(len(sample['samples']),64);self.assertIsNotNone(sample['probes'][0]['values'][9]);self.assertFalse(sample['probes'][1]['inside'])
                    self.assertEqual(s.run_inspect('flow')['tick'],1)
                self.assertEqual(s.run_assess('flow')['numerical_status'],'passed')
                self.assertEqual(s.run_assess('flow')['reference_accuracy']['status'],'not_applicable')
                s.run_control('flow','go','continue',scene['scene_revision'],5000);self.assertEqual(wait(s,'flow',True)['state'],'completed')
            finally:
                if s.run_inspect('flow')['state'] not in TERMINAL:s.run_control('flow','cleanup','cancel',scene['scene_revision'],5000)
            for name,dt,steps in [('time-coarse',.02,3),('time-fine',.01,6)]:
                s.run_start(name,**dict(options,dt=dt,steps=steps,start_paused=False));self.assertEqual(wait(s,name,True)['state'],'completed')
            self.assertIn('kinetic_energy_j',s.run_compare(['time-coarse','time-fine'],'temporal')['comparisons'][0]['changes'])

    def test_pause_cancel_and_idempotent_receipts(self):
        with tempfile.TemporaryDirectory() as root:
            s=Service(root);scene=s.scene_create('control','cfd_manufactured_3d')
            s.run_start('control',scene_id='control',scene_revision=scene['scene_revision'],grid=[16,16,16],steps=10000,dt=.005,
                        fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=32)
            self.assertEqual(wait(s,'control')['state'],'paused')
            s.run_control('control','go','continue',scene['scene_revision'],5000)
            started=time.monotonic();receipt=s.run_control('control','pause','pause',scene['scene_revision'],5000)
            self.assertEqual(receipt['state'],'paused');self.assertLess(time.monotonic()-started,2)
            tick=s.run_inspect('control')['tick']
            self.assertEqual(s.run_control('control','pause','pause',scene['scene_revision'],5000),receipt)
            self.assertEqual(s.run_inspect('control')['tick'],tick)
            s.run_control('control','cancel','cancel',scene['scene_revision'],5000)
            self.assertEqual(wait(s,'control',True)['state'],'cancelled')

    def test_budget_rejection_and_scope(self):
        with tempfile.TemporaryDirectory() as root:
            s=Service(root);scene=s.scene_create('duct','cfd_duct_3d')
            options=dict(scene_id='duct',scene_revision=scene['scene_revision'],grid=[64,32,32],steps=1,dt=.01,start_paused=False,
                         fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=1)
            s.run_start('limited',**options);row=wait(s,'limited',True);self.assertEqual(row['state'],'failed');self.assertEqual(row['error'],'numerical_memory_budget_exceeded')
            self.assertEqual(row['health']['numerical_memory']['live_bytes'],0);self.assertEqual(s.run_assess('limited')['numerical_status'],'failed')
            with self.assertRaises(SessionError):s.request(**dict(options,steps=2))
            with self.assertRaises(SessionError):s.request(**dict(options,grid=[16,16,1]))
            with self.assertRaises(SessionError):s.request(**dict(options,solver_cell_budget=1))
            with self.assertRaises(SessionError):s.scene_create('bad','cfd_duct_3d',channel={'pressure_gradient_pa_m':1})
            with self.assertRaises(SessionError):s.scene_create('bad2','cfd_manufactured_3d',channel={'solve_mode':'steady_duct'})
        fabricated={'state':'completed','health':{'projection_status':'converged','linear_relative_residual':0,'max_abs_divergence_s_inv':0},'qualification':{'duct_reference_gate':{'applicable':True,'passed':True}}}
        self.assertEqual(assess_3d(fabricated)['reference_accuracy']['status'],'not_established')

if __name__=='__main__':unittest.main()
