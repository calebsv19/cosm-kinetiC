"""One fixed-timing spatial comparison under prescribed coarse thermal forcing."""
import json, math
from pathlib import Path
import numpy as np
from .flow_time_report import SOURCE, pair, relative_l2
from .flow_spatial_report import validate as validate_spatial, load
from .flow_spatial_fields import restrict_extensive, restrict_faces, pack, localize
from .matched_heat_report import digest, indices
from .isolated_qualification import PROBES
from .velocity import sample_mac

FINE_SOURCE = '6db4841abd9c7418d56f100203d2727aca6ac8fdd8384feab7392dddb2172f30'
FORCING = 'coarse_old_time_piecewise_constant_temperature'

def validate(s):
    if (s['schema'] != 'prescribed_thermal_flow_native_v1' or s['forcing'] != FORCING
        or s['forcing_steps'] != 640 or s['forcing_time_offset_s'] != 0
        or s['forcing_sample_time_s'] != 3.195 or s['memory_limit_bytes'] != 2147483648
        or not 0 < s['memory_peak_bytes'] <= 2147483648
        or s['memory_live_bytes'] != 0 or s['rejected_allocations'] != 0):
        raise ValueError('prescribed forcing timing/resource identity')
    if [v['grid'] for v in s['runs']] != [64,128]:
        raise ValueError('ordered independently evolving grids')
    for row in s['runs']:
        if row['qualification_profile'] != 'prescribed_thermal_flow_v1':
            raise ValueError('prescribed profile required')
        original = dict(row, qualification_profile='flow_spatial_fixed_scalar_v1')
        validate_spatial(original, row['grid'])

def forcing_check(coarse, fine, n=64):
    coarse, fine = np.asarray(coarse), np.asarray(fine)
    if (coarse.size != n**3 or fine.size != (2*n)**3
        or not np.isfinite(coarse).all() or not np.isfinite(fine).all()
        or (coarse < 0).any() or (fine < 0).any()):
        raise ValueError('finite nonnegative forcing samples')
    expected = np.repeat(np.repeat(np.repeat(coarse.reshape(n,n,n),2,axis=0),2,axis=1),2,axis=2).ravel()/8
    if not np.array_equal(expected, fine):
        raise ValueError('fine forcing is not old coarse cell-average temperature')
    restricted = restrict_extensive(fine,n)
    scale = max(float(np.max(coarse)),1e-300)
    error = float(np.max(np.abs(restricted-coarse)))/scale
    if error > 2e-15:
        raise ValueError('forcing extensive restriction closure')
    return dict(child_values_bit_identical=True, restriction_max_relative=error,
                coarse_total_j=math.fsum(coarse), fine_total_j=math.fsum(fine),
                limitation='Prescribed thermal samples are momentum inputs, not heat-accounting additions.')

