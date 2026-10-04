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
from service import Service,SessionError
from protocol import call,tools
FLUID={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1}

class ChannelTests(unittest.TestCase):
    def test_channel_lifecycle_pressure_shear_and_result(self):
        with tempfile.TemporaryDirectory(prefix='channel-agent-') as tmp:
            s=Service(tmp)
            scene=call(s,'scene_create',dict(scene_id='channel',template='cfd_channel',dimensions=[2,1,.5],channel={'pressure_gradient_pa_m':.1}))
            rev=scene['scene_revision']
            valid=call(s,'scene_validate',dict(scene_id='channel',scene_revision=rev,grid=[1,64,1],fluid=FLUID,dt=.05))
            self.assertEqual(valid['effective_grid'],[1,64,1]);self.assertFalse(valid['resolution_changed'])
            settings=dict(request_id='run',scene_id='channel',scene_revision=rev,grid=[1,64,1],fluid=FLUID,dt=.05,steps=800)
            call(s,'run_start',settings)
            try:
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    started=s.run_inspect('run')
                    if started['state']!='starting':break
                    time.sleep(.01)
                self.assertEqual(started['state'],'paused')
                self.assertEqual(started['model'],'incompressible_channel_fv_v1')
                self.assertEqual(call(s,'run_start',settings)['tick'],0)
                self.assertEqual(s.run_inspect('run')['health']['volume_flux_m3_s'],0)
                receipt=s.run_control('run','one','step',rev,5000);self.assertEqual(receipt['status'],'applied')
                self.assertEqual(s.run_control('run','one','step',rev,5000),receipt)
                self.assertEqual(s.run_inspect('run')['tick'],1)
                self.assertEqual(s.run_control('run','pause','pause',rev,5000)['status'],'applied')
                sample=call(s,'run_sample',dict(run_id='run',request_id='pressure',field='pressure_pa',plane='XY',resolution=8,points=[[0,.5,.25],[2,.5,.25],[1,0,.25],[-1,0,0]],wait_ms=5000))
                p=sample['preview'];self.assertEqual(p['units'][9],'Pa');self.assertIsNone(p['samples'][0][6])
                self.assertAlmostEqual(p['probes'][0]['values'][9]-.0,.2)
                self.assertAlmostEqual(p['probes'][1]['values'][9],0)
                self.assertEqual(p['probes'][2]['values'][3],0)
                self.assertFalse(p['probes'][3]['inside'])
                self.assertEqual(s.run_inspect('run')['tick'],1)
                with self.assertRaises(SessionError):s.run_sample('run','wrong',field='pressure_proxy')
                with self.assertRaises(SessionError):s.run_start(**dict(settings,dt=.025))
                s.run_control('run','go','continue',rev,5000)
                deadline=time.monotonic()+10
                while time.monotonic()<deadline:
                    end=s.run_inspect('run',history=True)
                    if end['state'] in ('completed','failed'):break
                    time.sleep(.02)
                self.assertEqual(end['state'],'completed');self.assertEqual(end['tick'],800)
                self.assertAlmostEqual(end['simulation_time'],40,places=9)
                h=end['health'];p=end['physics']
                self.assertLess(abs(h['volume_flux_m3_s']-.5*.1/(12*.1))/(.5*.1/(12*.1)),.0005)
                self.assertAlmostEqual(p['wall_on_fluid_bottom_shear_pa'],-.05,places=10)
                self.assertAlmostEqual(p['wall_on_fluid_top_shear_pa'],-.05,places=10)
                self.assertAlmostEqual(h['pressure_drop_from_momentum_pa'],.2,places=10)
                self.assertLess(abs(h['momentum_balance_residual_n']),1e-10)
                self.assertLess(abs(h['energy_balance_residual_w']),1e-10)
                result=s.run_result('run')
                artifact=next(a for a in result['artifacts'] if a['path'].endswith('/channel_fields.json'))
                self.assertEqual(hashlib.sha256(Path(artifact['path']).read_bytes()).hexdigest(),artifact['sha256'])
                exported=json.loads(Path(artifact['path']).read_text());self.assertEqual(len(exported['physics']['profile_y_m_vx_m_s']),64)
            finally:
                if s.run_inspect('run')['state'] not in ('completed','cancelled','failed'):s.run_control('run','cleanup','cancel',rev,5000)

    def test_model_validation_and_cancellation(self):
        with tempfile.TemporaryDirectory() as tmp:
            s=Service(tmp);scene=s.scene_create('ch',template='cfd_channel');rev=scene['scene_revision']
            self.assertEqual(s.scene_create('ch',template='cfd_channel')['scene_revision'],rev)
            for options in ({},{'fluid':FLUID,'grid':[4,32,4]},{'fluid':FLUID,'qualification_mode':True},
                            {'fluid':{'density_kg_m3':1000,'dynamic_viscosity_pa_s':.00001}}, {'fluid':FLUID,'grid':[1,3,1]}):
                with self.assertRaises(SessionError):s.scene_validate('ch',rev,**options)
            with self.assertRaises(SessionError):s.scene_create('bad',template='wind_empty',channel={})
            with self.assertRaises(SessionError):s.scene_create('ch',template='cfd_channel',channel={'pressure_gradient_pa_m':2})
            with self.assertRaises(SessionError):s.scene_create('bad',template='cfd_channel',channel={'pressure_gradient_pa_m':float('nan')})
            with self.assertRaises(SessionError):s.scene_create('bad',template='cfd_channel',channel={'mystery':1})
            s.run_start('cancel','ch',rev,fluid=FLUID)
            self.assertEqual(s.run_control('cancel','stop','cancel',rev,5000)['state'],'cancelled')
            self.assertEqual(s.run_result('cancel')['status']['tick'],0)
            self.assertIn('incompressible_channel_fv_v1',s.capabilities()['models'])
            self.assertIn('cfd_channel',next(t for t in tools() if t['name']=='scene_create')['inputSchema']['properties']['template']['enum'])

if __name__=='__main__':unittest.main(verbosity=2)
