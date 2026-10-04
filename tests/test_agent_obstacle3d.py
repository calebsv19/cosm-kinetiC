import hashlib,json,math,struct,subprocess,sys,tempfile,unittest,os,time
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,SessionError
from protocol import call
from test_agent_refined import wait
FLUID={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1}
class ObstacleTests(unittest.TestCase):
 def test_controls_inspection_mask_fields_and_comparison(self):
  with tempfile.TemporaryDirectory() as root:
   s=Service(root);scene=call(s,'scene_create',{'scene_id':'cube','template':'cfd_obstacle_3d','dimensions':[4,2,2]})
   options=dict(scene_id='cube',scene_revision=scene['scene_revision'],grid=[16,8,8],steps=1,dt=.01,fluid=FLUID,numerical_memory_limit_mib=32)
   self.assertIn('arbitrary scene objects are unsupported',s.capabilities()['cartesian3d_model']['geometry_scope'])
   self.assertTrue(s.scene_validate(**options)['valid'])
   for n in [8,16]:
    run='cube'+str(n);s.run_start(run,**dict(options,grid=[2*n,n,n]));self.assertEqual(wait(s,run)['state'],'paused')
    before=s.run_inspect(run);sample=s.run_sample(run,'paused',plane='XY',resolution=8,field='solid',points=[[2,1,1],[1,1,1]],wait_ms=5000)
    self.assertEqual(sample['preview']['probes'][0]['values'][2],1);self.assertEqual(s.run_inspect(run)['tick'],before['tick'])
    s.run_control(run,'step','step',scene['scene_revision'],5000);row=wait(s,run,True);self.assertEqual(row['state'],'completed',row)
    self.assertEqual(row['simulation_time'],0);self.assertEqual(row['solve_mode'],'steady_obstacle_duct')
    self.assertEqual(s.run_assess(run)['numerical_status'],'passed');self.assertFalse(s.run_assess(run)['physical_accuracy_certified'])
    self.assertEqual(s.run_assess(run)['reference_accuracy']['status'],'not_established')
    force=row['boundary_force_budget'];self.assertFalse(force['shares_unrestricted_stress_with_energy_interpolant']);self.assertIn('incompressibility',force['normal_viscous_trace_policy']);self.assertEqual(len(force['body_sides']),6);self.assertEqual(force['closed_area_vector_m2'],[0,0,0]);self.assertGreater(force['body_total_force_n'][0],0)
    artifact=next(a for a in s.run_result(run)['artifacts'] if a['path'].endswith('channel_fields.json'))
    raw=Path(artifact['path']).read_bytes();self.assertEqual(hashlib.sha256(raw).hexdigest(),artifact['sha256']);fields=json.loads(raw)['cartesian_fields']
    self.assertEqual(sum(fields['solid_mask']),(n//2)**3);self.assertEqual(len(fields['outlet_x_velocity_m_s']),n*n)
    for solid,p in zip(fields['solid_mask'],fields['pressure_pa']):self.assertEqual(p is None,solid)
    reference=Path(root)/('native'+str(n)+'.bin')
    subprocess.run([str(ROOT/'build/c3d-obstacle/obstacle3d_test'),str(n),'4','2',str(reference)],check=True,capture_output=True)
    binary=reference.read_bytes();self.assertEqual(struct.unpack_from('=3i',binary),(2*n,n,n));values=struct.unpack_from('='+str((4*2*n*n*n+n*n))+'d',binary,12)
    for q,(v,p,solid) in enumerate(zip(fields['velocity_faces_m_s'],fields['pressure_pa'],fields['solid_mask'])):
     self.assertLess(max(abs(v[a]-values[4*q+a]) for a in range(3)),1e-12)
     if solid:self.assertTrue(math.isnan(values[4*q+3]))
     else:self.assertAlmostEqual(p,values[4*q+3],places=12)
    self.assertLess(max(abs(x-y) for x,y in zip(fields['outlet_x_velocity_m_s'],values[4*2*n*n*n:])),1e-12)
   cmp=s.run_compare(['cube8','cube16']);self.assertIn('body_pressure_force_n_0',cmp['comparisons'][0]['changes'])
   with self.assertRaises(SessionError):s.run_compare(['cube8','cube16'],'temporal')
   with self.assertRaises(SessionError):s.request(**dict(options,steps=2))
   with self.assertRaises(SessionError):s.request(**dict(options,grid=[18,9,9]))
   with self.assertRaises(SessionError):s.request(**dict(options,fluid={'density_kg_m3':1000,'dynamic_viscosity_pa_s':.001}))
   s.run_start('limited',**dict(options,grid=[64,32,32],numerical_memory_limit_mib=1,start_paused=False));row=wait(s,'limited',True)
   self.assertEqual(row['state'],'failed');self.assertEqual(row['health']['numerical_memory']['live_bytes'],0)
 def test_actual_mcp_discovery(self):
  with tempfile.TemporaryDirectory() as root:
   p=subprocess.Popen([sys.executable,str(ROOT/'scripts/physics_sim_session.py'),'--root',root,'--mcp'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
   def rpc(i,m,params):
    p.stdin.write(json.dumps({'jsonrpc':'2.0','id':i,'method':m,'params':params})+'\n');p.stdin.flush();return json.loads(p.stdout.readline())
   try:
    self.assertIn('result',rpc(1,'initialize',{'protocolVersion':'2024-11-05','capabilities':{},'clientInfo':{'name':'c3d8','version':'1'}}))
    tools=rpc(2,'tools/list',{})['result']['tools'];tool=next(t for t in tools if t['name']=='scene_create')
    self.assertIn('cfd_obstacle_3d',tool['inputSchema']['properties']['template']['enum'])
    result=rpc(3,'tools/call',{'name':'scene_create','arguments':{'scene_id':'mcp','template':'cfd_obstacle_3d','dimensions':[4,2,2]}})
    self.assertFalse(result['result'].get('isError',False),result)
    revision=result['result']['structuredContent']['scene_revision']
    def tool(i,name,args):
     response=rpc(i,'tools/call',{'name':name,'arguments':args})['result'];self.assertFalse(response.get('isError',False),response);return response['structuredContent']
    tool(4,'run_start',dict(request_id='mcp',scene_id='mcp',scene_revision=revision,grid=[16,8,8],steps=1,dt=.01,fluid=FLUID,numerical_memory_limit_mib=32))
    service=Service(root);self.assertEqual(wait(service,'mcp')['state'],'paused')
    sampled=tool(5,'run_sample',dict(run_id='mcp',request_id='mask',plane='XY',resolution=8,field='solid',points=[[2,1,1]],wait_ms=5000))
    self.assertEqual(sampled['preview']['probes'][0]['values'][2],1)
    tool(6,'run_control',dict(run_id='mcp',command_id='step',action='step',scene_revision=revision,wait_ms=5000))
    self.assertEqual(wait(service,'mcp',True)['state'],'completed')
    assessed=tool(7,'run_assess',dict(run_id='mcp'));self.assertEqual(assessed['reference_accuracy']['status'],'not_established')
   finally:
    p.stdin.close();p.wait(timeout=10);p.stdout.close();p.stderr.close()
 def test_inflight_inspection_cancel_preserves_unsolved_state(self):
  with tempfile.TemporaryDirectory() as root:
   service=Service(root);scene=service.scene_create('cancel','cfd_obstacle_3d')
   service.run_start('cancel',scene_id='cancel',scene_revision=scene['scene_revision'],grid=[64,32,32],steps=1,dt=.01,fluid=FLUID,numerical_memory_limit_mib=256,start_paused=False)
   deadline=time.monotonic()+10
   while time.monotonic()<deadline:
    row=service.run_inspect('cancel')
    if row.get('solve_in_progress'):break
    time.sleep(.02)
   try:
    self.assertTrue(row.get('solve_in_progress'),row)
    sample=service.run_sample('cancel','live',plane='XY',resolution=8,field='pressure_pa',points=[[1,1,1],[2,1,1]],wait_ms=5000)
    self.assertEqual(sample['tick'],0);self.assertIsNone(sample['preview']['probes'][0]['values'][9]);self.assertEqual(sample['preview']['probes'][1]['values'][2],1)
    started=time.monotonic();receipt=service.run_control('cancel','stop','cancel',scene['scene_revision'],5000)
    self.assertEqual(receipt['status'],'applied');self.assertLess(time.monotonic()-started,2)
    row=wait(service,'cancel',True);self.assertEqual(row['state'],'cancelled');self.assertEqual(row['tick'],0)
    artifact=next(a for a in service.run_result('cancel')['artifacts'] if a['path'].endswith('channel_fields.json'));fields=json.loads(Path(artifact['path']).read_text())['cartesian_fields']
    self.assertTrue(all(all(v==0 for v in face) for face in fields['velocity_faces_m_s']));self.assertTrue(all(p is None for p in fields['pressure_pa']))
   finally:
    if service.run_inspect('cancel')['state'] not in ('cancelled','completed','failed'):service.run_control('cancel','cleanup','cancel',scene['scene_revision'],5000)

if __name__=='__main__':unittest.main()
