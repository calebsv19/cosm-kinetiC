"""Actual local box scene/MCP authoring, accepted inspection and failure controls."""
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,SessionError
from protocol import call
from test_agent_refined import wait
FLUID={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1}
BODY={'body_min_m':[1.5,.375,.375],'body_max_m':[2.25,1.125,1.625]}

class BoxTests(unittest.TestCase):
    def test_scene_bounds_revision_and_admission(self):
        with tempfile.TemporaryDirectory() as root:
            s=Service(root)
            scene=call(s,'scene_create',dict(scene_id='box',template='cfd_box_3d',channel=BODY))
            self.assertEqual(call(s,'scene_create',dict(scene_id='box',template='cfd_box_3d',channel=BODY)),scene)
            authored=json.loads((Path(scene['project_path'])/'scene_authoring.json').read_text())
            runtime=json.loads((Path(scene['project_path'])/'scene_runtime.json').read_text())
            self.assertEqual(authored['extensions']['physics_sim']['channel_flow']['body_min_m'],BODY['body_min_m'])
            self.assertEqual(runtime['extensions']['physics_sim']['channel_flow']['body_max_m'],BODY['body_max_m'])
            options=dict(scene_id='box',scene_revision=scene['scene_revision'],grid=[32,16,16],dt=.01,
                         fluid=FLUID,numerical_memory_limit_mib=64)
            validation=call(s,'scene_validate',options)
            self.assertTrue(validation['valid']);self.assertEqual(validation['geometry'][0]['kind'],'stationary_aligned_box')
            self.assertEqual(validation['geometry'][0]['projected_area_m2'],.9375)
            for ch in ({},{'body_min_m':BODY['body_min_m']},dict(BODY,center_x_m=2),
                       dict(BODY,body_min_m=[True,.375,.375]),dict(BODY,body_max_m=[1.5,1.125,1.625]),
                       dict(BODY,body_min_m=[float('nan'),.375,.375])):
                with self.assertRaises(SessionError):s.scene_create('bad','cfd_box_3d',channel=ch)
            with self.assertRaises(SessionError):s.scene_create('bad-dim','cfd_box_3d',dimensions=[4,3,2],channel=BODY)
            for extra in (dict(grid=[36,16,16]),dict(steps=2),dict(solver_cell_budget=100),
                          dict(fluid={'density_kg_m3':1000,'dynamic_viscosity_pa_s':.001})):
                with self.assertRaises(SessionError):s.request(**{**options,'steps':1,**extra})
            shifted=dict(BODY,body_min_m=[1.625,.375,.375])
            with self.assertRaises(SessionError):s.scene_create('box','cfd_box_3d',channel=shifted)
            one=s.scene_create('one','cfd_box_3d',channel=dict(BODY,body_max_m=[1.625,1.125,1.625]))
            with self.assertRaises(SessionError):s.request(**dict(options,scene_id='one',scene_revision=one['scene_revision'],steps=1))

    def test_actual_mcp_create_step_sample_export_and_false_certification(self):
        with tempfile.TemporaryDirectory() as root:
            p=subprocess.Popen([sys.executable,str(ROOT/'scripts/physics_sim_session.py'),'--root',root,'--mcp'],
                               stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            def rpc(i,method,params):
                p.stdin.write(json.dumps({'jsonrpc':'2.0','id':i,'method':method,'params':params})+'\n');p.stdin.flush()
                return json.loads(p.stdout.readline())['result']
            def tool(i,name,args):
                response=rpc(i,'tools/call',dict(name=name,arguments=args))
                self.assertFalse(response.get('isError',False),response);return response['structuredContent']
            try:
                rpc(1,'initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'box-checkpoint','version':'1'}})
                catalog=rpc(2,'tools/list',{})['tools'];create=next(x for x in catalog if x['name']=='scene_create')
                self.assertIn('cfd_box_3d',create['inputSchema']['properties']['template']['enum'])
                self.assertIn('body_min_m',create['inputSchema']['properties']['channel']['properties'])
                self.assertIn('steady_box_duct',tool(3,'capabilities',{})['cartesian3d_model']['solve_modes'])
                scene=tool(4,'scene_create',dict(scene_id='box',template='cfd_box_3d',channel=BODY))
                options=dict(scene_id='box',scene_revision=scene['scene_revision'],grid=[32,16,16],dt=.01,
                             fluid=FLUID,numerical_memory_limit_mib=64)
                self.assertTrue(tool(5,'scene_validate',options)['valid'])
                tool(6,'run_start',dict(options,request_id='box',steps=1));service=Service(root)
                self.assertEqual(wait(service,'box')['state'],'paused')
                sampled=tool(7,'run_sample',dict(run_id='box',request_id='mask',field='solid',plane='XZ',resolution=8,
                              points=[[1.75,.75,1],[1,.75,1]],wait_ms=5000))
                self.assertEqual(sampled['preview']['probes'][0]['values'][2],1)
                tool(8,'run_control',dict(run_id='box',command_id='step',action='step',scene_revision=scene['scene_revision'],wait_ms=5000))
                status=wait(service,'box',True);self.assertEqual(status['state'],'completed',status)
                self.assertEqual(status['simulation_time'],0);self.assertEqual(status['solve_mode'],'steady_box_duct')
                force=status['boundary_force_budget'];self.assertGreater(force['body_total_force_n'][0],0)
                self.assertEqual([x['area_m2'] for x in force['body_sides']],[.9375,.9375,.9375,.9375,.5625,.5625])
                sample=rpc(9,'tools/call',dict(name='run_sample',arguments=dict(run_id='box',request_id='pressure',field='pressure_pa',resolution=8)))
                self.assertTrue(sample['isError']);self.assertIn('live_sampling_unavailable_after_terminal',sample['content'][0]['text'])
                assessed=tool(10,'run_assess',dict(run_id='box'));self.assertEqual(assessed['numerical_status'],'passed')
                self.assertEqual(assessed['reference_accuracy']['status'],'not_established');self.assertFalse(assessed['physical_accuracy_certified'])
                result=tool(11,'run_result',dict(run_id='box'))
                for artifact in result['artifacts']:
                    self.assertEqual(hashlib.sha256(Path(artifact['path']).read_bytes()).hexdigest(),artifact['sha256'])
                fields=json.loads(Path(next(a['path'] for a in result['artifacts'] if a['path'].endswith('channel_fields.json'))).read_text())['cartesian_fields']
                self.assertEqual(sum(fields['solid_mask']),6*6*10)
                for solid,pressure in zip(fields['solid_mask'],fields['pressure_pa']):
                    if solid:self.assertIsNone(pressure)
                    else:self.assertTrue(math.isfinite(pressure))
                self.assertEqual(len(fields['velocity_faces_m_s']),32*16*16)
                service.run_start('limited',**dict(options,steps=1,numerical_memory_limit_mib=1,start_paused=False))
                failed=wait(service,'limited',True);self.assertEqual(failed['state'],'failed')
                self.assertEqual(failed['health']['numerical_memory']['live_bytes'],0)
                self.assertEqual(service.run_assess('limited')['numerical_status'],'failed')
            finally:
                p.stdin.close();p.wait(timeout=10);p.stdout.close();p.stderr.close()

if __name__=='__main__':unittest.main()
