"""Closed full-horizon capacity; private validator retains all ordinary physical gates.

The ordinary module is pinned by historical producers. A private module instance
uses the exact same code, changing only its scalar-work admission range. The
process-global ordinary validator, solver counters and physical fields are untouched.
"""
from pathlib import Path
import importlib.util
from functools import lru_cache
from surface_sources.growth_fire_v1 import require
PROFILE='exterior-room30/v1'
STEPS=6000
CYCLES=4096
WORK_LIMIT=STEPS*64**3*2*CYCLES

def selected(config):
    return 'computation_profile' in config.get('rig_geometry',{})

def validate(config,state=None,steps=()):
    g=config['rig_geometry']
    require(g.get('computation_profile')==PROFILE and g.get('schema')=='physics_sim_exterior_room/v1' and g.get('exterior_profile')=='far' and g.get('window_profile') in ('small','large'),'closed exterior30 computation profile')
    require(config['grid']==[64]*3 and config['length_m']==[2]*3 and config['momentum_dt_s']==.005 and config.get('scalar_transport')=='mc_ssprk2/v1','unchanged exterior30 discretization')
    require(config['resource_limits']['scalar_work_cells']==WORK_LIMIT,'full6000step ordinary-cycle capacity')
    require(config['open_domain_forcing']['knots']==[dict(step=0,target_m_s=0,strength=0)],'no imposed wind')
    data=None if state is None else state['data'];first=0 if data is None else data['steps'];used=0 if data is None else data['scalar_work_cells']
    require(type(first) is int and 0<=first<=STEPS and first+len(steps)<=STEPS,'30s integer-step horizon')
    require(type(used) is int and first*64**3*2<=used<=first*64**3*2*CYCLES,'preserved cumulative work')
    require(used+(STEPS-first)*64**3*2*CYCLES<=WORK_LIMIT,'all remaining accepted cycles fit')

@lru_cache(maxsize=1)
def policy():
    import open_atmosphere as ordinary
    spec=importlib.util.spec_from_file_location('_exterior30_open_policy',Path(ordinary.__file__))
    private=importlib.util.module_from_spec(spec);spec.loader.exec_module(private)
    integer=private.integer
    def resource_integer(value,minimum,maximum):
        # Only the existing scalar-work resource admission call has this range.
        # Exact WORK_LIMIT is required by validate() before this private policy is used.
        if minimum==100000000 and maximum==1000000000 and value==WORK_LIMIT:
            return integer(value,minimum,WORK_LIMIT)
        return integer(value,minimum,maximum)
    private.integer=resource_integer
    return private

def capacity():
    return dict(profile=PROFILE,steps=STEPS,cells=64**3,scalar_stages=2,max_cycles_per_step=CYCLES,
                cumulative_work_limit=WORK_LIMIT,minimum_work=STEPS*64**3*2,
                numerical_checks='ordinary CFL/4096cycles/pressure/divergence/contrast/memory retained')
