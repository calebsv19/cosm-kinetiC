#!/usr/bin/env python3
"""Retain C3D-7 A/B/C agent matrices; completion needs a separate field audit."""
import hashlib
import json
import os
import shutil
from pathlib import Path
import sys
import time
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts/agent_session'))
from service import Service, TERMINAL


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, data):
    temp=path.with_suffix('.tmp')
    temp.write_text(json.dumps(data,indent=2,allow_nan=False)+'\n')
    temp.replace(path)


def cases():
    rows=[]
    for label,template in [('a','cfd_wall_stokes_3d'),('b','cfd_wall_transport_3d')]:
        for n in (8,16,32):
            rows.append((label+'-spatial'+str(n),template,[2,2.5,3],[n,n,n],.0025,.4,label+'-spatial'+str(n)+'.bin'))
        for dt in (.04,.02,.01,.005):
            name=label+'-time'+str(dt)
            rows.append((name,template,[2,2.5,3],[16,16,16],dt,.4,name+'.bin'))
    rows.append(('b-phase32','cfd_wall_transport_3d',[2,2.5,3],[32,32,32],.02,1,'b-phase32.bin'))
    for n in (8,16,32):
        for t in (.5,2,12):
            rows.append(('c-spatial'+str(n)+'-t'+format(t,'.6g'),'cfd_pressure_startup_3d',[4,2,2],[2*n,n,n],.02,t,
                         'c-spatial'+str(n)+'-t'+format(t,'.6g')+'.bin'))
    for dt in (.05,.025,.0125,.00625,.003125):
        for t in (.5,2):
            name='c-time'+str(dt)+'-t'+format(t,'.6g')
            rows.append((name,'cfd_pressure_startup_3d',[4,2,2],[32,16,16],dt,t,name+'.bin'))
    for length in (6,8):
        for t in (.5,2):
            name='c-outlet'+str(length)+'-t'+format(t,'.6g')
            rows.append((name,'cfd_pressure_startup_3d',[length,2,2],[length*16,32,32],.02,t,name+'.bin'))
    for n in (8,16,32):
        name='co-spatial'+str(n)
        rows.append((name,'cfd_open_wall_transient_3d',[4,2,2],[2*n,n,n],.005,.4,name+'.bin'))
    for dt in (.04,.02,.01,.0025):
        name='co-time'+str(dt)
        rows.append((name,'cfd_open_wall_transient_3d',[4,2,2],[32,16,16],dt,.4,name+'.bin'))
    for length in (6,8):
        name='co-outlet'+str(length)
        rows.append((name,'cfd_open_wall_transient_3d',[length,2,2],[length*16,32,32],.005,.4,name+'.bin'))
    return rows


