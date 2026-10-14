#!/usr/bin/env python3
"""Read completed direction-specific qualification runs without solver execution.

Individual directional passes do not establish a combined-grid convergence gate.
"""
import argparse,json
from pathlib import Path
from coupling.directional_report import pair
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();rows={}
for side in (32,64):
    rows[str(side)]={}
    for axis in range(3):
        grid=[side]*3;grid[axis]*=2
        name='no-box-'+'x'.join(map(str,grid))
        value=pair(a.root/f'no-box-{side}-parity',a.root/name,[32]*3)
        value['directional_heat_difference_below_threshold']=value['source_region_heat_relative_l1']<.1
        rows[str(side)]['xyz'[axis]]=value
anisotropic={}
for axis in range(3):
    coarse=[32]*3;coarse[axis]*=2
    fine=[2*v for v in coarse]
    value=pair(a.root/('no-box-'+'x'.join(map(str,coarse))),a.root/('no-box-'+'x'.join(map(str,fine))),[32]*3)
    value['full_twofold_grid_heat_difference_below_threshold']=value['source_region_heat_relative_l1']<.1
    anisotropic['xyz'[axis]]=value
box_rows={}
for axis in range(3):
    grid=[32]*3;grid[axis]*=2
    box_rows['xyz'[axis]]=pair(a.root/'box-32-parity',a.root/('box-'+'x'.join(map(str,grid))),[32]*3)
no_box_pass=any(v['full_twofold_grid_heat_difference_below_threshold'] for v in anisotropic.values())
report=dict(status='no_box_local_heat_pass_box_pending' if no_box_pass else 'local_heat_gate_failed',no_box_local_heat_converged=no_box_pass,no_box_anisotropic_full_twofold_refinement=anisotropic,box_directional_32=box_rows,schema='fire_fluid_directional_qualification/v1',threshold_relative_l1=.1,
    prescribed_source_frames=True,plume_duration_s=3.2,dt_s=.005,cases=rows,
    local_heat_converged=False,combined_grid_gate='box full refinement outstanding' if no_box_pass else 'all tested no-box full-refinement pairs fail',
    burning_faces_enabled=False,reciprocal_thermal_exchange=False,
    limits='Analytic evolving translation isolates scalar transport. Native plume fields evolve velocity through momentum and projection. Neither is calibrated fire physics. Insulated-box refinement and a combined-grid local-heat pass remain required.')
with a.output.open('x') as out:out.write(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
