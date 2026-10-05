#!/usr/bin/env python3
"""Admit and run a bounded offline periodic passive-atmosphere experiment."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
from surface_sources.growth_fire_v1 import strict_load,keys,number,integer,require,sealed
ROOT=Path(__file__).resolve().parents[1]
SCHEMA='physics_sim_passive_transport_request/v1'
MODEL='periodic_constant_property_passive3d_v1'

def validate(r):
    keys(r,('schema','grid','length_m','properties','initial_energy_j','initial_smoke_kg','steps'))
    require(r['schema']==SCHEMA,'unsupported passive request')
    for name in ('grid','length_m'):
        require(type(r[name]) is list and len(r[name])==3,'grid/length vectors')
    for v in r['grid']:integer(v,4,256)
    for v in r['length_m']:number(v,.001,1000)
    n=math.prod(r['grid']);require(n<=262144,'cell resource bound')
    keys(r['properties'],('density_kg_m3','heat_capacity_j_kg_k','reference_temperature_k',
        'conductivity_w_m_k','tracer_diffusivity_m2_s'))
    for key,v in r['properties'].items():number(v,0 if key in ('conductivity_w_m_k','tracer_diffusivity_m2_s') else 1e-9,1e9)
    def array(a,count,lo,hi):
        require(type(a) is list and len(a)==count,'field dimensions')
        for v in a:number(v,lo,hi)
    for key in ('initial_energy_j','initial_smoke_kg'):array(r[key],n,0,1e12)
    require(type(r['steps']) is list and 1<=len(r['steps'])<=256,'step count bound')
    for step in r['steps']:
        keys(step,('dt_s','face_velocity_m_s','energy_j','smoke_kg'))
        number(step['dt_s'],1e-12,1)
        array(step['face_velocity_m_s'],3*n,-1e6,1e6)
        for key in ('energy_j','smoke_kg'):array(step[key],n,0,1e12)
    return r

def run(request,worker):
    validate(request)
    worker=Path(worker).resolve();worker_hash=hashlib.sha256(worker.read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix='passive-atmosphere-') as temp:
        p=Path(temp)/'request.json';p.write_text(json.dumps(request,allow_nan=False,separators=(',',':')))
        require(p.stat().st_size<=64*1024*1024,'request byte resource bound')
        executed=subprocess.run([str(worker),str(p)],capture_output=True,text=True,timeout=120)
        require(executed.returncode==0,executed.stderr.strip() or 'worker failed')
        result_path=Path(temp)/'fields.json';result_path.write_text(executed.stdout)
        fields=strict_load(result_path)
    require(worker_hash==hashlib.sha256(worker.read_bytes()).hexdigest(),'worker changed during execution')
    require(fields['schema']=='physics_sim_passive_fields/v1' and fields['model']==MODEL,'worker model mismatch')
    n=math.prod(request['grid']);volume=math.prod(l/g for l,g in zip(request['length_m'],request['grid']))
    prop=request['properties'];capacity=prop['density_kg_m3']*prop['heat_capacity_j_kg_k']*volume
    budgets={}
    for quantity,field in (('energy_j','energy_j'),('smoke_kg','smoke_kg')):
        initial=math.fsum(request['initial_'+quantity]);input_total=math.fsum(math.fsum(s[quantity]) for s in request['steps'])
        require(type(fields[field]) is list and len(fields[field])==n,'result field dimensions')
        for v in fields[field]:number(v,0)
        stored=math.fsum(fields[field]);balance=stored-initial-input_total
        require(abs(balance)<=1e-10*max(initial+input_total,1e-12),'independent final conservation')
        budgets[quantity]={'initial':initial,'input':input_total,'stored':stored,'outflow':0,'loss':0,'balance':balance}
    for key in ('temperature_k','smoke_concentration_kg_m3'):
        require(type(fields[key]) is list and len(fields[key])==n,'derived field dimensions')
        for v in fields[key]:number(v,0)
    for i in range(n):
        require(math.isclose(fields['temperature_k'][i],prop['reference_temperature_k']+fields['energy_j'][i]/capacity,rel_tol=1e-13),'temperature conversion')
        require(math.isclose(fields['smoke_concentration_kg_m3'][i],fields['smoke_kg'][i]/volume,rel_tol=1e-13,abs_tol=1e-30),'tracer conversion')
    require(math.isclose(fields['time_s'],math.fsum(s['dt_s'] for s in request['steps']),rel_tol=1e-13),'accepted time')
    return sealed({'schema':'physics_sim_passive_experiment/v1','status':'completed','model':MODEL,
        'worker_sha256':worker_hash,'request':request,'fields':fields,'budgets':budgets,
        'boundaries':'periodic_xyz','sampling':'cell_centered_x_fast; lower_face_velocity_component_blocks',
        'source_mechanism':'integrated_volumetric_cell_deposition','momentum_feedback':False,
        'checkpoint_restart':False,'buoyancy_qualified':False,'ash_model':False})

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--worker',type=Path,default=ROOT/'build/passive-atmosphere/physics_sim_passive_worker')
    p.add_argument('--request',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    require(not a.output.exists(),'choose a new result path')
    result=run(strict_load(a.request),a.worker)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    # Offline experiment artifact, no consuming receiver journal or restart claim.
    with tempfile.NamedTemporaryFile(mode='w',dir=a.output.parent,prefix='.passive-',delete=False) as f:
        temp=Path(f.name)
        try:
            json.dump(result,f,allow_nan=False,separators=(',',':'));f.write('\n');f.flush();os.fsync(f.fileno())
            os.link(temp,a.output)
            directory=os.open(a.output.parent,os.O_RDONLY)
            try:os.fsync(directory)
            finally:os.close(directory)
        finally:temp.unlink(missing_ok=True)
    print(json.dumps({'status':'completed','result':str(a.output.resolve()),'digest':result['digest'],'budgets':result['budgets']}))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as e:raise SystemExit(str(e))
