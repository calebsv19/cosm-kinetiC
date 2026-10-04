#!/usr/bin/env python3
"""Admitted finest extended-domain correction check, immutable worker and full readback."""
import hashlib,json,os,shutil,struct,subprocess,sys,time,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.dont_write_bytecode=True
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,TERMINAL

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 worker=Path(os.environ['PHYSICS_SIM_SESSION_WORKER']).resolve();digest=sha(worker)
 root=ROOT/'build/c3d-obstacle/correction-v1/extended40'/digest;root.mkdir(parents=True,exist_ok=True)
 proof=root/'qualification.json'
 if proof.exists():
  r=json.loads(proof.read_text());assert r['worker_sha256']==digest
  for artifact in r['result']['artifacts']:assert sha(Path(artifact['path']))==artifact['sha256']
  print(json.dumps({'retained':str(proof)}));return
 frozen=root/'worker_snapshot'
 if not frozen.exists():shutil.copy2(worker,frozen)
 assert sha(frozen)==digest
 service=Service(root,worker=frozen);scene=service.scene_create('extended40','cfd_obstacle_3d',dimensions=[8,2,2],channel={'center_x_m':4})
 options=dict(scene_id='extended40',scene_revision=scene['scene_revision'],grid=[160,40,40],steps=1,dt=.01,fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=512)
 start=time.monotonic()
 if not (root/'runs/extended40/request.json').exists():service.run_start('extended40',**options,start_paused=False)
 while True:
  state=service.run_inspect('extended40')
  if state['state'] in TERMINAL:break
  assert time.monotonic()-start<240,state
  time.sleep(.2)
 assert state['state']=='completed',state
 agent_wall=time.monotonic()-start;result=service.run_result('extended40')
 for a in result['artifacts']:assert sha(Path(a['path']))==a['sha256']
 artifact=next(a for a in result['artifacts'] if a['path'].endswith('channel_fields.json'))
 fields=json.loads(Path(artifact['path']).read_text())['cartesian_fields'];v,p,mask,upper=(fields[k] for k in ('velocity_faces_m_s','pressure_pa','solid_mask','outlet_x_velocity_m_s'))
 binary=root/'native.bin';start=time.monotonic()
 native=subprocess.run([str(ROOT/'build/c3d-obstacle/obstacle3d_test'),'40','8','4',str(binary)],capture_output=True,text=True,check=True)
 native_wall=time.monotonic()-start;row=json.loads(native.stdout);data=binary.read_bytes()
 assert struct.unpack_from('=3i',data)==(160,40,40)
 raw=struct.unpack_from('='+str(4*160*40*40+40*40)+'d',data,12);difference=0
 for q in range(160*40*40):
  for a in range(3):difference=max(difference,abs(v[q][a]-raw[4*q+a]))
  if mask[q]:assert p[q] is None and math.isnan(raw[4*q+3]) and all(x==0 for x in v[q])
  else:assert math.isfinite(p[q]);difference=max(difference,abs(p[q]-raw[4*q+3]))
 difference=max(difference,max(abs(x-y) for x,y in zip(upper,raw[4*160*40*40:])))
 assert difference==0
 assert sum(mask)==8000 and row['relative_residual']<1e-11 and row['divergence']<1e-8 and row['flux_error']<1e-9
 r=dict(worker_sha256=digest,request=options,status=state,result=result,assessment=service.run_assess('extended40'),native=row,
        readback_max_difference=difference,agent_wall_s=agent_wall,native_wall_s=native_wall,binary_sha256=sha(binary),
        schema='physics_sim_c3d8_extended40_v1',cells=256000)
 proof.write_text(json.dumps(r,indent=2)+'\n');(ROOT/'build/c3d-obstacle/correction-v1/extended40/qualification.json').write_text(json.dumps(r,indent=2)+'\n')
 print(json.dumps({'worker_sha256':digest,'readback':difference,'agent_wall_s':agent_wall,'native_wall_s':native_wall,'physical_budget':r['assessment']['physical_budget']['status']}))
if __name__=='__main__':main()
