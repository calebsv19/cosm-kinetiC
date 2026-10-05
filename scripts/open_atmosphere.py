#!/usr/bin/env python3
"""Closed open-reservoir policy, native state continuation and independent SI flux budgets."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import tempfile
from evolving_atmosphere import array
from passive_atmosphere import ROOT
from surface_sources.growth_fire_v1 import keys,number,integer,require,strict_load,digest,sealed
SCHEMA='physics_sim_open_atmosphere_request/v1'
MODEL='xy_periodic_z_reservoir_projection3d_v1'
STATE='physics_sim_native_open_atmosphere_state/v1'
CONFIG_KEYS=('grid','length_m','properties','momentum_dt_s','initial_face_velocity_m_s','initial_energy_j','initial_smoke_kg','boundary_policy','buoyancy')

def configuration(r):return {k:r[k] for k in CONFIG_KEYS}

def validate(r):
    keys(r,('schema',*CONFIG_KEYS,'state','steps'));require(r['schema']==SCHEMA,'open model schema')
    for name in ('grid','length_m'):require(type(r[name]) is list and len(r[name])==3,'grid/length vector')
    for v in r['grid']:integer(v,4,64)
    for v in r['length_m']:number(v,.001,1000)
    n=math.prod(r['grid']);require(n<=32768,'open cell bound');plane=r['grid'][0]*r['grid'][1]
    number(r['momentum_dt_s'],1e-6,.1)
    props=r['properties'];keys(props,('density_kg_m3','dynamic_viscosity_pa_s','heat_capacity_j_kg_k','reference_temperature_k','conductivity_w_m_k','tracer_diffusivity_m2_s'))
    for key,v in props.items():number(v,0 if key in ('conductivity_w_m_k','tracer_diffusivity_m2_s') else 1e-9,1e9)
    boundary=r['boundary_policy'];keys(boundary,('schema','horizontal','vertical','predictor_velocity','pressure_datum_pa','inflow_temperature_k','inflow_smoke_concentration_kg_m3','scalar_diffusion'))
    require(boundary['schema']=='physics_sim_open_reservoir_boundary/v1' and boundary['horizontal']=='periodic_xy' and boundary['vertical']=='open_bottom_and_top' and
        boundary['predictor_velocity']=='zero_normal_gradient' and boundary['scalar_diffusion']=='zero_normal_flux','unsupported boundary policy')
    array(boundary['pressure_datum_pa'],2,-1e6,1e6);array(boundary['inflow_temperature_k'],2,props['reference_temperature_k'],1e9);array(boundary['inflow_smoke_concentration_kg_m3'],2,0,1e6)
    buoy=r['buoyancy'];keys(buoy,('enabled','gravity_m_s2','expansion_per_k','max_temperature_contrast_fraction'))
    require(type(buoy['enabled']) is bool,'buoyancy enabled flag');number(buoy['gravity_m_s2'],0,100);number(buoy['expansion_per_k'],0,1/props['reference_temperature_k']);number(buoy['max_temperature_contrast_fraction'],1e-6,.1)
    if buoy['enabled']:
        for t in boundary['inflow_temperature_k']:require((t-props['reference_temperature_k'])/props['reference_temperature_k']<=buoy['max_temperature_contrast_fraction'],'reservoir outside contrast envelope')
    array(r['initial_face_velocity_m_s'],3*n+plane,-1e6,1e6)
    for key in ('initial_energy_j','initial_smoke_kg'):array(r[key],n,0,1e12)
    require(type(r['steps']) is list and len(r['steps'])<=256,'new step count')
    for step in r['steps']:
        keys(step,('energy_j','smoke_kg'))
        for key in ('energy_j','smoke_kg'):array(step[key],n,0,1e12)
    if r['state'] is not None:
        state=r['state'];keys(state,('schema','config_digest','worker_sha256','data','digest'))
        require(state['schema']==STATE and state['digest']==digest(state) and state['config_digest']==digest(configuration(r)),'open checkpoint/config identity')
        data=state['data'];keys(data,('face_velocity_m_s','pressure_pa','energy_j','smoke_kg','boundary_fluxes','steps','time_s','scalar_work_cells','input_energy_j','input_smoke_kg','initial_energy_j','initial_smoke_kg'))
        array(data['face_velocity_m_s'],3*n+plane);array(data['pressure_pa'],n)
        for key in ('energy_j','smoke_kg'):array(data[key],n,0)
        array(data['boundary_fluxes'],8*plane,0);integer(data['steps'],0,10000);integer(data['scalar_work_cells'],0,100000000)
        for key in ('time_s','input_energy_j','input_smoke_kg','initial_energy_j','initial_smoke_kg'):number(data[key],0)
        require(data['time_s']==data['steps']*r['momentum_dt_s'],'checkpoint integer-step clock')
        for key in ('energy_j','smoke_kg'):require(math.isclose(data['initial_'+key],math.fsum(r['initial_'+key]),rel_tol=1e-12,abs_tol=1e-12),'checkpoint initial authority')
    return r

def run(request,worker):
    validate(request);worker=Path(worker).resolve();worker_sha=hashlib.sha256(worker.read_bytes()).hexdigest();old=request['state']
    if old is not None:require(old['worker_sha256']==worker_sha,'checkpoint worker changed')
    native=dict(request,state=None if old is None else old['data'])
    with tempfile.TemporaryDirectory(prefix='open-atmosphere-') as temp:
        path=Path(temp)/'request.json';path.write_text(json.dumps(native,allow_nan=False,separators=(',',':')));require(path.stat().st_size<=64*1024*1024,'native request byte bound')
        executed=subprocess.run([str(worker),str(path)],capture_output=True,text=True,timeout=120);require(executed.returncode==0,executed.stderr.strip() or 'worker failed')
        path.write_text(executed.stdout);fields=strict_load(path)
    require(hashlib.sha256(worker.read_bytes()).hexdigest()==worker_sha,'worker changed during execution');require(fields['schema']=='physics_sim_open_atmosphere_fields/v1','native model')
    data=fields.pop('state')
    for key in ('steps','scalar_work_cells'):
        require(type(data[key]) in (int,float) and data[key]==int(data[key]),'counter');data[key]=int(data[key])
    state=sealed({'schema':STATE,'config_digest':digest(configuration(request)),'worker_sha256':worker_sha,'data':data});validate(dict(request,state=state,steps=[]))
    count=0 if old is None else old['data']['steps'];require(data['steps']==count+len(request['steps']) and fields['time_s']==data['time_s'],'accepted step identity')
    for key in ('energy_j','smoke_kg','face_velocity_m_s','pressure_pa','boundary_fluxes'):require(fields[key]==data[key],'field/checkpoint identity')
    grid=request['grid'];n=math.prod(grid);plane=grid[0]*grid[1];faces=2*plane;volume=math.prod(l/g for l,g in zip(request['length_m'],grid));props=request['properties'];capacity=props['density_kg_m3']*props['heat_capacity_j_kg_k']*volume
    old_flux=[0.]*(4*faces) if old is None else old['data']['boundary_fluxes'];flux=fields['boundary_fluxes']
    for a,b in zip(old_flux,flux):require(b>=a,'boundary receipts cannot decrease')
    budgets={};receipts=[]
    for quantity,key in enumerate(('energy_j','smoke_kg')):
        initial=math.fsum(request['initial_'+key]);added=math.fsum(math.fsum(s[key]) for s in request['steps']);previous_input=0 if old is None else old['data']['input_'+key]
        require(math.isclose(data['input_'+key],previous_input+added,rel_tol=1e-12,abs_tol=1e-12),'source input identity')
        incoming=math.fsum(flux[2*quantity*faces:(2*quantity+1)*faces]);outgoing=math.fsum(flux[(2*quantity+1)*faces:(2*quantity+2)*faces]);stored=math.fsum(fields[key]);balance=stored-initial-data['input_'+key]-incoming+outgoing
        require(abs(balance)<=1e-10*max(initial+data['input_'+key]+incoming+outgoing,1e-12),'independent lifetime flux conservation')
        before=initial if old is None else math.fsum(old['data'][key]);chunk_in=math.fsum(b-a for a,b in zip(old_flux[2*quantity*faces:(2*quantity+1)*faces],flux[2*quantity*faces:(2*quantity+1)*faces]));chunk_out=math.fsum(b-a for a,b in zip(old_flux[(2*quantity+1)*faces:(2*quantity+2)*faces],flux[(2*quantity+1)*faces:(2*quantity+2)*faces]));chunk_balance=stored-before-added-chunk_in+chunk_out
        require(abs(chunk_balance)<=1e-10*max(before+added+chunk_in+chunk_out,1e-12),'independent chunk flux conservation')
        budgets[key]={'initial':initial,'input':data['input_'+key],'inflow':incoming,'outflow':outgoing,'stored':stored,'loss':0,'balance':balance,'chunk_balance':chunk_balance}
    for boundary,name in enumerate(('z_min','z_max')):
        record={'boundary':name,'normal':[0,0,-1 if boundary==0 else 1],'sampling':'X-fast boundary faces','grid_xy':grid[:2]}
        for quantity,key in enumerate(('energy_j','smoke_kg')):
            for direction in range(2):
                block=(2*quantity+direction)*faces+boundary*plane;values=flux[block:block+plane]
                record[key+('_inflow' if direction==0 else '_outflow')]={'per_face':values,'total':math.fsum(values)}
        receipts.append(record)
    array(fields['temperature_k'],n,0);array(fields['smoke_concentration_kg_m3'],n,0)
    for q in range(n):
        temperature=props['reference_temperature_k']+fields['energy_j'][q]/capacity
        require(math.isclose(fields['temperature_k'][q],temperature,rel_tol=1e-13),'SI temperature');require(math.isclose(fields['smoke_concentration_kg_m3'][q],fields['smoke_kg'][q]/volume,rel_tol=1e-13,abs_tol=1e-30),'SI smoke concentration')
        if request['buoyancy']['enabled']:require((temperature-props['reference_temperature_k'])/props['reference_temperature_k']<=request['buoyancy']['max_temperature_contrast_fraction'],'accepted contrast applicability')
        divergence=0.
        for axis in range(3):
            stride=(1,grid[0],plane)[axis];coordinate=(q//stride)%grid[axis];upper=q+stride if axis==2 or coordinate+1<grid[axis] else q-(grid[axis]-1)*stride
            divergence+=(fields['face_velocity_m_s'][axis*n+upper]-fields['face_velocity_m_s'][axis*n+q])/(request['length_m'][axis]/grid[axis])
        require(math.isfinite(divergence) and abs(divergence)<1e-8,'independent open divergence')
    number(fields['max_divergence_s_inv'],0,1e-8);number(fields['projection_relative_residual'],0,1e-11)
    start=count*request['momentum_dt_s'];return sealed({'schema':'physics_sim_open_atmosphere_experiment/v1','model':MODEL,'status':'completed','configuration':configuration(request),'start_time_s':start,'steps_advanced':len(request['steps']),'fields':fields,'state':state,'budgets':budgets,'boundary_receipts':receipts,'boundaries':'periodic_xy_open_z_reservoirs','momentum_feedback':request['buoyancy']['enabled'],'native_checkpoint_restart':True,'ash_model':False})

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--request',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--worker',type=Path,default=ROOT/'build/open-atmosphere/physics_sim_open_atmosphere_worker');a=p.parse_args()
    require(not a.output.exists(),'fresh output path');result=run(strict_load(a.request),a.worker);a.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.NamedTemporaryFile(mode='w',dir=a.output.parent,prefix='.open-',delete=False) as file:
        temp=Path(file.name)
        try:
            json.dump(result,file,allow_nan=False);file.flush();os.fsync(file.fileno());os.link(temp,a.output)
            directory=os.open(a.output.parent,os.O_RDONLY)
            try:os.fsync(directory)
            finally:os.close(directory)
        finally:temp.unlink(missing_ok=True)
    print(json.dumps({'digest':result['digest'],'time_s':result['fields']['time_s'],'budgets':result['budgets']}))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as error:raise SystemExit(str(error))
