"""Optional native Fluid substate: compensated object temperature and signed heat.

It shares the gas clock and joint publication. No separate core_sim participant,
wall law, radiation, ignition, or new Fire temperature interpretation is added.
"""
import math
from surface_sources.growth_fire_v1 import keys,number,integer,require
CONFIG='physics_sim_inert_object/v1'
STATE='physics_sim_inert_object_state/v1'

def validate(config,data=None):
    o=config.get('inert_object')
    if o is None:
        if data is not None:require('inert_object' not in data,'object state without configuration')
        return
    keys(o,('schema','axis','side','initial_temperature_k','capacity_j_k','h_w_m2_k','support_depth_m'))
    require(o['schema']==CONFIG,'inert object schema');integer(o['axis'],0,2)
    require(type(o['side']) is int and o['side'] in (-1,1),'object face side')
    ref=config['properties']['reference_temperature_k']
    number(o['initial_temperature_k'],ref,1e9);number(o['capacity_j_k'],1e-12,1e12)
    number(o['h_w_m2_k'],0,1e9);number(o['support_depth_m'],1e-12,1000)
    require('solid_box' in config,'object needs a stationary box')
    box=config['solid_box'];axis=o['axis'];spacing=config['length_m'][axis]/config['grid'][axis]
    layers=o['support_depth_m']/spacing
    require(layers>=1 and abs(layers-round(layers))<=1e-10,'object support must align')
    available=box['lower_m'][axis] if o['side']<0 else config['length_m'][axis]-box['upper_m'][axis]
    require(o['support_depth_m']<=available+1e-12,'object support leaves gas domain')
    if data is None:return
    s=data.get('inert_object');keys(s,('schema','temperature_k','temperature_correction_k','gas_to_object_j','steps','time_s'))
    require(s['schema']==STATE,'object state schema')
    number(s['temperature_k'],ref,1e9);number(s['temperature_correction_k'],-math.ulp(s['temperature_k']),math.ulp(s['temperature_k']))
    number(s['gas_to_object_j'],-1e20,1e20);integer(s['steps'],0,10000);number(s['time_s'],0)
    require(s['steps']==data['steps'] and s['time_s']==data['time_s'],'object/gas clock mismatch')
    require((s['temperature_k']-ref)+s['temperature_correction_k']>=0,'object below reference')
    delta=o['capacity_j_k']*((s['temperature_k']-o['initial_temperature_k'])+s['temperature_correction_k'])
    require(math.isfinite(delta) and abs(delta-s['gas_to_object_j'])<=1e-9,'object energy/receipt mismatch')

def transfer(config,data):
    return data['inert_object']['gas_to_object_j'] if 'inert_object' in config else 0.

def debit(config,data,key):
    return transfer(config,data) if key=='energy_j' else 0.

def fields(config,fields,data):
    if 'inert_object' in config:
        require(fields.get('inert_object')==data['inert_object'],'object field/checkpoint identity')
    else:require('inert_object' not in fields,'unexpected object fields')
