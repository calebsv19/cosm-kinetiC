"""Bounded recovery controls: name every physical intervention explicitly."""
from fractions import Fraction
from .recovery_cases import configuration as recovery
from .wind_sensitivity import refined,cadence
from .solid_box import topology
from .ventilation import BALANCED
from surface_sources.growth_fire_v1 import require
SMALL=('gust16','windoff16','no-buoyancy16','windoff-no-buoyancy16',
       'balanced16','reverse16','offset-object16','aperture16')
EXPENSIVE=('exchange32','spatial64','balanced32','reverse32')
CASES=SMALL+EXPENSIVE


def configuration(workers,name):
    require(name in CASES,'overnight case')
    n=64 if name=='spatial64' else 32 if name in EXPENSIVE else 16
    c=recovery(workers,'continuous-16' if n==16 else 'continuous-32')
    if n==64:c=refined(c,n)
    seconds=Fraction(1,5) if name in ('exchange32','spatial64') else Fraction(2,5)
    c=cadence(c,seconds,Fraction(32,5))
    c['source_delivery']['active_windows']=int(Fraction(8,5)/seconds)
    f=c['fluid'];w=f['wind_forcing'];step=lambda t:int(Fraction(t)/Fraction(1,200))
    if name in ('gust16','windoff16','windoff-no-buoyancy16'):
        when=Fraction(8,5) if name=='gust16' else Fraction(12,5)
        w['knots'].append({'step':step(when),'speed_m_s':0.})
    if 'no-buoyancy' in name:f['buoyancy']['enabled']=False
    if name.startswith(('balanced','reverse')):
        w['schema']=BALANCED;w['inlet']='x_pair';w.pop('inlet_patch_m')
        if name.startswith('reverse'):w['knots'].append({'step':480,'speed_m_s':-.5})
    if name=='offset-object16':
        for key in ('lower_m','upper_m'):f['solid_box'][key][1]+=.125
    if name=='aperture16':w['inlet_patch_m']={'lower_yz_m':[.25,0.],'upper_yz_m':[.75,.5]}
    f['resource_limits']['max_cells']=max(32768,n**3)
    f['resource_limits']['native_request_bytes']=256*1024*1024
    f['resource_limits']['native_output_bytes']=256*1024*1024
    c['native_packet_steps']=20 if n==64 else 80
    if n==64:c['journal_codec']='lossless_f64_zlib_1g/v1'
    mask=topology(f);r=c['receiver'];layers=round(r['injection_depth_m']*n)
    r['fluid_mask']=[1-v for v in mask[:n*n]]
    r['injection_fluid_mask']=[1-v for v in mask[:n*n*layers]]
    return c
