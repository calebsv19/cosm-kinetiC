#!/usr/bin/env python3
"""Bounded native checkpoint/restart for periodic evolving flow and passive scalars."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile
from passive_atmosphere import validate as passive_validate,ROOT
from surface_sources.growth_fire_v1 import keys,number,integer,require,strict_load,digest,sealed
SCHEMA='physics_sim_evolving_atmosphere_request/v1'
MODEL='periodic_evolving_passive3d_v1'
STATE='physics_sim_native_atmosphere_state/v1'

def array(a,n,lo=None,hi=None):
    require(type(a) is list and len(a)==n,'field dimension')
    for v in a:number(v,lo,hi)

def configuration(r):return {k:r[k] for k in ('grid','length_m','properties','momentum_dt_s','initial_face_velocity_m_s')}

def validate(r):
    keys(r,('schema','grid','length_m','properties','momentum_dt_s','initial_face_velocity_m_s','state','steps'))
    require(r['schema']==SCHEMA,'request model')
    number(r['momentum_dt_s'],1e-6,.1)
    require(type(r['grid']) is list and len(r['grid'])==3,'grid vector')
    for v in r['grid']:integer(v,4,64)
    require(type(r['properties']) is dict,'properties object')
    props=dict(r['properties']);require('dynamic_viscosity_pa_s' in props,'viscosity required');number(props.pop('dynamic_viscosity_pa_s'),1e-9,1e6)
    n=math.prod(r['grid']);require(n<=32768,'evolving cell resource bound')
    zero=[0.]*n
    passive_validate({'schema':'physics_sim_passive_transport_request/v1','grid':r['grid'],'length_m':r['length_m'],'properties':props,
        'initial_energy_j':zero,'initial_smoke_kg':zero,'steps':[{'dt_s':r['momentum_dt_s'],'face_velocity_m_s':r['initial_face_velocity_m_s'],'energy_j':zero,'smoke_kg':zero}]})
    for v in r['grid']:integer(v,4,64)
    require(type(r['steps']) is list and len(r['steps'])<=256,'chunk step count')
    for step in r['steps']:
        keys(step,('energy_j','smoke_kg'))
        for name in ('energy_j','smoke_kg'):array(step[name],n,0,1e12)
    if r['state'] is not None:
        state=r['state'];keys(state,('schema','config_digest','worker_sha256','data','digest'))
        require(state['schema']==STATE and state['digest']==digest(state) and state['config_digest']==digest(configuration(r)),'state/config integrity')
        data=state['data'];keys(data,('flow_storage','energy_j','smoke_kg','steps','time_s','scalar_work_cells','input_energy_j','input_smoke_kg','previous_kinetic_j','older_kinetic_j','kinetic_j'))
        array(data['flow_storage'],27*n)
        for key in ('energy_j','smoke_kg'):array(data[key],n,0)
        integer(data['steps'],0,10000);integer(data['scalar_work_cells'],0,100000000)
        for key in ('time_s','input_energy_j','input_smoke_kg','previous_kinetic_j','older_kinetic_j','kinetic_j'):number(data[key],0)
        require(math.isclose(data['time_s'],data['steps']*r['momentum_dt_s'],abs_tol=1e-12,rel_tol=1e-12),'state clock')
    return r

def run(request,worker):
    validate(request);worker=Path(worker).resolve();worker_sha=hashlib.sha256(worker.read_bytes()).hexdigest()
    old=request['state'];start=0. if old is None else old['data']['time_s'];n=math.prod(request['grid'])
    if old is not None:require(old['worker_sha256']==worker_sha,'checkpoint worker bytes changed')
    native=dict(request,state=None if old is None else old['data'])
    with tempfile.TemporaryDirectory(prefix='evolving-atmosphere-') as temp:
        path=Path(temp)/'request.json';path.write_text(json.dumps(native,allow_nan=False,separators=(',',':')))
        require(path.stat().st_size<=64*1024*1024,'request byte bound')
        result=subprocess.run([str(worker),str(path)],capture_output=True,text=True,timeout=120)
        require(result.returncode==0,result.stderr.strip() or 'worker failed')
        path.write_text(result.stdout);fields=strict_load(path)
    require(worker_sha==hashlib.sha256(worker.read_bytes()).hexdigest(),'worker changed during step')
    require(fields['schema']=='physics_sim_evolving_atmosphere_fields/v1','native field model')
    data=fields.pop('state')
    # json-c uses numeric doubles for metadata; canonical state uses integral counters.
    for key in ('steps','scalar_work_cells'):
        require(type(data[key]) in (int,float) and data[key]==int(data[key]),'native counter');data[key]=int(data[key])
    state=sealed({'schema':STATE,'config_digest':digest(configuration(request)),'worker_sha256':worker_sha,'data':data})
    validate(dict(request,state=state,steps=[]))
    expected_steps=(0 if old is None else old['data']['steps'])+len(request['steps'])
    require(data['steps']==expected_steps and math.isclose(fields['time_s'],start+len(request['steps'])*request['momentum_dt_s'],rel_tol=1e-12,abs_tol=1e-12),'accepted step clock')
    budgets={}
    for key,input_key in (('energy_j','input_energy_j'),('smoke_kg','input_smoke_kg')):
        array(fields[key],n,0);require(fields[key]==data[key],'native checkpoint field identity')
        before=0 if old is None else math.fsum(old['data'][key]);added=math.fsum(math.fsum(s[key]) for s in request['steps']);stored=math.fsum(fields[key])
        balance=stored-before-added;require(abs(balance)<=1e-10*max(before+added,1e-12),'independent chunk conservation')
        cumulative=(0 if old is None else old['data'][input_key])+added
        require(math.isclose(data[input_key],cumulative,abs_tol=1e-12,rel_tol=1e-12),'cumulative input')
        budgets[key]={'initial':0,'input':cumulative,'stored':stored,'outflow':0,'loss':0,'balance':stored-cumulative,'chunk_balance':balance}
        require(abs(stored-cumulative)<=1e-10*max(cumulative,1e-12),'independent lifetime conservation')
    volume=math.prod(l/n for l,n in zip(request['length_m'],request['grid']));props=request['properties'];capacity=props['density_kg_m3']*props['heat_capacity_j_kg_k']*volume
    array(fields['face_velocity_m_s'],3*n);array(fields['pressure_pa'],n)
    require(fields['face_velocity_m_s']==data['flow_storage'][:3*n] and fields['pressure_pa']==data['flow_storage'][6*n:7*n],'native checkpoint momentum identity')
    for key in ('temperature_k','smoke_concentration_kg_m3'):array(fields[key],n,0)
    for q in range(n):
        require(math.isclose(fields['temperature_k'][q],props['reference_temperature_k']+fields['energy_j'][q]/capacity,rel_tol=1e-13),'temperature conversion')
        require(math.isclose(fields['smoke_concentration_kg_m3'][q],fields['smoke_kg'][q]/volume,rel_tol=1e-13,abs_tol=1e-30),'concentration conversion')
    velocity=fields['face_velocity_m_s'];grid=request['grid']
    for q in range(n):
        divergence=0.
        for axis in range(3):
            stride=1 if axis==0 else grid[0] if axis==1 else grid[0]*grid[1]
            coord=(q//stride)%grid[axis];upper=q+stride if coord+1<grid[axis] else q-(grid[axis]-1)*stride
            divergence+=(velocity[axis*n+upper]-velocity[axis*n+q])/(request['length_m'][axis]/grid[axis])
        require(math.isfinite(divergence) and abs(divergence)<1e-8,'independent accepted divergence')
    number(fields['max_divergence_s_inv'],0,1e-8);number(fields['momentum_relative_residual'],0,1e-11)
    return sealed({'schema':'physics_sim_evolving_atmosphere_experiment/v1','model':MODEL,'status':'completed','configuration':configuration(request),
        'start_time_s':start,'steps_advanced':len(request['steps']),'fields':fields,'state':state,'budgets':budgets,
        'boundaries':'periodic_xyz','momentum_feedback':False,'native_checkpoint_restart':True,'buoyancy_qualified':False,'ash_model':False})

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--request',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--worker',type=Path,default=ROOT/'build/evolving-atmosphere/physics_sim_atmosphere_worker');a=p.parse_args()
    require(not a.output.exists(),'fresh output path');result=run(strict_load(a.request),a.worker)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('x') as f:json.dump(result,f,allow_nan=False)
    print(json.dumps({'digest':result['digest'],'time_s':result['fields']['time_s'],'steps_advanced':result['steps_advanced'],'budgets':result['budgets']}))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as error:raise SystemExit(str(error))
