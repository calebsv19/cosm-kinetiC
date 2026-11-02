"""Live fine-grid gust/wake runner using the normal atomic native Framework.

SQLite checkpoints are numerical authority. Selected f64 field snapshots are
convenient analysis views; every return payload is held from the accepted prior
checkpoint. Successful copied attempt payloads may retire after journal commit.
"""
import array
import base64
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import sqlite3
import sys
import time
from .framework import Framework
from .wind_presets import configuration
from .wind_sensitivity import refined
from .velocity import observation
from .kinetic_energy import NAMES
from surface_sources.growth_fire_v1 import require


def write(path,value):
    pending=path.with_suffix(path.suffix+'.pending');pending.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n');os.replace(pending,path)


def retire_attempts(root,receipts):
    # Caller owns the fresh experiment root exclusively. Native wrapper already
    # records terminal child/descendant verification; lsof is the second check.
    from verification_retention import consumers
    parent=root/'atmosphere-attempts'
    if not parent.exists():return
    receipts.mkdir(exist_ok=True)
    for capsule in parent.iterdir():
        state=json.loads((capsule/'receipt.json').read_text())
        if state['status']!='completed':continue
        require(state['terminal_processes_verified'] is True,'attempt process completion')
        held=consumers(capsule);require(not held,'attempt cleanup held: '+str(held))
        entries={}
        for path in capsule.rglob('*'):
            if not path.is_file():continue
            with path.open('rb') as stream:sha=hashlib.file_digest(stream,'sha256').hexdigest()
            entries[str(path.relative_to(capsule))]={'bytes':path.stat().st_size,'sha256':sha}
        require(not any(p.is_symlink() for p in capsule.rglob('*')),'attempt symlink refusal')
        write(receipts/(capsule.name+'.json'),{'receipt':state,'retired_files':entries,'reason':'Accepted joint SQLite checkpoint preserves exact source, fields and configuration; copied input/output/source staging is redundant','active_consumers':False})
        shutil.rmtree(capsule)


def record(cp,c,fire):
    f=cp['fluid'];fields=f['fields'];held=cp['evidence'].get('held_return')
    native=fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
    heat=native['fields']['heat'];areas=fire.areas(c['source']);weights=[a*h for a,h in zip(areas,heat)];nx=c['source']['width']
    sample=observation(cp,c,cp['evidence']['window_ticks'][1],0,height_m=.125)
    vectors=sample['native_velocity_m_s']
    return {'revision':cp['revision'],'time_s':fields['time_s'],'checkpoint_digest':cp['digest'],
        'source_digest':cp['source']['digest'],'source_frame_digest':None if cp['source']['frame'] is None else cp['source']['frame']['digest'],
        'fire_tick':native['tick'],'gas_steps':f['state']['data']['steps'],
        'held_from_checkpoint':None if held is None else held['observation']['joint_checkpoint_digest'],
        'wind_consumed_digest':None if held is None else cp['source']['operation']['wind_digest'],
        'return_mean_x_m_s':math.fsum(v[0] for v in vectors)/len(vectors),
        'return_vectors_m_s':vectors,'fire_heat_centroid_x_m':math.fsum(w*(c['source']['origin_m'][0]+q%nx*c['source']['cell_size_m']) for q,w in enumerate(weights))/math.fsum(weights),
        'fire_heat_proxy_integral':math.fsum(weights),'gas_energy_j':math.fsum(fields['energy_j']),
        'gas_smoke_kg':math.fsum(fields['smoke_kg']),'kinetic_stored_j':fields['kinetic_stored_j'],
        'kinetic_ledger_j':dict(zip(NAMES,fields['kinetic_ledger_j'])),
        'kinetic_closure_j':fields['kinetic_stored_j']-math.fsum(fields['kinetic_ledger_j']),
        'scalar_budgets':f['budgets'],'max_divergence_s_inv':fields['max_divergence_s_inv'],
        'projection_relative_residual':fields['projection_relative_residual'],'numerical_peak_bytes':fields['numerical_peak_bytes']}


def run_case(workers,output,grid,box=False,*,journal_codec='lossless_zlib/v1',native_packet_steps=20):
    require(type(native_packet_steps) is int and 1<=native_packet_steps<=20,'live native packet bound')
    output=Path(output);output.mkdir(parents=True,exist_ok=False)
    c=refined(configuration(workers,'abrupt_gust',duration=3.2,exchange=.1),grid)
    c['source']['run_id']='quiescent-live-gust'
    if box:c['fluid']['solid_box']={'schema':'physics_sim_stationary_box/v1','lower_m':[.375]*3,'upper_m':[.625]*3,'velocity_boundary':'no_slip','thermal_boundary':'insulated'}
    c['fluid']['wind_forcing']['momentum_backflow']='quiescent_exterior/v1'
    c['journal_codec']=journal_codec
    if native_packet_steps<20:c['native_packet_steps']=native_packet_steps
    c['fluid']['resource_limits']={'max_cells':max(32768,grid**3),'scalar_work_cells':1000000000,'numerical_bytes':512*1024*1024,'cache_pressure_operator':True,'native_request_bytes':256*1024*1024,'native_output_bytes':256*1024*1024}
    write(output/'config.json',c)
    attempts=output/'native-attempts';os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(attempts)
    start=time.monotonic();f=Framework(c,output/'run');cp=f.initialize();rows=[record(cp,c,f.fire)]
    retire_attempts(attempts,output/'attempt-receipts')
    for rev in range(c['windows']):
        write(output/'progress.json',{'status':'running','revision':rev,'grid':grid,'box':box,'elapsed_s':time.monotonic()-start})
        f.step(rev,'live-window-'+str(rev));cp=f.inspect()
        rows.append(record(cp,c,f.fire));write(output/'observations.json',rows)
        if rev+1 in (4,8,12,16,24,32):
            fields=cp['fluid']['fields'];path=output/('fields-%02d.bin'%(rev+1))
            with path.open('xb') as stream:
                for name in ('face_velocity_m_s','energy_j','smoke_kg'):array.array('d',fields[name]).tofile(stream)
        retire_attempts(attempts,output/'attempt-receipts')
        require(sum(p.stat().st_size for p in output.parent.rglob('*') if p.is_file())<4*1024**3,'live study4GiB output bound')
        print(output.name,rev+1,cp['fluid']['fields']['time_s'],flush=True)
    # Independent object discards in-session validation memo and revalidates all
    # retained native/source checkpoints, consumption, clock, and chain hashes.
    reopened=Framework(c,output/'run');head=reopened.inspect();require(head['digest']==cp['digest'],'independent joint restart identity')
    summary={'status':'completed','grid':grid,'box':box,'elapsed_s':time.monotonic()-start,'head_digest':cp['digest'],'reopened_head_digest':head['digest'],'windows':len(rows)-1,'final':rows[-1],'max_divergence_s_inv':max(v['max_divergence_s_inv'] for v in rows),'max_projection_residual':max(v['projection_relative_residual'] for v in rows),'max_abs_kinetic_closure_j':max(abs(v['kinetic_closure_j']) for v in rows),'journal_bytes':f.path.stat().st_size,'live_fire_feedback':True,'physical_calibration_qualified':False,'spatial_convergence_qualified':False}
    write(output/'summary.json',summary);write(output/'progress.json',{'status':'completed','windows':32,'elapsed_s':time.monotonic()-start})
    return summary
