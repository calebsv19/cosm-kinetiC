"""Bounded heat-only attribution under one prescribed evolving MAC flow.

Refinement differences are not calibrated errors. The prescribed fine flow is
the flux-preserving coarse representation, not an independently solved flow.
"""
from array import array
import hashlib
import json
import math
from pathlib import Path
from .convergence import restrict
from .directional_report import region_difference
from .surface_observations import relative_l1

THRESHOLD = .1
EXPECTED = [(64, .005), (64, .0025), (128, .005), (128, .0025)]

def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''):
            h.update(block)
    return h.hexdigest()

def validate_native(native):
    if native['schema'] != 'matched_heat_v1' or native['velocity_driver_grid'] != 64 or native['flow_dt_s'] != .005 or native['time_s'] != 3.2:
        raise ValueError('matched evolving flow identity')
    if not native['control_bit_identical_each_step'] or native['control_max_difference_j'] != 0 or native['driver_scalar_cycles'] != 640:
        raise ValueError('accepted control parity')
    if not 0 <= native['driver_max_divergence'] < 1e-8 or not 0 <= native['fine_max_divergence'] < 1e-8 or not 0 <= native['driver_max_pressure_residual'] < 1e-11:
        raise ValueError('flow divergence/projection')
    if native['memory_limit_bytes'] != 2147483648 or not 0 < native['memory_peak_bytes'] <= native['memory_limit_bytes'] or native['memory_live_bytes'] or native['rejected_allocations']:
        raise ValueError('allocation budget/closure')
    rows = native['carriers']
    if [(v['grid'], v['scalar_dt_s']) for v in rows] != EXPECTED:
        raise ValueError('explicit scalar timestep matrix')
    for row in rows:
        if row['scalar_cycles'] != round(3.2 / row['scalar_dt_s']) or not 0 <= row['maximum_courant_bound'] <= .4:
            raise ValueError('controlled cycles/CFL')
        for key in ('energy_j', 'input_j', 'inflow_j', 'outflow_j'):
            if not math.isfinite(row[key]) or row[key] < 0:
                raise ValueError('finite heat accounting')
        if not math.isfinite(row['heat_balance_j']) or abs(row['heat_balance_j']) >= 1e-9 or not 300 <= row['peak_temperature_k'] <= 330:
            raise ValueError('heat budget/contrast')
    if max(v['input_j'] for v in rows) - min(v['input_j'] for v in rows) >= 1e-9:
        raise ValueError('matched integrated source')

def load(path, count):
    if path.stat().st_size != count * 8:
        raise ValueError('native field byte count')
    values = array('d')
    with path.open('rb') as f:
        values.fromfile(f, count)
    if any(not math.isfinite(v) or v < 0 for v in values):
        raise ValueError('finite nonnegative heat field')
    return values

