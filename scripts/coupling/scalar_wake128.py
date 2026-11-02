"""Closed fixed-input3.2s capacity; no timestep/default/physical-law expansion."""
from fractions import Fraction
from surface_sources.growth_fire_v1 import require
from . import scalar_gust128 as short
PROFILE='scalar-wake128/v1'
MAX_STEPS=640
PACKET_STEPS=2
PULSE='d27b5fe13fa257b01418f6ef3822d25fa4e03c4787ad02bd3cea996bee22a1c0'

def selected(r):return r.get('capacity_profile')==PROFILE

def validate(r):
    require(selected(r),'closed128 wake profile')
    # Reuse all fixed physical/resource bounds of the short reference. Its
    # original100step horizon is deliberately not broadened.
    short.validate(dict(r,capacity_profile=short.PROFILE,state=None,steps=[]))
    steps=r.get('steps',[]);data=r.get('state')
    data={} if data is None else data.get('data',data)
    first=data.get('steps',0)
    require(type(first) in (int,float) and first>=0 and int(first)==first
            and type(steps) is list and len(steps)<=PACKET_STEPS
            and first+len(steps)<=MAX_STEPS,'closed128 wake horizon/packet')

class Source(short.Source):
    def __init__(self,pulse,receiver,*,deposition_interval=None):
        require(pulse.get('digest')==PULSE and deposition_interval is None,'fixed128 wake source clock/pulse')
        super().__init__(pulse,receiver)

    def steps(self,first,count,dt):
        require(type(first) is int and first>=0 and type(count) is int and 1<=count<=PACKET_STEPS
                and first+count<=MAX_STEPS and Fraction(dt)==Fraction(1,200),'closed128 wake replay clock')
        steps=self.replay.steps(first,count,dt)
        for step in steps:
            for key in ('energy_j','smoke_kg'):step[key].extend([0.]*(128**3-len(step[key])))
        return steps
