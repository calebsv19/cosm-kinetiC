"""Bounded heat/smoke isolation, separate from native Fire extinction.

Frames retain producer authority. The host records delivered versus diverted
nominal output; closed-valve energy is outside this gas/object subsystem.
"""
import math
from surface_sources.growth_fire_v1 import keys,integer,require
from numeric_digest_stream import sealed
from surface_sources.receiver import mapping
from .allocation import allocate_window
QUANTITIES=('energy_transferred_j','smoke_transferred_kg')


def validate(config):
    if 'source_delivery' not in config:return
    p=config['source_delivery'];keys(p,('schema','active_windows'))
    require(p['schema']=='fire_fluid_delivery_cutoff/v1','source delivery schema')
    integer(p['active_windows'],1,config['windows']-1)


def receipt(config,index,frame):
    if 'source_delivery' not in config:return None
    validate(config);integer(index,0,config['windows']-1)
    active=index<config['source_delivery']['active_windows']
    available={k:frame['totals'][k] for k in QUANTITIES}
    require(all(math.isfinite(v) and v>=0 for v in available.values()),'finite available source')
    delivered={k:v if active else 0. for k,v in available.items()}
    diverted={k:0. if active else v for k,v in available.items()}
    return sealed({'schema':'fire_fluid_delivery_receipt/v1','window':index,
        'source_frame_digest':frame['digest'],'active':active,
        'available':available,'delivered':delivered,'diverted':diverted,
        'disposition':'delivered_to_fluid' if active else 'diverted_outside_fluid'})


def expected(config,index,frame):
    r=receipt(config,index,frame)
    return {k:frame['totals'][k] for k in QUANTITIES} if r is None else r['delivered']


def allocate(config,index,frame,policy,window):
    r=receipt(config,index,frame)
    if r is None or r['active']:return allocate_window(frame,policy,window)
    # Geometry is still admitted when closed; no source bytes are relabeled.
    mapping(frame,policy)
    n=math.prod(policy['receiver']['dimensions'])
    zero={'energy_j':[0.]*n,'smoke_kg':[0.]*n}
    return [zero]*len(window['steps'])


def persisted(config,index,frame,evidence):
    r=receipt(config,index,frame)
    if r is None:
        require('source_delivery' not in evidence,'unexpected delivery receipt')
    else:
        require(evidence.get('source_delivery')==r,'persisted source delivery partition')
        require(evidence['transferred']==r['delivered'],'persisted actual delivery totals')
