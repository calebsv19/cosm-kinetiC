"""Closed 0.5s normal-worker qualification; no general128 or receiver admission."""
import copy
from fractions import Fraction
from surface_sources.growth_fire_v1 import require
from .scalar_capacity import validate as validate80
from .source_replay import Replay
PROFILE='scalar-gust128/v1'
MAX_STEPS=100
PACKET_STEPS=4
NUMERICAL_BYTES=2*1024**3

def selected(r):return r.get('capacity_profile')==PROFILE

def validate(r):
    require(selected(r),'closed128 profile')
    # Reuse the existing matched scalar physical bounds; change capacity identity only.
    validate80(dict(r,capacity_profile='scalar-wake80/v1',grid=[80]*3))
    require(r['grid']==[128]*3 and len(r.get('steps',[]))<=PACKET_STEPS,'closed128 grid/packet')
    state=r.get('state');data={} if state is None else state.get('data',state)
    first=data.get('steps',0)
    require(first+len(r.get('steps',[]))<=MAX_STEPS,'closed128 horizon')
    limits=r.get('resource_limits',{})
    require(limits.get('max_cells')==128**3 and limits.get('numerical_bytes')==NUMERICAL_BYTES and limits.get('cache_pressure_operator') is True,'explicit128 allocation bundle')
    require(limits.get('native_request_bytes')==256*1024**2 and limits.get('native_output_bytes')==256*1024**2,'explicit128 wire bounds')
    require(r.get('properties')==dict(density_kg_m3=1.2,dynamic_viscosity_pa_s=1.8e-5,heat_capacity_j_kg_k=1005,reference_temperature_k=300,conductivity_w_m_k=.026,tracer_diffusivity_m2_s=0.),'matched128 materials')
    require(r.get('buoyancy')==dict(enabled=True,gravity_m_s2=9.81,expansion_per_k=1/300,max_temperature_contrast_fraction=.1),'matched128 buoyancy')
    require(r.get('inert_object')==dict(schema='physics_sim_inert_object/v1',axis=0,side=-1,initial_temperature_k=300.,capacity_j_k=10.,h_w_m2_k=2,support_depth_m=.125),'matched128 object')
    require(r.get('open_domain_forcing')==dict(schema='physics_sim_open_weather_x/v1',ramp_height_m=.25,response_s=.2,interpolation='hold',momentum_backflow='quiescent_exterior/v1',knots=[dict(step=0,target_m_s=.5,strength=0),dict(step=80,target_m_s=.5,strength=1),dict(step=240,target_m_s=.5,strength=0)]),'matched128 gust clock')

class Source:
    """Reuse admitted lower subvolume at exact spacing, then zero-extend upwards."""
    def __init__(self,pulse,receiver,*,deposition_interval=None):
        require(receiver['dimensions']==[128]*3 and receiver['spacing_m']==[1/128]*3 and receiver['origin_m']==[0]*3 and receiver['layer']==0 and receiver['injection_depth_m']==.125,'closed128 source geometry')
        self.target=copy.deepcopy(receiver);lower=copy.deepcopy(receiver);lower['dimensions']=[128,128,32]
        self.replay=Replay(pulse,lower,deposition_interval=deposition_interval)
    def steps(self,first,count,dt):
        require(first+count<=MAX_STEPS and Fraction(dt)==Fraction(1,200),'closed128 replay clock')
        steps=self.replay.steps(first,count,dt)
        for step in steps:
            for key in ('energy_j','smoke_kg'):step[key].extend([0.]*(128**3-len(step[key])))
        return steps
