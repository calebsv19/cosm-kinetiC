#!/usr/bin/env python3
"""Report independently refined component errors and masked-energy gates.

A successful report generation does not mean force qualification passed.
The component_force_gate field is the authoritative result.
"""
import argparse,json,re,hashlib
from pathlib import Path

def rows(path,prefix):
    return [{k:float(v) for k,v in re.findall(r'(\w+)=([^ ]+)',line)} for line in path.read_text().splitlines() if line.startswith(prefix)]
def relative(a,b):return abs(a-b)/abs(b)
p=argparse.ArgumentParser(description=__doc__);p.add_argument('root',type=Path);root=p.parse_args().root
ref8=json.loads((root/'stokes-corners8.json').read_text())['rows'][-1]
ref=json.loads((root/'stokes-corners11.json').read_text())['rows'][-1]
mac=rows(root/'current-reference.log','n=')+rows(root/'mac128.log','n=')
final=[next(r for r in reversed(mac) if r['n']==n) for n in (16,32,64,128)]
for r in final:
    r['pressure_reference_error']=relative(r['pressure'],ref['pressure_drag_n'])
    r['viscous_reference_error']=relative(r['viscous'],ref['viscous_drag_n'])
    r['total_reference_error']=relative(r['surface_force'],ref['reaction_drag_n'])
transient=rows(root/'masked-transient.log','n=')
assert [(r['n'],r['dt']) for r in transient]==[(16,.001),(32,.001),(64,.001),(64,.0005)]
energy=rows(root/'current-reference.log','energy_n=')+rows(root/'mac128.log','energy_n=')
steady=[next(r for r in reversed(energy) if r['energy_n']==n) for n in (16,32,64,128)]
half=rows(root/'reference-halfdt.log','n=')[-1]
half_energy=rows(root/'reference-halfdt.log','energy_n=')[-1]
report={'schema':'physics_sim_component_energy_v1','reference':ref,
 'reference_component_refinement':{key:relative(ref[key],ref8[key]) for key in ('pressure_drag_n','viscous_drag_n')},
 'mac':final,'steady_energy':steady,'transient_energy':transient,
 'half_dt_force_sensitivity':{key:relative(half[key],final[2][key]) for key in ('pressure','viscous','surface_force')},
 'half_dt_energy':half_energy,
 'checks':{'reference_components_stable':all(relative(ref[k],ref8[k])<.005 for k in ('pressure_drag_n','viscous_drag_n')),
 'force_errors_decrease_32_to_128':all(final[1][k]>final[2][k]>final[3][k] for k in ('pressure_reference_error','viscous_reference_error')),
 'steady_energy_fine':abs(steady[2]['relative_energy_residual'])<.02 and abs(half_energy['relative_energy_residual'])<.02,
 'transient_energy_fine':all(abs(r['relative_residual'])<.02 for r in transient if r['n']==64),
 'total_force_fine':final[2]['total_reference_error']<.02,
 'steady_component_timestep_sensitivity':all(relative(half[k],final[2][k])<1e-4 for k in ('pressure','viscous'))},
 'component_force_gate':{'relative_error_limit':.02,'status':'passed' if max(final[2]['pressure_reference_error'],final[2]['viscous_reference_error'])<.02 else 'failed'},
 'scope':'Stationary confined rectangle, 2D, Re=.01. Transient energy .1..1.1 s excludes initial clipping/projection impulse. 128 grid is verification-only; agent cap remains 64. No 3D qualification.',
 'three_dimensional_gate_started':False}
files=['src/app/cfd_open2d.c','src/app/cfd_open2d_budget.c','src/app/cfd_open2d_force_check.c','scripts/cfd_fem_reference.py','tests/cfd_masked_energy_test.c','tests/cfd_masked_energy_transient.c','tests/cfd_open2d_reference_test.c']
report['verification_binary_sha256']=hashlib.sha256(Path('build/cfd_component_refinement').read_bytes()).hexdigest()
report['verification_128_build']='CFD_OPEN2D_GRID_LIMIT=128; verification-only build before later nonfinite-guess guard and CLI timestep-factor option, no numerical stencil difference'
report['source_sha256']={x:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in files}
(root/'assessment.json').write_text(json.dumps(report,indent=2)+'\n')
assert all(report['checks'].values()),report['checks']
print(json.dumps({'checks':report['checks'],'component_force_gate':report['component_force_gate']},indent=2))
