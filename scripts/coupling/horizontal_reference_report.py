"""X refinement at fixed vertical resolution, source and scalar/flow timing."""
import json,math
from pathlib import Path
import numpy as np
from .matched_heat_report import digest
from .matched_vertical_report import validate as validate_reference
from .vertical_reference_report import load,dense,winds,l2,SOURCE
from .flow_spatial_fields import pack
from .isolated_qualification import PROBES
GRID=[256,128,256]
REFERENCE_GRID=[128,128,256]

def validate(s):
    expected=dict(qualification_profile='flow_horizontal_matched_0025_v1',grid=GRID,dt_s=.0025,time_s=3.2,
                  scalar_dt_s=.00125,flow_steps=1280,scalar_cycles=2560,scalar_work_cells=2560*math.prod(GRID),
                  max_scalar_work_cells=22000000000,scalar_transport_scheme='muscl_minmod',memory_limit_bytes=8589934592)
    if any(s[k]!=v for k,v in expected.items()):raise ValueError('horizontal grid/timing/work identity')
    if (not 0<=s['maximum_divergence']<1e-8 or not 0<=s['maximum_pressure_residual']<1e-11
        or not abs(s['heat_balance_j'])<1e-9 or not abs(s['smoke_balance_kg'])<1e-15
        or not 300<=s['peak_temperature_k']<=330 or not 0<s['memory_peak_bytes']<=s['memory_limit_bytes']):
        raise ValueError('horizontal numerical/resource invariants')

def restrict_x(a, grid=GRID):
    x,y,z=grid
    if x%2:raise ValueError('even X dimension')
    f=dense(a['face_velocity_m_s'],grid)
    return dict(energy_j=a['energy_j'].reshape(z,y,x//2,2).sum(axis=3).ravel(),
                smoke_kg=a['smoke_kg'].reshape(z,y,x//2,2).sum(axis=3).ravel(),
                face_velocity_m_s=pack([f[0][:,:,::2],f[1].reshape(z,y+1,x//2,2).mean(axis=3),
                                        f[2].reshape(z+1,y,x//2,2).mean(axis=3)]))

def heat_compare(a,b,grid=REFERENCE_GRID):
    x,y,z=grid;q=(slice(0,3*z//8),slice(y//4,3*y//4),slice(x//4,3*x//4))
    aa=a.reshape(z,y,x);bb=b.reshape(z,y,x);scale=float(bb[q].sum());delta=float(np.abs(aa[q]-bb[q]).sum())
    if scale<=0:raise ValueError('positive ROI heat scale')
    profiles=[v[:,y//4:3*y//4,x//4:3*x//4].reshape(32,z//32,y//2,x//2).sum(axis=(1,2,3)) for v in (aa,bb)]
    return dict(local_heat_relative_l1=delta/scale,local_heat_difference_j=delta,fine_local_heat_j=scale,
                threshold_relative=.1,local_heat_gate_passed=delta/scale<=.1,
                height_profile_relative_l1=float(np.abs(profiles[0]-profiles[1]).sum()/profiles[1].sum()))

def report(root):
    r=Path(root);i=json.loads((r/'run-identity.json').read_text());ref=Path(i['reference'])
    if (i['grid']!=GRID or i['reference_grid']!=REFERENCE_GRID or i['flow_dt_s']!=.0025 or i['scalar_dt_s']!=.00125
        or i['windows']!=32 or i['window_s']!=.1 or i['heat_gate_relative']!=.1 or i['source_sha256']!=SOURCE
        or digest(i['source'])!=SOURCE or digest(r/'matched-horizontal-probe')!=i['probe_sha256']):raise ValueError('run source/probe identity')
    for run in (r,ref):
        if json.loads((run/'native-terminal.json').read_text())['returncode']!=0 or 'memory_live_bytes=0 rejected_allocations=0' not in (run/'native.log').read_text():raise ValueError('native completion/allocation closure')
    if json.loads((ref/'terminal.json').read_text())['status']!='matched_comparison_complete':raise ValueError('accepted reference terminal')
    ri=json.loads((ref/'run-identity.json').read_text())
    if ri['source_sha256']!=SOURCE or digest(ref/'matched-vertical-probe')!=ri['probe_sha256'] or digest(ref/'fields.bin')!=i['reference_fields_sha256'] or digest(ref/'native.json')!=i['reference_summary_sha256']:raise ValueError('reference provenance')
    if digest(r/'fields.bin')!=json.loads((r/'field-identity.json').read_text())['sha256']:raise ValueError('final fields hash')
    sa=json.loads((ref/'native.json').read_text());sb=json.loads((r/'native.json').read_text());validate_reference(sa);validate(sb)
    a=load(ref/'fields.bin',REFERENCE_GRID);native=load(r/'fields.bin',GRID);b=restrict_x(native)
    for key,tol in [('energy_j',1e-9),('smoke_kg',1e-15)]:
        if abs(float(a[key].sum())-sa[key])>=tol or abs(float(native[key].sum())-sb[key])>=tol or abs(float(native[key].sum())-float(b[key].sum()))>=tol:raise ValueError('totals/conservative restriction')
    heat=heat_compare(a['energy_j'],b['energy_j']);velocity=l2(a['face_velocity_m_s'],b['face_velocity_m_s'])
    ws=[winds(a,REFERENCE_GRID),winds(native,GRID)];wind=l2([v for p in PROBES for v in ws[0][p]],[v for p in PROBES for v in ws[1][p]])
    smoke=float(np.abs(a['smoke_kg']-b['smoke_kg']).sum()/b['smoke_kg'].sum());rise=sb['peak_temperature_k']-300
    peak=abs(sa['peak_temperature_k']-sb['peak_temperature_k'])/rise if rise else None
    metrics=[heat['local_heat_relative_l1'],heat['height_profile_relative_l1'],velocity['relative_l2'],wind['relative_l2'],smoke,peak]
    return dict(schema='horizontal_x_matched_verdict_v1',identity=i,heat_threshold_relative=.1,heat_region_m=[[.25,.75],[.25,.75],[0,.375]],
                heat=heat,full_mac_velocity=velocity,wind_probes_m_s=ws,wind_probe_norm=wind,full_smoke_relative_l1=smoke,
                peak_rise_relative_difference=peak,horizontal_heat_gate_passed=heat['local_heat_gate_passed'],
                horizontal_screens_passed=all(v is not None and math.isfinite(v) and v<=.1 for v in metrics),runs=[sa,sb],
                limitations=['X doubles; Y remains 128 and Z remains 256. Y refinement and full three-axis convergence remain unqualified.',
                             'Both runs independently evolve from zero at 2.5 ms flow and 1.25 ms scalar timing.',
                             'No surface heating, burning faces or measured physical calibration qualified.'])
