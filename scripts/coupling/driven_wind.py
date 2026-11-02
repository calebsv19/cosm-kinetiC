"""Closed X-inlet schedule, exact native-step sampling and independent SI ledger."""
import math
from surface_sources.growth_fire_v1 import keys,number,integer,require
from .ventilation import SCHEMAS,CHANNEL,BALANCED,LEGACY,validate_extension,face_state,volume
COUNTERS=('wind_air_in_m3','wind_air_out_m3','wind_inlet_volume_m3')

def validate(config):
    if 'wind_forcing' not in config:return
    w=config['wind_forcing'];keys(w,('schema','inlet','ramp_height_m','interpolation','knots',*(('momentum_backflow',) if 'momentum_backflow' in w else ()),*(('inlet_patch_m',) if w.get('schema')==CHANNEL else ())))
    require(w['schema'] in SCHEMAS and (w['inlet']=='x_pair' if w['schema']==BALANCED else w['inlet'] in ('x_min','x_max')),'X inlet policy')
    if 'momentum_backflow' in w:require(w['momentum_backflow']=='quiescent_exterior/v1','momentum backflow policy')
    b=config['boundary_policy']
    require(b['horizontal']=='ambient_open_xy' and b['vertical']=='solid_bottom_open_top','inlet requires open lateral ground model')
    require(w['interpolation'] in ('hold','linear'),'wind interpolation')
    number(w['ramp_height_m'],config['length_m'][2]/config['grid'][2],config['length_m'][2])
    require(type(w['knots']) is list and 1<=len(w['knots'])<=16,'bounded wind knots')
    previous=-1
    for knot in w['knots']:
        keys(knot,('step','speed_m_s'));integer(knot['step'],0,10000);number(knot['speed_m_s'],-10 if w['schema']==BALANCED else 0,10)
        require(knot['step']>previous,'strict wind knot order');previous=knot['step']
    require(w['knots'][0]['step']==0,'wind schedule starts at native step zero')
    validate_extension(config)

def command(config,step):
    w=config['wind_forcing'];knots=w['knots'];i=0
    while i+1<len(knots) and step>=knots[i+1]['step']:i+=1
    value=knots[i]['speed_m_s']
    if w['interpolation']=='linear' and i+1<len(knots):
        value+=(knots[i+1]['speed_m_s']-value)*(step-knots[i]['step'])/(knots[i+1]['step']-knots[i]['step'])
    return -value if w['inlet']=='x_max' else value

def integrated_inlet(config,steps):
    """Sum left-endpoint commands analytically; never use rounded wall time."""
    w=config['wind_forcing']
    if w['schema']!=LEGACY:return volume(config,steps,command)
    knots=w['knots'];total=0.
    for i,k in enumerate(knots):
        count=max(0,min(steps,knots[i+1]['step'] if i+1<len(knots) else steps)-k['step'])
        total+=count*k['speed_m_s']
        if w['interpolation']=='linear' and i+1<len(knots):
            total+=(knots[i+1]['speed_m_s']-k['speed_m_s'])*count*(count-1)/2/(knots[i+1]['step']-k['step'])
    dz=config['length_m'][2]/config['grid'][2]
    area=config['length_m'][1]*dz*math.fsum(min(1,(k+.5)*dz/w['ramp_height_m']) for k in range(config['grid'][2]))
    return total*area*config['momentum_dt_s']

def state(config,data):
    if 'wind_forcing' not in config:return
    for key in COUNTERS:number(data[key],0)
    incoming,outgoing,inlet=(data[k] for k in COUNTERS)
    require(math.isclose(inlet,integrated_inlet(config,data['steps']),rel_tol=1e-10,abs_tol=1e-14),'integrated prescribed inlet volume')
    require(abs(incoming-outgoing)<=1e-9*max(incoming+outgoing,1e-8) and incoming+1e-14>=inlet,'independent air volume balance')
    if config['wind_forcing']['schema']!=LEGACY:
        if data['steps']==0:
            if 'diagnostic_transport' in config:
                from .frozen_transport import validate as fixed_validate
                fixed_validate(config,data)
            else:
                require(all(v==0 for v in data['face_velocity_m_s']),'extended wind initial state is still')
        face_state(config,data,command);return
    if data['steps']==0:return
    nx,ny,nz=config['grid'];n=nx*ny*nz;w=config['wind_forcing'];dz=config['length_m'][2]/nz
    speed=command(config,data['steps']-1)
    for k in range(nz):
        expected=speed*min(1,(k+.5)*dz/w['ramp_height_m'])
        for j in range(ny):
            q=3*n+nx*ny+k*ny+j if w['inlet']=='x_max' else (k*ny+j)*nx
            require(math.isclose(data['face_velocity_m_s'][q],expected,rel_tol=1e-13,abs_tol=1e-14),'native prescribed inlet face')

def observation(config,fields,data,old):
    if 'wind_forcing' not in config:return None
    for key in COUNTERS:
        require(fields[key]==data[key],'wind field/checkpoint identity')
        require(old is None or data[key]>=old['data'][key],'wind receipt cannot decrease')
    result={'schema':'physics_sim_driven_wind_observation/v1','inlet':config['wind_forcing']['inlet'],
            'sampling':'left endpoint of each absolute native momentum step',
            'last_target_velocity_x_m_s':None if data['steps']==0 else command(config,data['steps']-1),
            'next_target_velocity_x_m_s':command(config,data['steps']),
            'air_in_m3':data[COUNTERS[0]],'air_out_m3':data[COUNTERS[1]],'inlet_volume_m3':data[COUNTERS[2]]}

    if config['wind_forcing']['schema']!=LEGACY:
        from .ventilation import description
        result.update(schema='physics_sim_ventilation_observation/v1',forcing_schema=config['wind_forcing']['schema'],boundary_model=description(config))
        if config['wind_forcing']['schema']==CHANNEL:result['inlet_patch_m']=config['wind_forcing']['inlet_patch_m']
    return result
