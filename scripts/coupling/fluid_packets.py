"""Bound each native capture without changing the exact coupling window.

Each packet uses ordinary restore, native steps and independent acceptance.
The final result is rechecked against the full window's original state/sources.
"""
import copy
from numeric_digest_stream import sealed
from surface_sources.growth_fire_v1 import integer,require
from . import fluid_adapter

def run(config,state,steps,worker,packet_steps,*,io_format=None):
    integer(packet_steps,1,256)
    selected=fluid_adapter.solver(config)
    require(hasattr(selected,'accept_fields'),'packetized flow requires open native model')
    if len(steps)<=packet_steps:return fluid_adapter.run(config,state,steps,worker,**({'io_format':io_format} if io_format else {}))
    original=state;packets=[]
    for offset in range(0,len(steps),packet_steps):
        result=fluid_adapter.run(config,state,steps[offset:offset+packet_steps],worker,**({'io_format':io_format} if io_format else {}))
        state=result['state'];packets.append({'begin_step':state['data']['steps']-result['steps_advanced'],'end_step':state['data']['steps'],'state_digest':state['digest'],'result_digest':result['digest']})
    request=dict(config,schema=selected.SCHEMA,state=original,steps=steps)
    fields=copy.deepcopy(result['fields']);fields['state']=state['data']
    fields['start_time_s']=0 if original is None else original['data']['time_s']
    combined=selected.accept_fields(request,fields,state['worker_sha256'])
    require(combined['state']==state,'packetized native final checkpoint identity')
    combined.pop('digest');combined['native_packets']=packets
    combined['packetization']='ordinary_native_restart_with_full_window_revalidation/v1'
    return sealed(combined)
