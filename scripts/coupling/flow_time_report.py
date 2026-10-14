"""Temporal sensitivity of an evolving 64-cubed plume at fixed scalar dt.

This does not qualify independent 128-cubed flow, spatial convergence, surface
heat flux or physical calibration. The existing spatial gate remains separate.
"""
import json
import math
from pathlib import Path
from fire_fluid_accuracy import fields
from .matched_heat_report import compare, digest
from .surface_observations import relative_l1
from .isolated_qualification import PROBES
from .velocity import sample_mac

TIMES = [.005, .0025, .00125]
SOURCE = 'bd4e4f8b42f0f9456377aab943d7e3da93f531d86ba6e44c59566c27f076288e'

def validate(s, dt):
    if (s['qualification_profile'] != 'flow_time_64_v1' or s['grid'] != 64
        or s['dt_s'] != dt or s['time_s'] != 3.2 or s['scalar_dt_s'] != .00125
        or s['flow_steps'] != round(3.2/dt) or s['scalar_cycles'] != 2560
        or s['scalar_work_cells'] != 2560*64**3
        or s['scalar_transport_scheme'] != 'muscl_minmod'):
        raise ValueError('fixed source/grid/time/scalar identity')
    if (not 0 <= s['maximum_divergence'] < 1e-8
        or not 0 <= s['maximum_pressure_residual'] < 1e-11
        or not abs(s['heat_balance_j']) < 1e-9
        or not abs(s['smoke_balance_kg']) < 1e-15
        or not 300 <= s['peak_temperature_k'] <= 330
        or s['memory_limit_bytes'] != 2147483648
        or not 0 < s['memory_peak_bytes'] <= s['memory_limit_bytes']):
        raise ValueError('native conservation/projection/contrast/resources')

def relative_l2(a, b):
    if len(a) != len(b):raise ValueError('matched velocity dimensions')
    delta = math.sqrt(math.fsum((x-y)**2 for x, y in zip(a, b)))
    scale = math.sqrt(math.fsum(y*y for y in b))
    return dict(absolute_l2=delta, fine_l2=scale,
                relative_l2=delta/scale if scale else None)

def pair(a, b, sa, sb, n=64):
    heat = compare(a['energy_j'], b['energy_j'], n)
    velocity = relative_l2(a['face_velocity_m_s'], b['face_velocity_m_s'])
    winds = [{name: sample_mac(f['face_velocity_m_s'], [n]*3, [1.]*3, point,
                 solid_floor=True, lateral_open=True) for name, point in PROBES.items()}
             for f in (a, b)]
    wind = relative_l2([v for name in PROBES for v in winds[0][name]],
                       [v for name in PROBES for v in winds[1][name]])
    smoke = relative_l1(a['smoke_kg'], b['smoke_kg'])
    rise = sb['peak_temperature_k']-300
    peak = abs(sa['peak_temperature_k']-sb['peak_temperature_k'])/rise if rise else None
    screens = [heat['local_heat_relative_l1'], heat['height_profile_relative_l1'],
               velocity['relative_l2'], wind['relative_l2'], smoke, peak]
    return dict(flow_dt_s=[sa['dt_s'], sb['dt_s']], heat=heat,
                full_mac_velocity=velocity, wind_probes_m_s=winds, wind_probe_norm=wind,
                full_smoke_relative_l1=smoke, peak_rise_relative_difference=peak,
                temporal_screens_passed=all(v is not None and math.isfinite(v) and v <= .1 for v in screens))

def report(root, spatial):
    root, spatial = map(Path, (root, spatial))
    identity = json.loads((root/'run-identity.json').read_text())
    if (identity['source_sha256'] != SOURCE or digest(identity['source']) != SOURCE
        or identity['scalar_dt_s'] != .00125 or identity['flow_dt_s'] != TIMES
        or digest(root/'qualified-flow-time-probe') != identity['probe_sha256']
        or json.loads((root/'terminal.json').read_text())['returncode'] != 0):
        raise ValueError('terminal source/executable identity')
    summaries, arrays, retained = [], [], {}
    for dt, label in zip(TIMES, ('005', '0025', '00125')):
        run = root/('flow-'+label)
        s = json.loads((run/'summary.json').read_text());validate(s, dt)
        if (s['source_sha256'] != SOURCE or s['probe_sha256'] != identity['probe_sha256']
            or digest(run/'fields.bin') != s['fields_sha256']
            or json.loads((run/'terminal.json').read_text())['returncode'] != 0
            or 'memory_live_bytes=0 rejected_allocations=0' not in (run/'native.log').read_text()):
            raise ValueError('case identity and allocation closure')
        a = fields(run/'fields.bin', 64)
        if any(not math.isfinite(v) for row in a.values() for v in row):
            raise ValueError('nonfinite native fields')
        if any(v < 0 for k in ('energy_j', 'smoke_kg') for v in a[k]):
            raise ValueError('negative native scalars')
        summaries.append(s);arrays.append(a);retained[label] = s['fields_sha256']
    pairs = [pair(arrays[i], arrays[i+1], summaries[i], summaries[i+1]) for i in range(2)]
    def ratio(x, y): return y/x if x else None
    trend = dict(local_heat_difference_ratio=ratio(pairs[0]['heat']['local_heat_difference_j'],
                  pairs[1]['heat']['local_heat_difference_j']),
                 velocity_difference_ratio=ratio(pairs[0]['full_mac_velocity']['absolute_l2'],
                  pairs[1]['full_mac_velocity']['absolute_l2']))
    trend_pass = all(v is not None and math.isfinite(v) and v < 1 for v in trend.values())
    old = json.loads(spatial.read_text())
    return dict(schema='flow_time_verdict_v1',
        status='64_flow_temporal_screen_passed' if all(p['temporal_screens_passed'] for p in pairs) and trend_pass else '64_flow_temporal_screen_failed',
        source_sha256=SOURCE, probe_sha256=identity['probe_sha256'], field_sha256=retained,
        domain_m=[1,1,1], source_region_m=[[.25,.75],[.25,.75],[0,.125]],
        heat_region_m=[[.25,.75],[.25,.75],[0,.375]], heat_threshold_relative=.1,
        scalar_dt_s=.00125, runs=summaries, adjacent_time_comparisons=pairs,
        refinement_difference_trend=trend, differences_shrink=trend_pass,
        retained_spatial_verdict=dict(path=str(spatial.resolve()), sha256=digest(spatial), status=old['status'],
             heat_threshold_relative=old['local_heat_gate'], passed=old['local_heat_gate_passed'],
             local_heat_relative_l1=old['stabilization']['relative_changes']['local_heat_l1']),
        limitations=['Final-state temporal comparison on independently evolved 64-cubed flow only.',
                    '128-cubed flow timestep sensitivity is unqualified; the prior 13.146 percent spatial heat failure remains unresolved.',
                    'Differences are sensitivity estimates, not calibrated prediction errors.',
                    'Surface heat flux, material response and burning objects remain disabled.'])
