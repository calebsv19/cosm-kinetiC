"""Long delivery isolation and recovery; fixed source/object physics across cases."""
import copy
from fractions import Fraction
from .inert_cases import recipe
from .wind_sensitivity import refined
from .solid_box import topology
from surface_sources.growth_fire_v1 import require
CASES=('continuous-16','continuous-32','continuous-32-half','wind-off-32')


def configuration(workers,name):
    require(name in CASES,'recovery case')
    c=refined(recipe(workers,live=True),16 if name.endswith('-16') else 32)
    c['periods'][2]=[2,5];c['windows']=32;c['source_delivery']={'schema':'fire_fluid_delivery_cutoff/v1','active_windows':4}
    c['source']['run_id']='inert-recovery-reference'
    f=c['fluid'];dt=Fraction(1,400) if name.endswith('-half') else Fraction(1,200)
    c['periods'][1]=[dt.numerator,dt.denominator];f['momentum_dt_s']=float(dt)
    f['wind_forcing']['knots']=[{'step':0,'speed_m_s':.5}]
    if name=='wind-off-32':f['wind_forcing']['knots'].append({'step':480,'speed_m_s':0})
    c['native_packet_steps']=80
    # Low box intersects some otherwise-unused injection-layer cells. Declare
    # their actual topology even though the emitter is strictly upstream.
    mask=topology(f);r=c['receiver'];n=f['grid'][0];layers=round(r['injection_depth_m']*n)
    r['fluid_mask']=[1-q for q in mask[:n*n]]
    r['injection_fluid_mask']=[1-q for q in mask[:n*n*layers]]
    return c
