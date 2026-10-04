#!/usr/bin/env python3
"""Assess a bounded total-drag reference case; do not certify arbitrary scenes."""
import argparse,json,re
from pathlib import Path

def records(path):
    return [{k:float(v) for k,v in re.findall(r'(\w+)=([^ ]+)',line)}
            for line in path.read_text().splitlines() if line.startswith('n=') and 'velocity_change_relative=' in line]

def difference(a,b):return abs(a-b)/abs(b)

p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);args=p.parse_args();root=args.root
ref=json.loads((root/'stokes.json').read_text());fine=json.loads((root/'stokes-128.json').read_text())['rows'][-1]
ref64=ref['rows'][-1];ref_long=json.loads((root/'stokes-long.json').read_text())['rows'][-1]
mac=records(root/'corrected-mac.log');half=records(root/'half-re.log')[-1];long=records(root/'long-mac.log')[-1]
rows=[]
for n in (16,32,64):
    group=[r for r in mac if r['n']==n];assert len(group)>=2
    assert all(r['velocity_change_relative']<1e-6 for r in group[-2:])
    assert abs(group[-1]['time']-group[-2]['time']-1)<1e-7
    r=group[-1]
    rows.append(dict(r,surface_reference_error=difference(r['surface_force'],fine['reaction_drag_n']),
                     cv_reference_error=difference(r['cv_force'],fine['reaction_drag_n']),
                     pressure_reference_difference=difference(r['pressure'],fine['pressure_drag_n']),
                     viscous_reference_difference=difference(r['viscous'],fine['viscous_drag_n'])))
checks={
 'reference_total_drag_refinement':difference(fine['reaction_drag_n'],ref64['reaction_drag_n'])<.005,
 'reference_surface_reaction_agreement':difference(fine['pressure_drag_n']+fine['viscous_drag_n'],fine['reaction_drag_n'])<.005,
 'mac_total_drag_reference':rows[-1]['surface_reference_error']<.02,
 'mac_cv_reference':rows[-1]['cv_reference_error']<.02,
 'mac_surface_refinement':rows[0]['surface_reference_error']>rows[1]['surface_reference_error']>rows[2]['surface_reference_error'],
 'mac_low_re_sensitivity':difference(half['cv_force']/half['inlet_mean'],rows[-1]['cv_force']/rows[-1]['inlet_mean'])<.001,
 'mac_domain_sensitivity':difference(long['cv_force'],rows[1]['cv_force'])<.001,
 'fem_domain_sensitivity':difference(ref_long['reaction_drag_n'],ref64['reaction_drag_n'])<.001,
 'steady_mass_and_divergence':all(r['mass_error']<1e-8 and r['divergence']<1e-8 for r in rows),
 'exact_reference_calibration':ref['calibration']['velocity_reference_error']<1e-10 and ref['calibration']['pressure_reference_error']<1e-9}
report={'schema':'physics_sim_confined_drag_reference_v1','scope':'One stationary 1 by .5 m rectangle in a 4 by 2 m channel, .5 m width, rho1 mu.1 mean speed .002; total drag only',
 'boundary_scope':'FEM natural vector-Laplacian traction; MAC pressure outlet. Difference screened by doubled domain; no formal BC equivalence claim',
 'reference_screen_passed':all(checks.values()),'general_drag_qualified':False,'individual_force_components_qualified':False,
 'reference_drag_n':fine['reaction_drag_n'],'reference_relative_refinement':difference(fine['reaction_drag_n'],ref64['reaction_drag_n']),
 'checks':checks,'mac_runs':rows,'half_re':half,'long_mac':long,'long_fem':ref_long}
(root/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
raise SystemExit(0 if report['reference_screen_passed'] else 1)
