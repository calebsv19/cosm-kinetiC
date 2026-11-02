"""Named open-pressure driving interventions with fixed Fire/object physics."""
from fractions import Fraction
from surface_sources.growth_fire_v1 import require
from .overnight_cases import configuration as baseline
from .wind_sensitivity import cadence
from .open_weather import validate,SCHEMA
PROFILES=('none','continuous','gust','fade','reversal')


def configuration(workers,profile,grid=16):
    require(profile in PROFILES and type(grid) is int and grid in (16,32),
            'bounded named open-weather recipe')
    c=cadence(baseline(workers,'gust16' if grid==16 else 'exchange32'),
              Fraction(1,10),Fraction(16,5))
    c['source_delivery']['active_windows']=16;c['native_packet_steps']=20
    f=c['fluid'];f.pop('wind_forcing')
    w={'schema':SCHEMA,'ramp_height_m':.25,'response_s':.2,'interpolation':'hold',
       'momentum_backflow':'quiescent_exterior/v1'}
    def knot(time,target=.5,strength=1):
        step=Fraction(str(time))/Fraction(*c['periods'][1])
        require(step.denominator==1,'weather event aligned to native clock')
        return {'step':int(step),'target_m_s':target,'strength':strength}
    if profile=='none':w['knots']=[knot(0,strength=0)]
    elif profile=='continuous':w['knots']=[knot(0)]
    elif profile=='gust':w['knots']=[knot(0,strength=0),knot(.4),knot(1.2,strength=0)]
    elif profile=='fade':
        w['interpolation']='linear'
        w['knots']=[knot(0,strength=0),knot(.4,strength=0),knot(.6),knot(1.2),knot(1.6,strength=0)]
    else:w['knots']=[knot(0),knot(1.2,target=-.5),knot(2.4,target=-.5,strength=0)]
    f['open_domain_forcing']=w;validate(f)
    return c