def report(root):
    root = Path(root); identity = json.loads((root/'identity.json').read_text())
    if (json.loads((root/'terminal.json').read_text())['returncode'] != 0
        or identity['source_sha256'] != SOURCE or digest(identity['source']) != SOURCE
        or identity['fine_source_sha256'] != FINE_SOURCE or digest(identity['fine_source']) != FINE_SOURCE
        or identity['forcing'] != FORCING or identity['flow_dt_s'] != .005
        or identity['scalar_dt_s'] != .00125 or identity['grids'] != [64,128]
        or identity['source_grid'] != 64 or identity['windows'] != 32 or identity['window_s'] != .1
        or digest(root/'qualified-probe') != identity['probe_sha256']):
        raise ValueError('terminal original-source/executable identity')
    native = json.loads((root/'summary.json').read_text()); validate(native)
    if native['probe_sha256'] != identity['probe_sha256']:
        raise ValueError('summary executable identity')
    arrays=[]; samples=[]
    for n,row in zip((64,128),native['runs']):
        p=root/f'fields-{n}.bin';f=root/f'forcing-sample-{n}.bin'
        if digest(p) != native['fields_sha256'][str(n)] or digest(f) != native['forcing_sha256'][str(n)]:
            raise ValueError('final field/forcing hashes')
        a=load(p,n); samples.append(np.fromfile(f,dtype=np.float64)); arrays.append(a)
        if abs(math.fsum(a['energy_j'])-row['energy_j'])>=1e-9 or abs(math.fsum(a['smoke_kg'])-row['smoke_kg'])>=1e-15:
            raise ValueError('independent native totals readback')
    forcing=forcing_check(*samples)
    prior=Path(identity['prior_pair']); old_identity=json.loads((prior/'run-identity.json').read_text())
    if (json.loads((prior/'terminal.json').read_text())['returncode'] != 0
        or old_identity['source_sha256'] != SOURCE or old_identity['fine_source_sha256'] != FINE_SOURCE):
        raise ValueError('prior paired source/terminal identity')
    old=[]
    for n in (64,128):
        s=json.loads((prior/f'grid-{n}/summary.json').read_text()); validate_spatial(s,n)
        if digest(prior/f'grid-{n}/fields.bin')!=s['fields_sha256']:
            raise ValueError('prior paired fields drift')
        old.append(load(prior/f'grid-{n}/fields.bin',n))
    if digest(root/'fields-64.bin') != digest(prior/'grid-64/fields.bin'):
        raise ValueError('coarse accepted driver changed')
    restricted={k:restrict_extensive(arrays[1][k],64) for k in ('energy_j','smoke_kg')}
    restricted['face_velocity_m_s']=pack(restrict_faces(arrays[1]['face_velocity_m_s'],64))
    comparison=pair(arrays[0],restricted,*native['runs']); comparison.pop('temporal_screens_passed');comparison.pop('flow_dt_s')
    winds=[{name:sample_mac(a['face_velocity_m_s'],[n]*3,[1.]*3,p,solid_floor=True,lateral_open=True)
            for name,p in PROBES.items()} for a,n in zip(arrays,(64,128))]
    wind=relative_l2([v for name in PROBES for v in winds[0][name]], [v for name in PROBES for v in winds[1][name]])
    comparison['wind_probes_m_s']=winds;comparison['wind_probe_norm']=wind
    screens=[comparison['heat']['local_heat_relative_l1'],comparison['heat']['height_profile_relative_l1'],
             comparison['full_mac_velocity']['relative_l2'],wind['relative_l2'],comparison['full_smoke_relative_l1'],comparison['peak_rise_relative_difference']]
    all_pass=all(v is not None and math.isfinite(v) and v<=.1 for v in screens)
    matched=Path(identity['prior_matched'])/'run/heat-128.bin'
    # Corrected matched-flow source/timing proof is retained and digest-bound.
    mreport=json.loads((prior.parent/'matched-original-verdict.json').read_text())
    if (digest(matched)!=mreport['field_sha256']['heat-128'] or not mreport['source_and_scalar_confound_removed']
        or mreport['identity']['source_sha256']!=SOURCE or mreport['identity']['fine_source_sha256']!=FINE_SOURCE):
        raise ValueError('prior corrected matched heat identity')
    mh=np.fromfile(matched,dtype=np.float64)
    if len(mh)!=128**3 or not np.isfinite(mh).all() or (mh<0).any():raise ValueError('matched heat carrier')
    q=indices(64);c=arrays[0]['energy_j'][q];m=restrict_extensive(mh,64)[q]
    f=restricted['energy_j'][q];actual=restrict_extensive(old[1]['energy_j'],64)[q]
    parts=[m-c,f-m,actual-f];total=actual-c
    decomposition=dict(matched_transport_difference_j=math.fsum(abs(x) for x in parts[0]),
                       prescribed_momentum_response_difference_j=math.fsum(abs(x) for x in parts[1]),
                       thermal_input_choice_difference_j=math.fsum(abs(x) for x in parts[2]),
                       full_coupled_difference_j=math.fsum(abs(x) for x in total),
                       identity_max_residual_j=float(np.max(np.abs(sum(parts)-total))),
                       limitation='Signed fields add; L1 norms are not additive shares. Thermal-input choice includes coarse forcing representation, not only nonlinear feedback.')
    fine_choice=relative_l2(arrays[1]['face_velocity_m_s'],old[1]['face_velocity_m_s'])
    return dict(schema='prescribed_thermal_flow_verdict_v1',status='prescribed_spatial_screens_passed' if all_pass else 'prescribed_spatial_screens_failed',
                local_heat_gate_passed=comparison['heat']['local_heat_gate_passed'],spatial_screens_passed=all_pass,heat_threshold_relative=.1,
                domain_m=[1,1,1],heat_region_m=[[.25,.75],[.25,.75],[0,.375]],source_region_m=[[.25,.75],[.25,.75],[0,.125]],
                identity=identity,native=native,forcing_readback=forcing,coarse_control_bit_identical=True,comparison=comparison,
                velocity_localization=localize(arrays[0]['face_velocity_m_s'],arrays[1]['face_velocity_m_s'],64),
                native_fine_velocity_thermal_input_choice=fine_choice,decomposition=decomposition,
                limitations=['Fine momentum evolves independently under coarse old-time, piecewise-constant thermal forcing; fine heat/smoke are independently transported with original sources.',
                            'The diagnostic is externally forced, not a replacement fully coupled solver. Coarse thermal representation remains in the comparison.',
                            'Final-state spatial sensitivity, not calibrated prediction error or formal convergence order; fine flow timestep sensitivity remains unqualified.',
                            'No gas-to-surface exchange, material response or burning faces enabled.'])