def main():
    worker=Path(os.environ['PHYSICS_SIM_SESSION_WORKER']).resolve()
    identity=sha(worker)
    base=ROOT/'build/c3d-wall/agent-evidence'
    root=base/identity
    root.mkdir(parents=True,exist_ok=True)
    frozen=root/'worker_snapshot'
    if not frozen.exists():shutil.copy2(worker,frozen)
    assert sha(frozen)==identity
    service=Service(root,frozen)
    records_dir=root/'records';records_dir.mkdir(exist_ok=True)
    records=[]
    overrides_path=root/'verification-retries.json'
    overrides=json.loads(overrides_path.read_text()) if overrides_path.exists() else {}
    for name,template,length,grid,dt,t,native in cases():
        # Run IDs exclude the dot in decimal timesteps; immutable request identity
        # still includes the exact dt and physical end time.
        retry=overrides.get(name)
        run_id=retry.get('run_id') if retry else name.replace('.','p')
        if retry:
            predecessor=root/'runs'/retry['superseded_run_id']
            assert sha(predecessor/'snapshot.json')==retry['superseded_snapshot_sha256']
            assert sha(predecessor/'events.jsonl')==retry['events_sha256']
            assert sha(predecessor/'output/channel_fields.json')==retry['completed_field_sha256']
        retained=records_dir/(run_id+'.json')
        if retained.exists():
            record=json.loads(retained.read_text())
            assert record['worker_sha256']==identity
            for artifact in record['result']['artifacts']:
                assert sha(Path(artifact['path']))==artifact['sha256']
            records.append(record)
            print(json.dumps({'case':name,'retained':True}),flush=True)
            continue
        scene_id=template+'-L'+str(length[0])
        scene=service.scene_create(scene_id,template,dimensions=length)
        steps=round(t/dt);assert abs(steps*dt-t)<1e-10
        request=dict(scene_id=scene_id,scene_revision=scene['scene_revision'],grid=grid,dt=dt,steps=steps,
                     fluid={'density_kg_m3':1,'dynamic_viscosity_pa_s':.1},numerical_memory_limit_mib=512,start_paused=False)
        fresh=not (root/'runs'/run_id/'request.json').exists()
        begin=time.monotonic()
        assert sha(frozen)==identity
        service.run_start(run_id,**request)
        deadline=time.monotonic()+1200
        while True:
            status=service.run_inspect(run_id,history=True)
            if status['state'] in TERMINAL:break
            if time.monotonic()>deadline:
                raise RuntimeError('worker observation timeout; inspect the existing run before continuation: '+run_id)
            time.sleep(.05)
        if run_id in service.children:service.children[run_id].wait(timeout=10)
        if status['state']!='completed':
            write(root/(run_id+'-failure.json'),status)
            raise RuntimeError(f"{name}: {status['state']} / {status.get('error')} at tick {status.get('tick')}; retained failure snapshot")
        saved_request=json.loads((root/'runs'/run_id/'request.json').read_text())
        assert saved_request['worker_sha256']==identity
        assert Path(saved_request['worker_path'])==frozen
        for point in status.get('history',[]):
            budget=point.get('energy_budget',{})
            if not budget.get('available'):continue
            reconstructed=(budget['physical_boundary_power_w']+budget['body_force_power_w']-
                           budget['transport_power_w']-budget['physical_strain_dissipation_w']-
                           budget['kinetic_energy_rate_w'])
            assert abs(reconstructed-budget['residual_w'])<1e-12,(name,point['tick'])
        assert status['tick']==steps and abs(status['simulation_time']-t)<1e-9
        assessment=service.run_assess(run_id)
        assert assessment['numerical_status']=='passed',assessment
        if name in ('a-spatial32','b-spatial32','co-spatial32','co-outlet6','co-outlet8') or name.startswith('c-spatial32') or name.startswith('c-outlet'):
            assert assessment['reference_accuracy']['status']=='passed',assessment
        if name=='b-phase32':assert assessment['harmonic_accuracy']['status']=='passed',assessment
        result=service.run_result(run_id)
        for artifact in result['artifacts']:
            assert sha(Path(artifact['path']))==artifact['sha256']
        if retry:
            prior=json.loads((root/'runs'/retry['superseded_run_id']/'request.json').read_text())
            assert prior['request_fingerprint']==saved_request['request_fingerprint']
            new_field=next(a for a in result['artifacts'] if a['path'].endswith('channel_fields.json'))
            assert json.loads(Path(new_field['path']).read_text())['cartesian_fields']==json.loads((predecessor/'output/channel_fields.json').read_text())['cartesian_fields']
        record={'case':name,'run_id':run_id,'verification_retry':retry,'worker_sha256':identity,'request':request,'status':status,
                'assessment':assessment,'result':result,'native_field':str(ROOT/'build/c3d-wall'/native),
                'total_wall_s':time.monotonic()-begin if fresh else None}
        write(retained,record);records.append(record)
        write(root/'progress.json',{'records_completed':len(records),'records_required':len(cases()),'last_case':name,'goal_complete':False})
        print(json.dumps({'case':name,'wall_s':record['total_wall_s'],'step_ms':status['step_ms'],
                          'reference':assessment['reference_accuracy']['status'],'checkpoint_ms':status['runtime_cost']['checkpoint_service_wall_ms']}),flush=True)
    output={'schema':'physics_sim_c3d_transient_agent_evidence_v1','worker_sha256':identity,
            'evidence_root':str(root),'agent_matrix_passed':True,'goal_complete':False,
            'requires':'independent exported field, convergence/outlet and full requirement audit', 'records':records}
    write(root/'qualification.json',output);write(base/'qualification.json',output)
    print(json.dumps({'agent_matrix_passed':True,'cases':len(records),'evidence':str(base/'qualification.json')}),flush=True)


if __name__=='__main__':main()
