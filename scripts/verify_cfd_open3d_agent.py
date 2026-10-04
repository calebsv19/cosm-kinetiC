#!/usr/bin/env python3
"""Retained C3D-6 source session evidence with exact executable/artifact identity."""
import hashlib
import json
import os
from pathlib import Path
import sys
import time
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service,TERMINAL

def wait(service,run):
    deadline=time.monotonic()+120
    while time.monotonic()<deadline:
        row=service.run_inspect(run)
        if row['state'] in TERMINAL:
            if run in service.children:service.children[run].wait(timeout=10)
            return row
        time.sleep(.05)
    raise RuntimeError('bounded C3D-6 worker timeout')

def main():
    worker=Path(os.environ['PHYSICS_SIM_SESSION_WORKER']);identity=hashlib.sha256(worker.read_bytes()).hexdigest()
    base=ROOT/'build/c3d-open/agent-evidence';root=base/identity[:16];s=Service(root);records=[]
    retained=root/'qualification.json'
    if retained.exists():
        previous=json.loads(retained.read_text());assert previous['worker_sha256']==identity and previous['passed']
        for row in previous['records']:
            for artifact in row['result']['artifacts']:
                assert hashlib.sha256(Path(artifact['path']).read_bytes()).hexdigest()==artifact['sha256']
        (base/'qualification.json').write_text(json.dumps(previous,indent=2)+'\n')
        print('Existing complete digest-verified C3D-6 evidence retained; original cost measurements preserved.')
        return
    scene=s.scene_create('open','cfd_open_duct_3d',dimensions=[4,2,2])
    opts=dict(scene_id='open',scene_revision=scene['scene_revision'],steps=1,dt=.005,start_paused=False,
              fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=128)
    for n in (8,16,32):
        name='open-'+str(n);fresh=not (s.run_dir(name)/'snapshot.json').exists();start=time.monotonic();s.run_start(name,grid=[2*n,n,n],**opts);row=wait(s,name)
        assert row['state']=='completed',row;assessment=s.run_assess(name)
        assert assessment['reference_accuracy']['status']==('passed' if n==32 else 'failed'),assessment
        result=s.run_result(name)
        for artifact in result['artifacts']:
            assert hashlib.sha256(Path(artifact['path']).read_bytes()).hexdigest()==artifact['sha256']
        records.append({'run_id':name,'grid':row['effective_grid'],'assessment':assessment,'physics':row['physics'],'energy':row['energy_budget'],'force':row['boundary_force_budget'],'health':row['health'],
                        'step_wall_ms':row['step_ms'],'publication':row['runtime_cost'],'total_wall_s':time.monotonic()-start if fresh else None,'result':result})
    output={'schema':'physics_sim_c3d_open_agent_evidence_v1','passed':True,'worker_sha256':identity,'evidence_root':str(root),'records':records,'comparison':s.run_compare(['open-8','open-16','open-32']),
            'scope':'three-grid stationary straight open duct; outlet extension evidence in numerical qualification.json; no package/commit'}
    (root/'qualification.json').write_text(json.dumps(output,indent=2)+'\n');(base/'qualification.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps([{k:v[k] for k in ('run_id','step_wall_ms','total_wall_s')} for v in records]))

if __name__=='__main__':main()
