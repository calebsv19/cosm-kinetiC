#!/usr/bin/env python3
"""Reproducible local box scene, two grids, inspection and complete SI readback.

Uses the actual scene/session service and its immutable revision/worker controls.
Forces remain provisional. No package/install or general transient/wake claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import sys
import time
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts/agent_session'))
from service import Service, TERMINAL
from protocol import call
from run_cfd_native_accuracy_regression import execute, require, save, sha
from run_cfd_native_box import FILES as NATIVE_FILES

FRONTEND = ('scripts/run_cfd_box_scene.py', 'scripts/physics_sim_session.py',
            'scripts/agent_session/service.py', 'scripts/agent_session/protocol.py',
            'scripts/agent_session/cartesian3d.py', 'include/app/cfd_3d_session.h',
            'src/app/cfd_3d_session.c', 'src/app/cfd_obstacle3d_observation.c')
LOWER = (1.25, .75, .75)
UPPER = (2.75, 1.25, 1.25)
FLUID = {'density_kg_m3': 1, 'dynamic_viscosity_pa_s': .1}


def readback_file(fields, status, path):
    """Map every exported face/cell into the native compact topology, no solve."""
    n = [int(x) for x in status['effective_grid']]
    h = fields['spacing_m']
    lo = [round(LOWER[a]/h[a]) for a in range(3)]
    hi = [round(UPPER[a]/h[a]) for a in range(3)]
    mask, velocity, pressure = (fields[k] for k in
                               ('solid_mask', 'velocity_faces_m_s', 'pressure_pa'))
    require(len(mask) == len(velocity) == len(pressure) == n[0]*n[1]*n[2], 'All exported cells')
    def flat(c): return (c[2]*n[1]+c[1])*n[0]+c[0]
    def fluid(c):
        return all(0 <= c[a] < n[a] for a in range(3)) and not mask[flat(c)]
    def face(a,c):
        if a == 0 and c[0] == n[0]:
            return fields['outlet_x_velocity_m_s'][c[2]*n[1]+c[1]]
        if not all(0 <= c[b] < n[b] for b in range(3)): return 0.
        return velocity[flat(c)][a]
    values = []
    for a in range(3):
        shape = [n[b]+int(a==b) for b in range(3)]
        for k in range(shape[2]):
            for j in range(shape[1]):
                for i in range(shape[0]):
                    c = [i,j,k];lower = c.copy();lower[a] -= 1
                    active = fluid(c) and fluid(lower)
                    if a == 0 and i in (0,n[0]): active = fluid(c if i == 0 else lower)
                    if active: values.append(face(a,c))
    faces = len(values)
    maxdiv = 0.;flux = [0.]*(n[0]+1)
    for k in range(n[2]):
        for j in range(n[1]):
            for i in range(n[0]):
                c = [i,j,k];q = flat(c)
                actual_solid = all(lo[a] <= c[a] < hi[a] for a in range(3))
                require(mask[q] == actual_solid, 'Actual physical body mask')
                require((pressure[q] is None) == actual_solid, 'Null solid pressure')
                if actual_solid: continue
                values.append(pressure[q]);div = 0.
                for a in range(3):
                    upper = c.copy();upper[a] += 1
                    div += (face(a,upper)-face(a,c))/h[a]
                maxdiv = max(maxdiv, abs(div))
    for i in range(n[0]+1):
        flux[i] = sum(face(0,[i,j,k])*h[1]*h[2] for k in range(n[2]) for j in range(n[1]))
    flux_error = max(abs(q/.008-1) for q in flux)
    require(maxdiv < 1e-8 and flux_error < 1e-9, 'Independent complete export continuity/flux')
    cells = len(values)-faces
    with path.open('wb') as f:
        f.write(b'C3DBOX1\0');f.write(struct.pack('=11i',*n,*lo,*hi,faces,cells))
        f.write(struct.pack('=7d',4,2,2,1,.1,.008,status['physics']['pressure_drop_pa']))
        f.write(struct.pack('='+str(len(values))+'d',*values))
    return {'maximum_divergence_s_inv':maxdiv,'flux_relative_error':flux_error,
            'compact_velocity_faces':faces,'fluid_pressure_cells':cells,'field_sha256':sha(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', required=True)
    parser.add_argument('--worker', type=Path, default=ROOT/'build/c3d-box/workflow-build/physics_sim_session_worker')
    args = parser.parse_args()
    require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.name), 'Unique run identity')
    require(args.worker.is_file(), 'Build the source session worker first; see docs/cfd_3d_box_checkpoint.md')
    directory = ROOT/'build/c3d-box/scenes'/args.name
    directory.mkdir(parents=True, exist_ok=False)
    source = directory/'source'
    sources = {q:sha(ROOT/q) for q in dict.fromkeys((*NATIVE_FILES,*FRONTEND))}
    for q,h in sources.items():
        p=source/q;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((ROOT/q).read_bytes())
        require(sha(p)==h,'Frozen source '+q)
    worker=directory/'worker_snapshot';shutil.copy2(args.worker,worker);identity=sha(worker)
    contract=dict(body_min_m=LOWER,body_max_m=UPPER,dimensions_m=[4,2,2],volume_flow_m3_s=.008,
                  grids=[[48,24,24],[96,48,48]],fluid=FLUID,numerical_memory_limit_mib=256,
                  solver_cell_budget=262144,case_wall_cap_s=600,rss_cap_bytes=1536*1024**2,
                  full_SI_residual_max=1e-11,divergence_max=1e-8,flux_max=1e-9,
                  absolute_force_qualification=False,source_sha256=sources,worker_sha256=identity)
    save(directory/'contract.json',contract)
    result=dict(status='failed',source_sha256=sources,worker_sha256=identity,cases=[],
                physical_accuracy_certified=False,transient_or_wake_supported=False)
    service=Service(directory/'session',worker=worker)
    active=None
    try:
        cc=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-DCFD_MIXED3D_VERIFY',
            '-I'+str(source/'include'),str(source/'tests/cfd_obstacle3d_box_field_probe.c')]
        cc += [str(source/q) for q in NATIVE_FILES if q.startswith('src/') and not q.endswith('cfd_obstacle3d_mixed.c')]
        cc += ['-lm','-o',str(directory/'readback')]
        result['compile']=execute(cc,directory,'compile-readback',60,1024**3)
        scene=call(service,'scene_create',dict(scene_id='long-box',template='cfd_box_3d',dimensions=[4,2,2],
                    channel={'body_min_m':list(LOWER),'body_max_m':list(UPPER),'volume_flow_m3_s':.008}))
        result['scene']=scene;save(directory/'scene-response.json',scene)
        for grid in contract['grids']:
            run='grid-'+str(grid[1]);case=directory/run;case.mkdir()
            options=dict(scene_id=scene['scene_id'],scene_revision=scene['scene_revision'],grid=grid,steps=1,
                         dt=.01,fluid=FLUID,numerical_memory_limit_mib=256,solver_cell_budget=262144)
            validation=call(service,'scene_validate',{k:v for k,v in options.items() if k!='steps'});require(validation['valid'],'Real native initializer')
            call(service,'run_start',dict(options,request_id=run,start_paused=False));active=run
            start=time.monotonic();peak=0
            while True:
                status=service.run_inspect(run)
                if status['state'] in TERMINAL:break
                require(time.monotonic()-start<600,'Bounded scene case')
                pid=service.children[run].pid if run in service.children else None
                if pid:
                    import subprocess
                    sample=subprocess.run(['ps','-o','rss=','-p',str(pid)],capture_output=True,text=True,timeout=3)
                    if sample.returncode==0 and sample.stdout.strip():peak=max(peak,int(sample.stdout.strip())*1024)
                    require(peak<=1536*1024**2,'Sampled scene worker RSS')
                time.sleep(.2)
            if run in service.children:service.children[run].wait(timeout=30)
            require(status['state']=='completed', 'Terminal accepted scene '+str(status.get('error')));active=None
            require(service.run_inspect(run)['tick']==status['tick'],'Retained inspection does not advance flow')
            assessment=call(service,'run_assess',dict(run_id=run))
            require(assessment['numerical_status']=='passed' and not assessment['physical_accuracy_certified'],
                    'Numerical/absolute accuracy separation')
            outcome=call(service,'run_result',dict(run_id=run))
            for artifact in outcome['artifacts']:require(sha(Path(artifact['path']))==artifact['sha256'],'Session artifact identity')
            artifact=next(a for a in outcome['artifacts'] if a['path'].endswith('channel_fields.json'))
            field=json.loads(Path(artifact['path']).read_text())['cartesian_fields']
            pressure=[p for p in field['pressure_pa'] if p is not None]
            sample=dict(scope='retained complete SI field; terminal live sampler is unavailable',
                        pressure_min_pa=min(pressure),pressure_max_pa=max(pressure),
                        maximum_face_speed_m_s=max(max(abs(v) for v in row) for row in field['velocity_faces_m_s']),
                        solid_cells=sum(field['solid_mask']),tick=status['tick'],simulation_time_s=status['simulation_time'])
            export=readback_file(field,status,case/'field.bin')
            process=execute([str(directory/'readback'),'--readback',str(case/'field.bin')],case,'readback',180,1536*1024**2)
            check=json.loads((case/'readback.stdout').read_text())
            for key in ('pressure_force_n','viscous_force_n'):
                observed=status['boundary_force_budget']['body_'+key]
                require(max(abs(a-b) for a,b in zip(observed,check[key]))<1e-12,'Complete exported force readback '+key)
            require(check['pressure_drop_pa']==status['physics']['pressure_drop_pa'],'Pressure SI readback')
            save(case/'status.json',status);save(case/'sample.json',sample);save(case/'assessment.json',assessment)
            result['cases'].append(dict(run_id=run,grid=grid,status=status,assessment=assessment,
                 result=outcome,export_readback=export,native_equation_readback=check,readback_process=process,
                 sampled_peak_rss_bytes=peak,case_wall_s=time.monotonic()-start))
            print(json.dumps(dict(case=run,numerical=assessment['numerical_status'],physical_budget=assessment['physical_budget']['status'],absolute_accuracy=False)),flush=True)
        result['comparison']=call(service,'run_compare',dict(run_ids=[x['run_id'] for x in result['cases']],kind='spatial'))
        save(directory/'comparison.json',result['comparison'])
        for q,h in sources.items():require(sha(ROOT/q)==h==sha(source/q),'Final source identity '+q)
        require(sha(worker)==identity==sha(args.worker),'Worker identity preserved')
        result['status']='completed_local_stationary_box_scene_and_full_equation_readback'
    except Exception as error:
        result['failure']=str(error)
    finally:
        if active:
            try:service.run_control(active,'cleanup','cancel',result['scene']['scene_revision'],5000)
            except Exception:pass
            if active in service.children:
                import subprocess
                child=service.children[active]
                try:child.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    child.terminate()
                    try:child.wait(timeout=10)
                    except subprocess.TimeoutExpired:child.kill();child.wait(timeout=10)
        result['artifact_sha256']={str(p.relative_to(directory)):sha(p) for p in directory.rglob('*') if p.is_file() and p.name!='service.lock'}
        save(directory/'receipt.json',result)
    print(json.dumps(dict(status=result['status'],receipt=str(directory/'receipt.json'),failure=result.get('failure'))),flush=True)
    return 0 if result['status'].startswith('completed') else 1

if __name__=='__main__':raise SystemExit(main())
