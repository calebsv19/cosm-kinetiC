#!/usr/bin/env python3
"""Frozen-worker cube matrix and independent exported topology/flux/native readback."""
import hashlib,json,math,os,shutil,struct,subprocess,sys,time
from pathlib import Path
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,TERMINAL

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
 worker=Path(os.environ['PHYSICS_SIM_SESSION_WORKER']).resolve();identity=sha(worker)
 root=ROOT/'build/c3d-obstacle/agent-evidence'/identity;root.mkdir(parents=True,exist_ok=True)
 frozen=root/'worker_snapshot'
 if not frozen.exists():shutil.copy2(worker,frozen)
 assert sha(frozen)==identity
 existing=root/'qualification.json'
 if existing.exists():
  previous=json.loads(existing.read_text());assert previous['worker_sha256']==identity
  for record in previous['records']:
   for artifact in record['result']['artifacts']:assert sha(Path(artifact['path']))==artifact['sha256']
   assert sha(root/(record['case']+'.bin'))==record['binary_sha256']
  print(json.dumps({'retained_cases':len(previous['records']),'worker_sha256':identity,'cost_scope':'original executed measurements preserved'}));return
 service=Service(root,worker=frozen);records=[]
 for name,n,L,cx in [('n8',8,4,2),('n16',16,4,2),('n32',32,4,2),('n48',48,4,2),('outlet6',32,6,2),('outlet8',32,8,2),('inlet4',32,8,4)]:
  scene=service.scene_create(name,'cfd_obstacle_3d',dimensions=[L,2,2],channel={'center_x_m':cx})
  started=time.monotonic();run=name;opts=dict(scene_id=name,scene_revision=scene['scene_revision'],grid=[L*n//2,n,n],steps=1,dt=.01,fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=512)
  if not (root/'runs'/run/'request.json').exists():service.run_start(run,**opts,start_paused=False)
  while True:
   state=service.run_inspect(run,history=True)
   if state['state'] in TERMINAL:break
   assert time.monotonic()-started<240,(name,state['state'])
   time.sleep(.2)
  assert state['state']=='completed',(name,state)
  agent_wall_s=time.monotonic()-started
  result=service.run_result(run);artifact=next(a for a in result['artifacts'] if a['path'].endswith('channel_fields.json'))
  for a in result['artifacts']:assert sha(Path(a['path']))==a['sha256']
  fields=json.loads(Path(artifact['path']).read_text())['cartesian_fields'];N=opts['grid'];nx,ny,nz=N
  v=fields['velocity_faces_m_s'];p=fields['pressure_pa'];mask=fields['solid_mask'];upper=fields['outlet_x_velocity_m_s'];h=fields['spacing_m']
  assert len(v)==len(p)==len(mask)==nx*ny*nz and sum(mask)==(n//2)**3
  def face(a,i,j,k):
   if a==0 and i==nx:return upper[k*ny+j]
   if a==1 and j==ny or a==2 and k==nz:return 0
   return v[(k*ny+j)*nx+i][a]
  maxdiv=0
  for k in range(nz):
   for j in range(ny):
    for i in range(nx):
     q=(k*ny+j)*nx+i
     if mask[q]:assert p[q] is None and all(x==0 for x in v[q]);continue
     assert math.isfinite(p[q]) and all(math.isfinite(x) for x in v[q])
     div=(face(0,i+1,j,k)-face(0,i,j,k))/h[0]+(face(1,i,j+1,k)-face(1,i,j,k))/h[1]+(face(2,i,j,k+1)-face(2,i,j,k))/h[2]
     maxdiv=max(maxdiv,abs(div))
  assert maxdiv<1e-8
  flux=[sum(face(0,i,j,k)*h[1]*h[2] for k in range(nz) for j in range(ny)) for i in range(nx+1)]
  assert max(abs(q/.008-1) for q in flux)<1e-9
  # Separate native executable, full fields rather than selected diagnostics.
  binary=root/(name+'.bin');start=time.monotonic()
  native=subprocess.run([str(ROOT/'build/c3d-obstacle/obstacle3d_test'),str(n),str(L),str(cx),str(binary)],capture_output=True,text=True,check=True)
  native_row=json.loads(native.stdout);data=binary.read_bytes();assert struct.unpack_from('=3i',data)==tuple(N)
  raw=struct.unpack_from('='+str(4*nx*ny*nz+ny*nz)+'d',data,12);difference=0
  for q in range(nx*ny*nz):
   for a in range(3):difference=max(difference,abs(v[q][a]-raw[4*q+a]))
   if mask[q]:assert math.isnan(raw[4*q+3])
   else:difference=max(difference,abs(p[q]-raw[4*q+3]))
  difference=max(difference,max(abs(x-y) for x,y in zip(upper,raw[4*nx*ny*nz:])))
  assert difference<1e-10
  force=state['boundary_force_budget'];energy=state['energy_budget'];health=state['health']
  assert health['linear_relative_residual']<=1e-11 and energy['discrete_relative_imbalance']<1e-9
  assert abs(force['discrete_momentum_residual_n'][0])/(4*state['physics']['pressure_drop_pa'])<1e-9
  record={'case':name,'request':opts,'status':state,'assessment':service.run_assess(run),'result':result,'artifact_sha256':artifact['sha256'],'readback_max_difference':difference,'independent_export_divergence':maxdiv,'native':native_row,'native_wall_s':time.monotonic()-start,'agent_wall_s':agent_wall_s,'binary_sha256':sha(binary)}
  records.append(record);(root/'progress.json').write_text(json.dumps({'cases':len(records),'required':7,'last':name})+'\n')
  print(json.dumps({'case':name,'readback':difference,'agent_wall_s':record['agent_wall_s'],'physical_budget':record['assessment']['physical_budget']['status']}),flush=True)
 output={'schema':'physics_sim_c3d8_agent_evidence_v1','worker_sha256':identity,'evidence_root':str(root),'agent_matrix_and_field_readback_passed':True,'physical_boundary_gate_passed':False,'records':records}
 encoded=json.dumps(output,indent=2)+'\n';(root/'qualification.json').write_text(encoded);(ROOT/'build/c3d-obstacle/agent-evidence/qualification.json').write_text(encoded)
 print(json.dumps({'cases':7,'agent_matrix_and_field_readback_passed':True,'physical_boundary_gate_passed':False}),flush=True)
if __name__=='__main__':main()
