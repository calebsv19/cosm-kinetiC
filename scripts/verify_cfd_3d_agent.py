#!/usr/bin/env python3
"""Retained finite 3D source-checkout session evidence and matched cost."""
import json
import hashlib
import os
from pathlib import Path
import sys
import time
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent/'agent_session'))
from service import Service,TERMINAL


def wait(service,run):
    deadline=time.monotonic()+60
    while time.monotonic()<deadline:
        row=service.run_inspect(run)
        if row['state'] in TERMINAL:
            if run in service.children:service.children[run].wait(timeout=10)
            return row
        time.sleep(.02)
    raise RuntimeError('bounded local worker timeout')


def main():
    base=Path('build/c3d/agent-evidence');worker=Path(os.environ['PHYSICS_SIM_SESSION_WORKER'])
    identity=hashlib.sha256(worker.read_bytes()).hexdigest()
    root=base/identity[:16];s=Service(root);records=[]
    duct=s.scene_create('duct','cfd_duct_3d',dimensions=[4,2,2])
    options=dict(scene_id='duct',scene_revision=duct['scene_revision'],steps=1,dt=.005,start_paused=False,
                 fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=64)
    for n in (8,16,32):
        run=f'duct-{n}';start=time.monotonic();s.run_start(run,grid=[2*n,n,n],**options);row=wait(s,run)
        assert row['state']=='completed',row
        assessment=s.run_assess(run);assert assessment['reference_accuracy']['status']==('passed' if n==32 else 'failed')
        records.append({'run_id':run,'state':row['state'],'grid':row['effective_grid'],'assessment':assessment,
                        'physical_observations':row['physics'],'energy':row['energy_budget'],'health':row['health'],
                        'step_wall_ms':row['step_ms'],'publication':row['runtime_cost'],
                        'total_wall_s':time.monotonic()-start,'result':s.run_result(run)})
    duct_comparison=s.run_compare(['duct-8','duct-16','duct-32'])
    flow=s.scene_create('transient','cfd_manufactured_3d',dimensions=[2,2.5,3])
    options=dict(scene_id='transient',scene_revision=flow['scene_revision'],start_paused=False,
                 fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=64)
    for run,n,dt,steps in [('flow-8',8,.005,80),('flow-16',16,.005,80),('flow-32',32,.005,80),('time-16',16,.01,40)]:
        start=time.monotonic();s.run_start(run,grid=[n,n,n],dt=dt,steps=steps,**options);row=wait(s,run)
        assert row['state']=='completed',row;assert s.run_assess(run)['numerical_status']=='passed'
        records.append({'run_id':run,'state':row['state'],'grid':row['effective_grid'],'assessment':s.run_assess(run),
                        'qualification':row['qualification'],'energy':row['energy_budget'],'health':row['health'],
                        'step_wall_ms':row['step_ms'],'publication':row['runtime_cost'],
                        'total_wall_s':time.monotonic()-start,'result':s.run_result(run)})
    output={'evidence_root':str(root.resolve()),'worker_sha256':identity,'schema':'physics_sim_c3d_agent_evidence_v1','records':records,'duct_comparison':duct_comparison,
            'transient_spatial_comparison':s.run_compare(['flow-8','flow-16','flow-32']),
            'transient_time_comparison':s.run_compare(['time-16','flow-16'],'temporal'),
            'matched_duct_accuracy':'1% pressure/velocity; 2% wall shear/physical dissipation; first accepted grid 64x32x32',
            'scope':'local optimized source worker only; numerical budget excludes JSON/RSS; no desktop install',
            'passed':True}
    (root/'qualification.json').write_text(json.dumps(output,indent=2)+'\n')
    (base/'qualification.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps([{k:r[k] for k in ('run_id','grid','step_wall_ms','total_wall_s')} for r in records]))

if __name__=='__main__':main()
