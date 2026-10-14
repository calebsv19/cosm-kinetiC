"""Bounded sensitivity/stress scenarios; physical geometry and sampling stay explicit."""
import copy
from fractions import Fraction
import math
from surface_sources.growth_fire_v1 import require,integer,number


def case(base,*,height=.0625,exchange=Fraction(1,25),duration=Fraction(2,5),wind_scale=1,domain_height=.5):
    c=copy.deepcopy(base);exchange=Fraction(exchange);duration=Fraction(duration)
    count=duration/exchange
    require(count.denominator==1,'duration must align with exchange')
    integer(count.numerator,3,32)
    for period in c['periods'][:2]:
        require((exchange/Fraction(*period)).denominator==1,'native cadence must align')
    number(height,0,domain_height);require(0<height<domain_height,'interior physical sampling height')
    number(wind_scale,0,40)
    c['periods'][2]=[exchange.numerator,exchange.denominator];c['windows']=count.numerator
    c['return_channel']['sample_height_m']=height
    f=c['fluid'];spacing=c['receiver']['spacing_m'][2]
    layers=domain_height/spacing;require(layers==int(layers),'domain height must preserve cell spacing')
    f['grid'][2]=int(layers);f['length_m'][2]=domain_height;c['receiver']['dimensions'][2]=int(layers)
    plane=f['grid'][0]*f['grid'][1];n=math.prod(f['grid'])
    # Identical physical shear gradient; changing domain height does not rescale it.
    f['initial_face_velocity_m_s']=[speed*wind_scale*((k+.5)*spacing) for speed in (.4,.1) for k in range(f['grid'][2]) for _ in range(plane)]+[0.]*(n+plane)
    f['initial_energy_j']=[0.]*n;f['initial_smoke_kg']=[0.]*n
    return c


def matrix(base):
    specs=[('reference',{}),('height-half',{'height':.03125}),('height-double',{'height':.125}),
        ('exchange-half',{'exchange':Fraction(1,50)}),('exchange-slow',{'exchange':Fraction(1,10)}),
        ('strong-wind',{'wind_scale':20}),
        ('long',{'duration':Fraction(1)}),
        ('long-strong',{'duration':Fraction(2),'exchange':Fraction(1,10),'wind_scale':20}),
        ('taller-long-strong',{'duration':Fraction(2),'exchange':Fraction(1,10),'wind_scale':20,'domain_height':1.})]
    return [(label,case(base,**options)) for label,options in specs]


def comparison(reference,candidate):
    a=reference['metrics'];b=candidate['metrics']
    return {'energy_relative_change':b['energy_j']/a['energy_j']-1,
        'smoke_relative_change':b['smoke_kg']/a['smoke_kg']-1,
        'temperature_peak_change_k':b['temperature_peak_k']-a['temperature_peak_k']}