def indices(n):
    return [(k*n+j)*n+i for k in range(3*n//8)
            for j in range(n//4, 3*n//4) for i in range(n//4, 3*n//4)]

def profile(values, n):
    return [math.fsum(values[(k*n+j)*n+i]
                     for k in range(z*n//32, (z+1)*n//32)
                     for j in range(n//4, 3*n//4) for i in range(n//4, 3*n//4))
            / (1.2*1005*.25/32) for z in range(32)]

def compare(coarse, fine, n):
    chosen = indices(n)
    delta = math.fsum(abs(coarse[q]-fine[q]) for q in chosen)
    scale = math.fsum(fine[q] for q in chosen)
    relative = region_difference(coarse, fine, [n]*3)
    bands=[]
    for lower,upper in ((0.,.125),(.125,.25),(.25,.375)):
        qs=[(k*n+j)*n+i for k in range(round(lower*n),round(upper*n))
            for j in range(n//4,3*n//4) for i in range(n//4,3*n//4)]
        bands.append(dict(z_m=[lower,upper],absolute_difference_j=math.fsum(abs(coarse[q]-fine[q]) for q in qs),
                          fine_heat_j=math.fsum(fine[q] for q in qs)))
    return dict(local_heat_relative_l1=relative, local_heat_difference_j=delta,
                fine_local_heat_j=scale, threshold_relative=THRESHOLD,
                local_heat_gate_passed=relative is not None and relative <= THRESHOLD,
                height_profile_relative_l1=relative_l1(profile(coarse,n),profile(fine,n)),
                vertical_heat_difference_bands=bands)

def report(root, control, coupled_fine):
    root, control, coupled_fine = map(Path, (root, control, coupled_fine))
    native = json.loads((root/'run/native.json').read_text())
    validate_native(native)
    terminal = json.loads((root/'run-terminal.json').read_text())
    if terminal['returncode'] != 0:
        raise ValueError('native replay incomplete')
    identity = json.loads((root/'run-identity.json').read_text())
    if digest(root/'qualified-matched-heat-probe') != identity['probe_sha256']:
        raise ValueError('native replay executable digest')
    if digest(identity['source']) != identity['source_sha256']:
        raise ValueError('source digest drift')
    old64 = json.loads((control/'summary.json').read_text())
    old128 = json.loads((coupled_fine/'summary.json').read_text())
    if old64['source']['sha256'] != identity['source_sha256'] or old64['source_frames_sha256'] != old128['source_frames_sha256'] or old64['box'] or old128['box'] or old64['grid'] != 64 or old128['grid'] != 128:
        raise ValueError('accepted reference identity')
    for p, info in ((control,old64),(coupled_fine,old128)):
        if digest(p/'fields.bin') != info['fields_sha256']:
            raise ValueError('accepted reference field digest')
    original = (control/'fields.bin').read_bytes()
    driver = (root/'run/driver-fields.bin').read_bytes() + (root/'run/driver-velocity.bin').read_bytes()
    if original != driver:
        raise ValueError('accepted full driver final parity')
    fields = [load(root/'run'/f"{v['name']}.bin",v['grid']**3) for v in native['carriers']]
    if fields[0].tobytes() != original[:64**3*8]:
        raise ValueError('heat control final parity')
    for values, row in zip(fields,native['carriers']):
        if abs(math.fsum(values)-row['energy_j']) >= 1e-9:
            raise ValueError('native heat total readback')
    restricted = [restrict(v,[128]*3,[64]*3) for v in fields[2:]]
    spatial = [compare(fields[i],restricted[i],64) for i in range(2)]
    temporal = [compare(fields[i],fields[i+1],native['carriers'][i]['grid']) for i in (0,2)]
    for i,row in enumerate(spatial):
        rise=native['carriers'][i+2]['peak_temperature_k']-300
        row['peak_temperature_difference_k']=abs(native['carriers'][i]['peak_temperature_k']-native['carriers'][i+2]['peak_temperature_k'])
        row['peak_temperature_rise_relative']=row['peak_temperature_difference_k']/rise if rise else None
    with (coupled_fine/'fields.bin').open('rb') as f:
        old_energy = array('d');old_energy.fromfile(f,128**3)
    old_restricted = restrict(old_energy,[128]*3,[64]*3)
    selected=indices(64)
    transport=[restricted[0][q]-fields[0][q] for q in selected]
    remainder=[old_restricted[q]-restricted[0][q] for q in selected]
    coupled=[old_restricted[q]-fields[0][q] for q in selected]
    decomposition=dict(matched_transport_difference_j=math.fsum(abs(v) for v in transport),
                       other_flow_and_cycle_difference_j=math.fsum(abs(v) for v in remainder),
                       full_coupled_difference_j=math.fsum(abs(v) for v in coupled),
                       identity_max_residual_j=max(abs(a+b-c) for a,b,c in zip(transport,remainder,coupled)),
                       limitation='Remainder combines independent fine-flow evolution, coarse-flow prolongation representation and prior adaptive scalar cycles; these are not isolated contributions and L1 norms are not additive shares.')
    temporal_stable=all(v['local_heat_relative_l1'] is not None and v['local_heat_relative_l1']<=.01 for v in temporal)
    passed=spatial[1]['local_heat_gate_passed']
    return dict(schema='matched_heat_report_v1',native=native,
                source_identity=identity,source_frames_sha256=old64['source_frames_sha256'],
                full_control_bit_identical=True,spatial_5ms=spatial[0],spatial_2_5ms=spatial[1],
                timestep_refinement_64=temporal[0],timestep_refinement_128=temporal[1],
                temporal_1percent_diagnostic_screen_passed=temporal_stable,
                status=('matched_flow_heat_spatial_gate_passes' if passed else 'matched_flow_heat_spatial_gate_fails'),
                decomposition=decomposition,
                field_sha256={v['name']:digest(root/'run'/f"{v['name']}.bin") for v in native['carriers']},
                limits=['Prescribed evolving accepted 64-cubed flow; fine normal velocities are linear and tangential values are piecewise constant within each coarse cell.',
                        'Flow samples held for 5 ms; halving heat steps does not refine the driver flow history.',
                        'Two scalar timesteps give a sensitivity check, not a formal time-convergence order.',
                        'Frozen earlier 8-cubed Fire sources; no calibrated combustion, gas-to-surface exchange, burning objects or live fine-grid feedback.'])
