"""Select native boundary model and verify its persisted physical budget."""
import math
import evolving_atmosphere as periodic
import open_atmosphere as open_domain
from surface_sources.growth_fire_v1 import require


def solver(config):
    return open_domain if 'boundary_policy' in config else periodic


def validate(config,state=None):
    selected=solver(config)
    selected.validate(dict(config,schema=selected.SCHEMA,state=state,steps=[]))


def run(config,state,steps,worker,*,io_format=None):
    selected=solver(config)
    require(io_format is None or selected is open_domain,'field packet IO requires open native model')
    return selected.run(dict(config,schema=selected.SCHEMA,state=state,steps=steps),worker,**({'io_format':io_format} if io_format else {}))


def conservation(config,result):
    data=result['state']['data'];fields=result['fields']
    for key in ('energy_j','smoke_kg'):
        require(fields[key]==data[key],'fluid checkpoint/field contents')
        total=math.fsum(data[key])
        if 'boundary_policy' in config:
            # Stored = initial + sources + reservoir inflow - reservoir outflow.
            from .open_boundaries import layout
            faces=layout(config)[1] if config['boundary_policy'].get('horizontal') else 2*config['grid'][0]*config['grid'][1]
            offset=0 if key=='energy_j' else 2*faces
            inflow=math.fsum(data['boundary_fluxes'][offset:offset+faces])
            outflow=math.fsum(data['boundary_fluxes'][offset+faces:offset+2*faces])
            from .inert_object import debit
            expected=data['initial_'+key]+data['input_'+key]+inflow-outflow-debit(config,data,key)
        else:expected=data['input_'+key]
        require(math.isclose(total,expected,rel_tol=1e-10,abs_tol=1e-20),'persisted whole-domain conservation')
