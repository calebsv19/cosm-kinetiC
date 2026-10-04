#!/usr/bin/env python3
"""Predeclared steady C3D-6 gate. No package, commit or continuation dispatch."""
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/c3d-open'
BINARY=OUT/'open3d_test'

def finite(x):return isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x)
def run(name,args):
    start=time.monotonic()
    p=subprocess.run([str(BINARY),*map(str,args)],capture_output=True,text=True,check=True,cwd=ROOT)
    row=json.loads(p.stdout);row['wall_s']=time.monotonic()-start
    (OUT/(name+'.json')).write_text(json.dumps(row,indent=2)+'\n')
    assert row['solved'] and finite(row['residual']) and row['residual']<=1e-11 and finite(row['max_divergence']) and row['max_divergence']<1e-8 and finite(row['flux_error']) and row['flux_error']<=1e-10,row
    return row

def gate(row,flow=.008):
    # Recompute; do not trust the executable's boolean.
    return all(finite(row[k]) and 0<=row[k]<=v for k,v in [('velocity_error',.01),('pressure_error',.01),('energy_error',.02),('energy_imbalance',.02)]) and all(finite(x) and 0<=x<=.02 for x in row['wall_error']) and all(abs(row[k]/flow-1)<=.01 for k in ('inlet_flow','outlet_flow'))

def rel(a,b):return abs(b-a)/max(abs(a),1e-30)

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    grids=[run('grid'+str(n),[n]) for n in (8,16,32)]
    assert [gate(row) for row in grids]==[False,False,True]
    orders=[{k:math.log(a[k]/b[k],2) for k in ('velocity_error','pressure_error','energy_error')} for a,b in zip(grids,grids[1:])]
    outlets=[grids[-1],run('outlet6',[32,6]),run('outlet8',[32,8])]
    changes=[]
    for a,b in zip(outlets,outlets[1:]):
        changes.append({'upstream_gradient_relative_change':rel(a['upstream_gradient_pa_m'],b['upstream_gradient_pa_m']),
                        'wall_per_length_relative_change':[rel(x/a['length_m'],y/b['length_m']) for x,y in zip(a['wall_force_n'],b['wall_force_n'])],
                        'dissipation_per_length_relative_change':rel(a['dissipation_w']/a['length_m'],b['dissipation_w']/b['length_m'])})
    assert all(gate(row) for row in outlets)
    assert all(row['upstream_gradient_relative_change']<=.01 and row['dissipation_per_length_relative_change']<=.01 and max(row['wall_per_length_relative_change'])<=.01 for row in changes),changes
    rectangle=run('rectangle',[24,4,1.5,3,.1,0,32]);assert gate(rectangle,.009)
    mu2=run('mu2',[8,4,2,2,.2]);datum=run('datum',[8,4,2,2,.1,.02]);base=grids[0]
    viscosity_checks={k:rel(2*base[k],mu2[k]) for k in ('pressure_drop_pa','dissipation_w','boundary_power_w')}
    viscosity_checks['walls']=max(rel(2*x,y) for x,y in zip(base['wall_force_n'],mu2['wall_force_n']))
    assert max(viscosity_checks.values())<1e-9
    datum_checks={'inlet_shift_error_pa':abs(datum['pressure_in_pa']-base['pressure_in_pa']-.02),'outlet_shift_error_pa':abs(datum['pressure_out_pa']-base['pressure_out_pa']-.02),
                  'velocity_error_change':abs(datum['velocity_error']-base['velocity_error']),
                  'pressure_drop_relative_change':rel(base['pressure_drop_pa'],datum['pressure_drop_pa']),
                  'physical_dissipation_relative_change':rel(base['dissipation_w'],datum['dissipation_w'])}
    assert max(datum_checks.values())<1e-9
    result={'schema':'physics_sim_c3d_open_qualification_v1','passed':True,'scope':'stationary straight Stokes duct with analytic inlet, no-slip walls and natural vector-Laplacian outlet only',
            'binary_sha256':hashlib.sha256(BINARY.read_bytes()).hexdigest(),'grids':grids,'spatial_orders':orders,'outlet_extensions':outlets,'outlet_changes':changes,
            'rectangle':rectangle,'viscosity_scaling':viscosity_checks,'datum_covariance':datum_checks,
            'limitations':['no wall transient or wake/outflow transport proof','no obstacle/closed body force certificate','serial cached nested Krylov; controls between stationary solves','uniform grids; no adaptive mesh/GPU']}
    (OUT/'qualification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'passed':True,'orders':orders,'outlet_changes':changes}))

if __name__=='__main__':main()
