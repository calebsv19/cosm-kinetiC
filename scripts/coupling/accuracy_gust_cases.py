"""Three fixed-input nested-grid gust controls; native events follow physical time."""
from fractions import Fraction
from .open_weather_cases import configuration as weather
from .wind_sensitivity import refined
from .solid_box import topology
from .fluid_adapter import validate
from surface_sources.growth_fire_v1 import require
CASES=('gust32','gust64','gust64-half')

def configuration(workers,name):
    require(name in CASES,'named fixed-input gust accuracy case')
    c=weather(workers,'gust',32);n=32 if name=='gust32' else 64
    c=refined(c,n);f=c['fluid'];dt=Fraction(1,400) if name.endswith('-half') else Fraction(1,200)
    old=Fraction(*c['periods'][1]);c['periods'][1]=[dt.numerator,dt.denominator]
    c['periods'][2]=[1,5];c['windows']=16;c['source_delivery']['active_windows']=8
    f['momentum_dt_s']=float(dt)
    for k in f['open_domain_forcing']['knots']:
        index=k['step']*old/dt;require(index.denominator==1,'native-aligned physical gust event');k['step']=int(index)
    f['resource_limits']['max_cells']=n**3
    f['resource_limits'].update(native_request_bytes=256*1024**2,native_output_bytes=256*1024**2)
    mask=topology(f);r=c['receiver'];r['fluid_mask']=[1-v for v in mask[:n*n]]
    r['injection_fluid_mask']=[1-v for v in mask[:n*n*(n//8)]]
    c['journal_codec']='lossless_f64_zlib_1g/v1';c['native_packet_steps']=20
    validate(f)
    return c
