"""Corrected matched-velocity attribution: original source and fixed scalar time."""
import json,math
from pathlib import Path
import numpy as np
from .matched_heat_report import digest,compare,indices
from .flow_time_report import SOURCE
from .flow_spatial_fields import restrict_extensive
from .flow_spatial_report import load,validate as validate_coupled

def validate(native):
    if (native['schema']!='matched_original_source_v1' or native['velocity_driver_grid']!=64
        or native['flow_dt_s']!=.005 or native['time_s']!=3.2 or native['driver_scalar_cycles']!=2560
        or not native['control_bit_identical_each_step'] or native['control_max_difference_j']!=0
        or not 0<=native['driver_max_divergence']<1e-8 or not 0<=native['fine_max_divergence']<1e-8
        or not 0<=native['driver_max_pressure_residual']<1e-11 or native['memory_limit_bytes']!=2147483648
        or not 0<native['memory_peak_bytes']<=2147483648 or native['memory_live_bytes'] or native['rejected_allocations']):raise ValueError('matched original-source control/flow/resource identity')
    rows=native['carriers']
    if [(v['grid'],v['scalar_dt_s'],v['scalar_cycles']) for v in rows]!=[(64,.00125,2560),(128,.00125,2560)]:raise ValueError('carrier fixed timing')
    for v in rows:
        if not abs(v['heat_balance_j'])<1e-9 or not 0<=v['maximum_courant_bound']<=.4 or not 300<=v['peak_temperature_k']<=330:raise ValueError('carrier heat balance/CFL/contrast')
        for key in ('energy_j','input_j','inflow_j','outflow_j'):
            if not math.isfinite(v[key]) or v[key]<0:raise ValueError('carrier finite heat accounting')
    if abs(rows[0]['input_j']-rows[1]['input_j'])>=1e-9:raise ValueError('matched integrated heat source')

def report(root,coupled):
    root,coupled=map(Path,(root,coupled));run=root/'run';native=json.loads((run/'native.json').read_text());validate(native)
    identity=json.loads((root/'identity.json').read_text());pair_identity=json.loads((coupled/'run-identity.json').read_text())
    if (json.loads((root/'terminal.json').read_text())['returncode']!=0 or digest(root/'qualified-probe')!=identity['probe_sha256']
        or identity['source_sha256']!=SOURCE or digest(identity['source'])!=SOURCE
        or identity['fine_source_sha256']!=pair_identity['fine_source_sha256'] or digest(identity['fine_source'])!=identity['fine_source_sha256']):raise ValueError('original source/probe terminal identity')
    if json.loads((coupled/'terminal.json').read_text())['returncode']!=0:raise ValueError('coupled pair incomplete')
    prior=json.loads((coupled/'grid-64/summary.json').read_text());validate_coupled(prior,64)
    if digest(coupled/'grid-64/fields.bin')!=prior['fields_sha256']:raise ValueError('coarse coupled digest')
    driver=(run/'driver-fields.bin').read_bytes()+(run/'driver-velocity.bin').read_bytes()
    import hashlib
    if hashlib.sha256(driver).hexdigest()!=prior['fields_sha256']:raise ValueError('accepted fixed-scalar final driver differs')
    heat=[];hashes={}
    for row in native['carriers']:
        p=run/(row['name']+'.bin');v=np.fromfile(p,dtype=np.float64)
        if len(v)!=row['grid']**3 or not np.isfinite(v).all() or (v<0).any() or abs(math.fsum(v)-row['energy_j'])>=1e-9:raise ValueError('finite native carrier/total')
        heat.append(v);hashes[row['name']]=digest(p)
    if heat[0].tobytes()!=driver[:64**3*8]:raise ValueError('final heat control parity')
    matched=restrict_extensive(heat[1],64);spatial=compare(heat[0],matched,64)
    rise=native['carriers'][1]['peak_temperature_k']-300
    spatial['peak_temperature_rise_relative_difference']=abs(native['carriers'][0]['peak_temperature_k']-native['carriers'][1]['peak_temperature_k'])/rise if rise else None
    fine_summary=json.loads((coupled/'grid-128/summary.json').read_text());validate_coupled(fine_summary,128)
    if digest(coupled/'grid-128/fields.bin')!=fine_summary['fields_sha256']:raise ValueError('coupled fine digest')
    actual=restrict_extensive(load(coupled/'grid-128/fields.bin',128)['energy_j'],64)
    q=indices(64);transport=matched[q]-heat[0][q];flow=actual[q]-matched[q];total=actual[q]-heat[0][q]
    decomposition=dict(matched_transport_difference_j=math.fsum(abs(v) for v in transport),
                       flow_representation_and_evolution_difference_j=math.fsum(abs(v) for v in flow),
                       full_coupled_difference_j=math.fsum(abs(v) for v in total),
                       identity_max_residual_j=float(np.max(np.abs(transport+flow-total))),
                       limitation='The residual combines coarse-flow prolongation representation and independent fine-flow response. Source mapping and scalar timing are now identical. These L1 norms are not additive percentage shares.')
    return dict(schema='matched_original_source_verdict_v1',status='matched_original_heat_gate_passed' if spatial['local_heat_gate_passed'] else 'matched_original_heat_gate_failed',
                native=native,identity=identity,spatial=spatial,decomposition=decomposition,field_sha256=hashes,
                source_and_scalar_confound_removed=True,
                historical_limit='Prior 6.228 percent matched heat result used parent/8 fine source and 2.5 ms heat steps; it does not alone qualify the original-source fixed-timing experiment.',
                limits=['Fine heat follows flux-preserving prolongation of the accepted evolving 64 flow, not an independently solved fine flow.',
                        'Frozen Fire source and no-box plume; not calibrated surface heat exchange or physical prediction.'])
