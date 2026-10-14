#!/usr/bin/env python3
"""Matched closed/open Fire boundary qualification under actual held Fluid wind."""
import argparse,base64,copy,json,math,os
from fractions import Fraction
from pathlib import Path
from fire_fluid_feedback import configuration
from fire_fluid_regression import qualify
from coupling.qualification import case,comparison
from coupling.framework import load_fire
from surface_sources.growth_fire_v1 import require

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output.resolve()/'native-evidence')
    cases={};budgets={}
    for label,profile,half in [('closed-strong','physical_rates_heat_advection_v1',False),
                             ('open-strong','physical_rates_heat_outflow_v1',False),
                             ('open-time-half','physical_rates_heat_outflow_v1',True)]:
        c=configuration(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker,
                        fire_denominator=100 if half else 50,fluid_denominator=400 if half else 200)
        c=case(c,exchange=Fraction(1,10),duration=Fraction(2),wind_scale=20)
        c['source']['evolution']['model']=profile;cases[label]=qualify(c,a.output/label)
        cp=json.loads((a.output/label/'accepted-checkpoint.json').read_text());fire=load_fire(a.growth_root)
        state=fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
        areas=fire.areas(c['source']);heat=state['fields']['heat'];total=math.fsum(v*w for v,w in zip(heat,areas))
        cases[label]['right_edge_heat_proxy_fraction']=math.fsum(v*w for i,(v,w) in enumerate(zip(heat,areas)) if i%c['source']['width']==c['source']['width']-1)/total
        cases[label]['fire_response_profile']=profile
        if profile=='physical_rates_heat_outflow_v1':
            import sqlite3
            db=sqlite3.connect((a.output/label/'run/joint.sqlite3').resolve().as_uri()+'?mode=ro',uri=True)
            try:frames=[json.loads(row[0])['source']['frame'] for row in db.execute('SELECT payload FROM checkpoints ORDER BY revision')]
            finally:db.close()
            b=[f['diagnostics']['thermal_transport'] for f in frames if f is not None]
            budgets[label]={'units':'normalized_proxy_m2','initial':b[0]['initial'],'final':b[-1]['final'],
                **{key:math.fsum(v[key] for v in b) for key in ('outflow','response_net','transport_roundoff')}}
    require(cases['open-strong']['right_edge_heat_proxy_fraction']<cases['closed-strong']['right_edge_heat_proxy_fraction'],'open boundary must reduce matched edge trapping')
    require(all(b['outflow']>0 for b in budgets.values()),'open boundary must emit proxy outflow')
    result={'schema':'fire_fluid_boundary_qualification/v1','status':'numerical_cases_passed','cases':cases,
        'comparisons':{'open-versus-closed':comparison(cases['closed-strong'],cases['open-strong']),
                       'open-time-half':comparison(cases['open-strong'],cases['open-time-half'])},
        'proxy_budgets':budgets,'physical_calibration_qualified':False,
        'acceptance':'open transport balance and coupled conservation/recovery; no material calibration'}
    (a.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({key:result[key] for key in ('status','comparisons','proxy_budgets')},indent=2))
if __name__=='__main__':main()
