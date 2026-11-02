"""Explicit open-domain relaxation driver and separate mechanical-work receipts.

This module is prepared independently; native/host integration is gated by the
controlled timing campaign. It does not activate itself or alter inlet modes.
"""
import math
from surface_sources.growth_fire_v1 import keys,number,integer,require
from .kinetic_energy import kinetic
SCHEMA='physics_sim_open_weather_x/v1'
STATE='physics_sim_open_weather_state/v1'
FIELD='open_domain_forcing'
RECEIPT='open_domain_forcing_state'


def validate(config):
    if FIELD not in config:return
    require('wind_forcing' not in config and 'diagnostic_transport' not in config,
            'open driver excludes prescribed inlet and fixed momentum diagnostics')
    w=config[FIELD]
    keys(w,('schema','ramp_height_m','response_s','interpolation','momentum_backflow','knots'))
    require(w['schema']==SCHEMA and w['momentum_backflow']=='quiescent_exterior/v1',
            'explicit open driver and quiescent exterior momentum')
    b=config['boundary_policy']
    require(b['horizontal']=='ambient_open_xy' and b['vertical']=='solid_bottom_open_top'
            and b['pressure_datum_pa']==[0,0], 'open pressure reservoirs and solid ground')
    number(w['ramp_height_m'],config['length_m'][2]/config['grid'][2],config['length_m'][2])
    number(w['response_s'],.001,100)
    require(w['interpolation'] in ('hold','linear'),'driver interpolation')
    require(type(w['knots']) is list and 1<=len(w['knots'])<=16,'bounded driver knots')
    previous=-1
    for knot in w['knots']:
        keys(knot,('step','target_m_s','strength'))
        integer(knot['step'],0,10000);number(knot['target_m_s'],-10,10);number(knot['strength'],0,1)
        require(knot['step']>previous,'strict driver knot order');previous=knot['step']
    require(w['knots'][0]['step']==0,'driver starts at absolute native step zero')


def command(config,step):
    validate(config);integer(step,0,10000)
    w=config[FIELD];knots=w['knots'];i=0
    while i+1<len(knots) and step>=knots[i+1]['step']:i+=1
    values=[knots[i]['target_m_s'],knots[i]['strength']]
    if w['interpolation']=='linear' and i+1<len(knots):
        t=(step-knots[i]['step'])/(knots[i+1]['step']-knots[i]['step'])
        for j,key in enumerate(('target_m_s','strength')):
            values[j]+=(knots[i+1][key]-values[j])*t
    return tuple(values)


def state(config,data):
    if FIELD not in config:
        require(RECEIPT not in data,'unbound open driver receipt')
        return
    validate(config)
    r=data[RECEIPT];keys(r,('schema','external_work_j','air_in_m3','air_out_m3'))
    require(r['schema']==STATE,'versioned open driver state')
    number(r['external_work_j']);number(r['air_in_m3'],0);number(r['air_out_m3'],0)
    integer(data['steps'],0,10000)
    # Boundary flux is observed after projection; no inlet volume is imposed.
    elapsed=data['steps']*config['momentum_dt_s'];volume=math.prod(config['length_m'])
    require(abs(r['air_in_m3']-r['air_out_m3'])<=elapsed*volume*1e-8+
            1e-12*(r['air_in_m3']+r['air_out_m3']), 'open driver air-volume balance')
    ledger=data['kinetic_ledger_j'];stored=data['kinetic_stored_j']
    require(type(ledger) is list and len(ledger)==10,'ordinary ten-term ledger retained')
    for value in ledger:number(value)
    number(stored,0)
    scale=max(1,abs(stored),math.fsum(abs(v) for v in ledger),abs(r['external_work_j']))
    require(abs(stored-kinetic(config,data['face_velocity_m_s']))<1e-11*scale,
            'independent driver MAC kinetic energy')
    require(abs(stored-math.fsum([*ledger,r['external_work_j']]))<1e-10*scale,
            'separate external mechanical work closes kinetic state')
    require(ledger[5]>=0 and ledger[6]==0,'Euler work nonnegative; no inlet enforcement work')
    initial=kinetic(config,config['initial_face_velocity_m_s'])
    require(abs(ledger[0]-initial)<1e-12*max(1,initial),'driver initial kinetic authority')
    if data['steps']==0:
        require(r['external_work_j']==0 and r['air_in_m3']==r['air_out_m3']==0,
                'initial driver receipts are empty')


def observation(config,fields,data,old):
    if FIELD not in config:return None
    state(config,data)
    for key in (RECEIPT,'kinetic_ledger_j','kinetic_stored_j'):
        require(fields[key]==data[key],'driver field/checkpoint identity')
    if old is not None:
        for key in ('air_in_m3','air_out_m3'):
            require(data[RECEIPT][key]>=old['data'][RECEIPT][key],'driver volume receipt cannot decrease')
    target,strength=command(config,data['steps'])
    return {'schema':'physics_sim_open_weather_observation/v1',
            'boundary_model':'ambient_pressure_xy_top_solid_ground_quiescent_momentum_backflow',
            'sampling':'left endpoint of each absolute native momentum step',
            'next_target_m_s':target,'next_strength':strength,'state':dict(data[RECEIPT])}
