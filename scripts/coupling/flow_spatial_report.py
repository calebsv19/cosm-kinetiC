"""Strict paired evolving-flow qualification with identical fixed scalar timing."""
import json,math
from pathlib import Path
import numpy as np
from .flow_time_report import SOURCE,pair
from .matched_heat_report import digest
from .flow_spatial_fields import restrict_extensive,restrict_faces,pack,localize
from .velocity import sample_mac
from .isolated_qualification import PROBES

def validate(s,n):
    if (s['qualification_profile']!='flow_spatial_fixed_scalar_v1' or s['grid']!=n or n not in (64,128)
        or s['dt_s']!=.005 or s['time_s']!=3.2 or s['scalar_dt_s']!=.00125
        or s['flow_steps']!=640 or s['scalar_cycles']!=2560 or s['scalar_work_cells']!=2560*n**3
        or s['max_scalar_work_cells']!=8000000000 or s['scalar_transport_scheme']!='muscl_minmod'):
        raise ValueError('paired native timing/grid/work identity')
    if (not 0<=s['maximum_divergence']<1e-8 or not 0<=s['maximum_pressure_residual']<1e-11
        or not abs(s['heat_balance_j'])<1e-9 or not abs(s['smoke_balance_kg'])<1e-15
        or not 300<=s['peak_temperature_k']<=330 or s['memory_limit_bytes']!=2147483648
        or not 0<s['memory_peak_bytes']<=s['memory_limit_bytes']):
        raise ValueError('native physical/numerical/resource invariants')

def load(path,n):
    v=np.fromfile(path,dtype=np.float64);c=n**3
    if len(v)!=5*c+3*n*n or not np.isfinite(v).all() or (v[:2*c]<0).any():raise ValueError('native finite nonnegative fields')
    return dict(energy_j=v[:c],smoke_kg=v[c:2*c],face_velocity_m_s=v[2*c:])

def report(root,prior):
    root,prior=map(Path,(root,prior));mapping=json.loads((root.parent/'source-mapping-check.json').read_text())
    if (not mapping['reconstructed_full_source_bit_identical'] or mapping['original_full_fine_source_sha256']!='8ed9289652afa4dd8bce5d3a872a312752bfc0cf627996e29ffaea55d40d8251' or mapping['maximum_restriction_relative']>=1e-12):raise ValueError('original fine receiver mapping proof')
    identity=json.loads((root/'run-identity.json').read_text())
    if (identity['source_sha256']!=SOURCE or digest(identity['source'])!=SOURCE
        or identity['scalar_dt_s']!=.00125 or identity['flow_dt_s']!=.005 or identity['grids']!=[64,128]
        or digest(identity['fine_source'])!=identity['fine_source_sha256'] or identity['fine_source_sha256']!=mapping['fine_support_sha256']
        or identity['source_grid']!=64 or identity['windows']!=32 or identity['window_s']!=.1
        or digest(root/'qualified-flow-spatial-probe')!=identity['probe_sha256']
        or json.loads((root/'terminal.json').read_text())['returncode']!=0):raise ValueError('paired terminal/source/probe identity')
    summaries=[];arrays=[]
    for n in (64,128):
        run=root/('grid-'+str(n));s=json.loads((run/'summary.json').read_text());validate(s,n)
        if (s['source_sha256']!=SOURCE or s['native_input_sha256']!=(SOURCE if n==64 else identity['fine_source_sha256']) or s['probe_sha256']!=identity['probe_sha256']
            or digest(run/'fields.bin')!=s['fields_sha256']
            or json.loads((run/'terminal.json').read_text())['returncode']!=0
            or 'memory_live_bytes=0 rejected_allocations=0' not in (run/'native.log').read_text()):raise ValueError('case terminal/hash/closure')
        a=load(run/'fields.bin',n)
        if abs(math.fsum(a['energy_j'])-s['energy_j'])>1e-9 or abs(math.fsum(a['smoke_kg'])-s['smoke_kg'])>1e-15:raise ValueError('native totals readback')
        summaries.append(s);arrays.append(a)
    previous=json.loads((prior/'summary.json').read_text())
    if previous['source_sha256']!=SOURCE or digest(prior/'fields.bin')!=previous['fields_sha256'] or summaries[0]['fields_sha256']!=previous['fields_sha256']:raise ValueError('64 control differs from accepted fixed-scalar replay')
    restricted={k:restrict_extensive(arrays[1][k],64) for k in ('energy_j','smoke_kg')}
    restricted['face_velocity_m_s']=pack(restrict_faces(arrays[1]['face_velocity_m_s'],64))
    comparison=pair(arrays[0],restricted,*summaries)
    # Wind probes sample each native grid directly, not the restricted surrogate.
    winds=[{name:sample_mac(a['face_velocity_m_s'],[n]*3,[1.]*3,p,solid_floor=True,lateral_open=True)
            for name,p in PROBES.items()} for a,n in zip(arrays,(64,128))]
    from .flow_time_report import relative_l2
    norm=relative_l2([v for name in PROBES for v in winds[0][name]], [v for name in PROBES for v in winds[1][name]])
    comparison.pop('temporal_screens_passed');comparison.pop('flow_dt_s')
    comparison['wind_probes_m_s']=winds;comparison['wind_probe_norm']=norm
    screens=[comparison['heat']['local_heat_relative_l1'],comparison['heat']['height_profile_relative_l1'],
             comparison['full_mac_velocity']['relative_l2'],norm['relative_l2'],comparison['full_smoke_relative_l1'],comparison['peak_rise_relative_difference']]
    passed=all(v is not None and math.isfinite(v) and v<=.1 for v in screens)
    bands=comparison['heat']['vertical_heat_difference_bands'];delta=comparison['heat']['local_heat_difference_j']
    for b in bands:b['share_of_local_heat_difference']=b['absolute_difference_j']/delta if delta else 0
    return dict(schema='flow_spatial_fixed_scalar_verdict_v1',status='spatial_screens_passed' if passed else 'spatial_screens_failed',
                local_heat_gate_passed=comparison['heat']['local_heat_gate_passed'],spatial_screens_passed=passed,
                heat_threshold_relative=.1,domain_m=[1,1,1],source_region_m=[[.25,.75],[.25,.75],[0,.125]],heat_region_m=[[.25,.75],[.25,.75],[0,.375]],
                identity=identity,source_mapping=mapping,runs=summaries,control_64_bit_identical=True,comparison=comparison,
                velocity_localization=localize(arrays[0]['face_velocity_m_s'],arrays[1]['face_velocity_m_s'],64),
                limitations=['Final-state paired spatial sensitivity; no formal convergence order or calibrated prediction error.',
                'Identical fixed scalar timing and flow timestep; 128-cubed flow temporal sensitivity remains unqualified.',
                'Original Fire receiver mapping at each grid, conservative lower-support embedding; no live fine-grid feedback.',
                'No surface heat exchange, material response or burning faces enabled.'])
