"""Reusable source-owned recipes for bounded native inlet experiments."""
from fractions import Fraction
from .driven_wind_qualification import configuration as driven
from .wind_sensitivity import cadence
from .ventilation import CHANNEL,BALANCED
from surface_sources.growth_fire_v1 import require,number
PROFILES=('continuous','abrupt_gust','ramped_gust','channel_full','channel_patch','channel_patch_box',
          'single_gust_recovery','repeated_gust','balanced_forward','balanced_reversal')


def configuration(workers,profile,*,feedback=True,speed=None,duration=None,exchange=None,
                  gust_start=.4,gust_stop=1.2,ramp=.4):
    require(profile in PROFILES,'unknown native wind preset')
    require(type(feedback) is bool,'explicit feedback setting')
    number(gust_start,0,40);number(gust_stop,0,40);number(ramp,0,10)
    require(gust_start<gust_stop,'positive gust duration')
    extended=profile in ('single_gust_recovery','repeated_gust','balanced_forward','balanced_reversal')
    duration=Fraction(str(duration if duration is not None else (4.8 if extended else 3.2)))
    exchange=Fraction(str(exchange if exchange is not None else (.2 if extended else .1)))
    speed=(1.75 if profile in ('channel_patch','channel_patch_box') else .5) if speed is None else speed
    number(speed,0,2)
    base='ramped-gust' if profile=='ramped_gust' else 'abrupt-gust' if profile in ('abrupt_gust','single_gust_recovery','repeated_gust') else 'continuous-positive'
    c=driven(workers,base,feedback,box=profile=='channel_patch_box')
    c=cadence(c,exchange,duration);w=c['fluid']['wind_forcing'];dt=Fraction(str(c['fluid']['momentum_dt_s']))
    def knot(seconds,value):
        step=Fraction(str(seconds))/dt;require(step.denominator==1,'wind event/native step alignment')
        require(0<=step<=10000,'bounded native wind event');return {'step':int(step),'speed_m_s':value}
    if profile in ('abrupt_gust','single_gust_recovery','repeated_gust'):
        w['knots']=[knot(0,0),knot(gust_start,speed),knot(gust_stop,0)]
        if profile=='repeated_gust':
            second_start=Fraction(str(gust_stop))+Fraction(4,5);second_stop=second_start+Fraction(str(gust_stop))-Fraction(str(gust_start))
            w['knots'] += [knot(second_start,speed),knot(second_stop,0)]
    elif profile=='ramped_gust':
        require(ramp>0 and Fraction(str(gust_start))+Fraction(str(ramp))<Fraction(str(gust_stop)),'positive ramp and hold')
        w['knots']=[knot(0,0),knot(gust_start,0),knot(Fraction(str(gust_start))+Fraction(str(ramp)),speed),knot(gust_stop,speed),knot(Fraction(str(gust_stop))+Fraction(str(ramp)),0)]
    else:w['knots']=[knot(0,speed)]
    if profile.startswith('channel_'):
        w['schema']=CHANNEL
        patch={'lower_yz_m':[0,0],'upper_yz_m':[1,1]} if profile=='channel_full' else {'lower_yz_m':[.25,.25],'upper_yz_m':[.75,.75]}
        w['inlet_patch_m']=patch
    if profile.startswith('balanced_'):
        w.update(schema=BALANCED,inlet='x_pair',interpolation='hold')
        w['knots']=[knot(0,speed)]
        if profile=='balanced_reversal':w['knots'].append(knot(1.2,-speed))
        w['knots'].append(knot(2.4,0))
    # Recipe timing at t=0 denotes an immediate command, not duplicate knots.
    compact=[]
    for entry in w['knots']:
        if compact and compact[-1]['step']==entry['step']:compact[-1]=entry
        else:compact.append(entry)
    w['knots']=compact
    return c
