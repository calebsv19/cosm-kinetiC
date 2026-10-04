import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,SessionError
from cartesian3d import assess_3d
from test_agent_refined import wait

class Open3dTests(unittest.TestCase):
    def test_open_scene_controls_sample_and_assessment(self):
        with tempfile.TemporaryDirectory() as root:
            s=Service(root);scene=s.scene_create('open','cfd_open_duct_3d',dimensions=[4,2,2])
            options=dict(scene_id='open',scene_revision=scene['scene_revision'],grid=[16,8,8],steps=1,dt=.01,
                         fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=16)
            s.run_start('open',**options);self.assertEqual(wait(s,'open')['state'],'paused')
            s.run_control('open','step','step',scene['scene_revision'],5000)
            row=wait(s,'open',True);self.assertEqual(row['state'],'completed',row);self.assertEqual(row['solve_mode'],'steady_open_duct');self.assertEqual(row['simulation_time'],0)
            self.assertEqual(s.run_assess('open')['reference_accuracy']['status'],'failed')
            artifact=next(a for a in s.run_result('open')['artifacts'] if a['path'].endswith('channel_fields.json'))
            data=Path(artifact['path']).read_bytes();self.assertEqual(hashlib.sha256(data).hexdigest(),artifact['sha256'])
            fields=json.loads(data)['cartesian_fields'];self.assertEqual(len(fields['outlet_x_velocity_m_s']),64)
            self.assertGreater(max(fields['pressure_pa'])-min(fields['pressure_pa']),.001)
            self.assertIn('energy_imbalance',s.run_assess('open')['reference_accuracy']['checks'])
            s.run_start('limited',**dict(options,grid=[64,32,32],numerical_memory_limit_mib=1,start_paused=False))
            row=wait(s,'limited',True);self.assertEqual(row['error'],'numerical_memory_budget_exceeded');self.assertEqual(row['health']['numerical_memory']['live_bytes'],0)
            with self.assertRaises(SessionError):s.request(**dict(options,steps=2))
            with self.assertRaises(SessionError):s.run_compare(['open'],'temporal')
            with self.assertRaises(SessionError):s.scene_create('bad','cfd_open_duct_3d',channel={'outlet_pressure_pa':1})

    def test_open_gate_cannot_inherit_periodic_gate(self):
        status={'state':'completed','solve_mode':'steady_open_duct','health':{'projection_status':'converged','linear_relative_residual':0,'max_abs_divergence_s_inv':0},'qualification':{'duct_reference_gate':{'applicable':True,'passed':True,'velocity_relative_l2':0,'pressure_relative_error':0,'volume_flow_relative_error':0,'max_wall_relative_error':0,'energy_relative_error':0}}}
        self.assertEqual(assess_3d(status)['reference_accuracy']['status'],'not_established')
        ref=status['qualification']['duct_reference_gate'];ref.update(energy_imbalance=0,flux_relative_error=0)
        self.assertEqual(assess_3d(status)['reference_accuracy']['status'],'passed')
        ref['energy_imbalance']=.03;self.assertEqual(assess_3d(status)['reference_accuracy']['status'],'failed')
        ref['energy_imbalance']=0;ref['flux_relative_error']=float('nan');self.assertEqual(assess_3d(status)['reference_accuracy']['status'],'not_established')

if __name__=='__main__':unittest.main()
