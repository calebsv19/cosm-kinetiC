"""Three full grids, one physical observation partition, unchanged heat gate.

Refinement differences are not errors against an exact/calibrated solution.
The observed trend is an indicator, not proof of an asymptotic order.
"""
import math
from .convergence import restrict
from .directional_report import read,compare_runs

THRESHOLD=.1

def refinement_pattern(fields,grids,common_grid):
    common=[restrict(f['energy_j'],g,common_grid) for f,g in zip(fields,grids)]
    nx,ny,nz=common_grid
    indices=[(k*ny+j)*nx+i for k in range(3*nz//8)
             for j in range(ny//4,3*ny//4) for i in range(nx//4,3*nx//4)]
    differences=[[b[q]-a[q] for q in indices] for a,b in zip(common,common[1:])]
    norms=[math.fsum(abs(v) for v in d) for d in differences]
    scale=math.fsum(abs(common[-1][q]) for q in indices)
    dot=math.fsum(a*b for a,b in zip(*differences))
    length=math.prod(math.sqrt(math.fsum(v*v for v in d)) for d in differences)
    return dict(common_grid=common_grid,absolute_difference_j=norms,
                relative_differences_same_fine_scale=[v/scale for v in norms],
                difference_ratio=norms[1]/norms[0] if norms[0] else None,
                observed_order_indicator=math.log2(norms[0]/norms[1]) if all(norms) else None,
                difference_pattern_cosine=dot/length if length else None)

def stabilization(pair,coarse,fine):
    wind_scale=math.sqrt(math.fsum(v*v for vector in pair['wind_probes_m_s'][1].values() for v in vector))
    wind_delta=pair['wind_probe_difference_l2_m_s']
    wind_relative=wind_delta/wind_scale if wind_scale else (0. if wind_delta==0 else None)
    rise=fine['peak_temperature_k']-300.
    peak_delta=abs(coarse['peak_temperature_k']-fine['peak_temperature_k'])
    peak_relative=peak_delta/rise if rise else (0. if peak_delta==0 else None)
    quantities=dict(local_heat_l1=pair['source_region_heat_relative_l1'],
                    height_temperature_rise_profile_l1=pair['height_profile_relative_l1'],
                    full_smoke_l1=pair['full_fields']['smoke_kg']['relative_l1'],
                    aggregate_wind_probe_l2=wind_relative,peak_temperature_rise=peak_relative)
    passed={k:v is not None and math.isfinite(v) and 0<=v<=THRESHOLD for k,v in quantities.items()}
    return dict(threshold_relative=THRESHOLD,relative_changes=quantities,passed=passed,
                all_passed=all(passed.values()),wind_difference_l2_m_s=wind_delta,
                fine_wind_probe_norm_m_s=wind_scale,peak_difference_k=peak_delta)

def report(root,source_restriction):
    runs=[read(root/f'no-box-{n}',full_grid=True) for n in (32,64,128)]
    if [v[2] for v in runs]!=[[n]*3 for n in (32,64,128)] or any(v[0]['box'] for v in runs):
        raise ValueError('full 32/64/128 cubic no-box runs required')
    if not source_restriction['physical_source_restriction_passed'] or source_restriction['max_relative_l1']>=1e-12 or source_restriction['fine_sha256']!=runs[2][0]['source']['sha256'] or source_restriction['coarse_sha256']!=runs[1][0]['source']['sha256']:
        raise ValueError('physical source restriction proof mismatch')
    pairs=[compare_runs(*runs[i],*runs[i+1],[32]*3) for i in range(2)]
    trend=refinement_pattern([v[1] for v in runs],[v[2] for v in runs],[32]*3)
    screen=stabilization(pairs[1],runs[1][0],runs[2][0])
    if screen['all_passed']:status='fine_grid_stabilizes_for_prescribed_no_box_plume'
    elif screen['passed']['local_heat_l1']:status='heat_pass_observables_not_stable'
    elif trend['difference_ratio'] is not None and trend['difference_ratio']<1:
        status='resolution_sensitive_with_decreasing_difference'
    else:status='refinement_not_decreasing_requires_operator_isolation'
    return dict(status=status,local_heat_gate=THRESHOLD,local_heat_gate_passed=screen['passed']['local_heat_l1'],
                stabilization=screen,refinement_trend=trend,pairs=pairs,
                native_runs=[v[0] for v in runs],prescribed_source_replay=True,
                time_discretization=dict(outer_dt_s=.005,fluid_steps=640,
                    total_scalar_cycles=[v[0]['scalar_work_cells']//math.prod(v[2]) for v in runs],
                    limit='Matched outer dt; accepted CFL control produces grid-dependent scalar subcycles. Independent fine-grid time-error qualification is not established.'),
                surface_heating_enabled=False,burning_faces_enabled=False,
                limits='Only this accepted-solver no-box prescribed-source plume. No exact solution, physical calibration, fine-grid live Fire feedback, obstacle or surface-response admission. Ordinary worker capacity unchanged.')
