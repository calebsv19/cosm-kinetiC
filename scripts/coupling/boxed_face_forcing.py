"""Fixed physical face-support qualification, independent of native sampling."""
import hashlib
import json
import math
from pathlib import Path
import numpy as np

DEPTH=1/32
AREA=1/16
RHO_CP=1206.
THRESHOLD=.1


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def relative(a,b):
    a=np.asarray(a);b=np.asarray(b)
    if a.shape!=b.shape or not np.isfinite(a).all() or not np.isfinite(b).all():
        raise ValueError('matched finite observables required')
    denominator=float(np.abs(b).sum())
    return float(np.abs(a-b).sum())/denominator if denominator>0 else None


def patches(energy,n):
    """Conservative common 8x8 face tiles, each extruded over fixed 1/32 m."""
    if n not in (32,64):raise ValueError('qualified grid is 32 or 64')
    r=n//32
    e=np.asarray(energy).reshape(n,n,n)
    slab=e[11*r:12*r,12*r:20*r,12*r:20*r]
    return slab.reshape(r,8,r,8,r).sum(axis=(0,2,4)).ravel()/(RHO_CP/32**3)


def response(rows, cadence):
    """Old-time held prescribed forcing; diagnostic demand, no gas debit."""
    base=3.2/(len(rows)-1) if len(rows)>1 else 0
    stride=round(cadence/base) if base else 0
    if (cadence not in (.005,.01,.0125,.025,.05,.1) or stride<1
        or abs(stride*base-cadence)>1e-12 or abs((len(rows)-1)/stride-round((len(rows)-1)/stride))>1e-9):
        raise ValueError('bounded matched history/cadence required')
    rise=0.;transfer=0.;trace=[0.]
    for i in range(0,len(rows)-1,stride):
        gas=float(rows[i]['support_mean_rise_k'])
        delta=(gas-rise)*(-math.expm1(-10*AREA*cadence/100))
        rise+=delta;transfer+=100*delta;trace.append(rise)
    return dict(surface_rise_k=rise,gas_transfer_demand_j=transfer,
                surface_rise_history_k=trace,reciprocal_debit_applied=False)


def load(root,n):
    r=Path(root)/f'box-{n}'
    terminal=json.loads((r/'terminal.json').read_text())
    identity=json.loads((r/'identity.json').read_text())
    if (terminal['returncode']!=0 or identity['grid']!=n or identity['flow_dt_s']!=.0025
        or identity['scalar_dt_s']!=.00125 or identity['duration_s']!=3.2
        or identity['support_lower_m']!=[.375,.375,.34375]
        or identity['support_upper_m']!=[.625,.625,.375] or identity['heat_gate_relative']!=THRESHOLD):
        raise ValueError('run identity/terminal mismatch')
    if sha(Path(identity['command'][2]))!=identity['source_sha256'] or sha(Path(identity['command'][0]))!=identity['probe_sha256']:
        raise ValueError('source/probe hash mismatch')
    path=r/'fields.bin'
    if sha(path)!=terminal['fields_sha256']:raise ValueError('field hash mismatch')
    fields=np.fromfile(path,dtype='f8')
    if fields.size!=5*n**3+3*n*n or not np.isfinite(fields).all() or (fields[:2*n**3]<0).any():
        raise ValueError('native field carrier mismatch')
    lines=[json.loads(line) for line in (r/'trace.jsonl').read_text().splitlines()]
    native=lines.pop()
    reconstruction=identity.get('scalar_reconstruction','minmod')
    if reconstruction not in ('minmod','mc') or native.get('scalar_mc',False)!=(reconstruction=='mc'):
        raise ValueError('scalar reconstruction identity mismatch')
    if (not native.get('terminal') or native['grid']!=n or native['time_s']!=3.2
        or native['flow_dt_s']!=.0025 or native['scalar_dt_s']!=.00125
        or native['flow_steps']!=1280 or native['scalar_work_cells']!=2560*n**3
        or native['memory_live_bytes'] or native['rejected_allocations']
        or not 0<native['memory_peak_bytes']<=1073741824
        or not all(math.isfinite(native[k]) for k in ('heat_balance_j','maximum_final_divergence','final_pressure_residual'))
        or abs(native['heat_balance_j'])>1e-9 or native['maximum_final_divergence']>=1e-8
        or native['final_pressure_residual']>=1e-11):
        raise ValueError('native numerical closure mismatch')
    interval=native.get('observation_dt_s',.025)
    if interval not in (.005,.0125,.025) or len(lines)!=round(3.2/interval)+1:
        raise ValueError('complete declared observation history required')
    for i,row in enumerate(lines):
        patch=np.asarray(row['patch_rise_k'])
        if (abs(row['time_s']-i*interval)>1e-12 or patch.shape!=(64,)
            or not math.isfinite(row['time_s']) or not math.isfinite(row['support_mean_rise_k'])
            or not np.isfinite(patch).all() or (patch<0).any()
            or abs(float(patch.mean())-row['support_mean_rise_k'])>1e-12):
            raise ValueError('observation history mismatch')
    expected=patches(fields[:n**3],n)
    if np.count_nonzero(lines[0]['patch_rise_k']):raise ValueError('from-zero initial support required')
    solid=(slice(3*n//8,5*n//8),)*3
    if np.count_nonzero(fields[:n**3].reshape(n,n,n)[solid]):raise ValueError('solid heat must be zero')
    if not np.allclose(expected,lines[-1]['patch_rise_k'],rtol=1e-12,atol=1e-18):
        raise ValueError('independent final sampling differs')
    return dict(identity=identity,native=native,terminal=terminal,rows=lines,energy=fields[:n**3])


def report(root):
    cases=[load(root,n) for n in (32,64)]
    if cases[0]['identity'].get('scalar_reconstruction','minmod')!=cases[1]['identity'].get('scalar_reconstruction','minmod'):
        raise ValueError('spatial qualification requires the same scalar operator')
    histories=[np.asarray([r['patch_rise_k'] for r in c['rows']]) for c in cases]
    energy=[cases[0]['energy'].reshape(32,32,32),
            cases[1]['energy'].reshape(32,2,32,2,32,2).sum(axis=(1,3,5))]
    roi=(slice(0,12),slice(8,24),slice(8,24))
    heat=relative(energy[0][roi],energy[1][roi])
    # Use nonzero thermal excess/flux, never absolute ~300 K normalization.
    spatial=relative(histories[0][1:],histories[1][1:])
    final=relative(histories[0][-1],histories[1][-1])
    responses={str(n):{str(dt):response(c['rows'],dt) for dt in (.1,.05,.025)} for n,c in zip((32,64),cases)}
    cadence={str(n):[relative([responses[str(n)][str(a)]['gas_transfer_demand_j']],
                              [responses[str(n)][str(b)]['gas_transfer_demand_j']])
                        for a,b in ((.1,.05),(.05,.025))] for n in (32,64)}
    transfer=relative([responses['32']['0.025']['gas_transfer_demand_j']],
                      [responses['64']['0.025']['gas_transfer_demand_j']])
    gates=dict(local_heat=heat is not None and heat<=THRESHOLD,
        face_history=spatial is not None and spatial<=THRESHOLD,
        face_final=final is not None and final<=THRESHOLD,
        transfer_demand=transfer is not None and transfer<=THRESHOLD,
        exchange_cadence=all(v is not None and v<=THRESHOLD for values in cadence.values() for v in values))
    return dict(schema='fire_fluid_fixed_box_face_forcing/v1',threshold_relative=THRESHOLD,
        support=dict(lower_m=[.375,.375,.34375],upper_m=[.625,.625,.375],area_m2=AREA,
                     depth_m=DEPTH,volume_m3=AREA*DEPTH,gas_capacity_j_k=RHO_CP*AREA*DEPTH),
        local_heat_relative_l1=heat,face_history_relative_l1=spatial,
        face_final_relative_l1=final,transfer_demand_relative=transfer,
        exchange_cadence_relative=cadence,gates=gates,forcing_qualified=all(gates.values()),
        responses=responses,native=[c['native'] for c in cases],
        wall_s=[c['terminal']['wall_s'] for c in cases],
        final_mean_rise_k=[c['rows'][-1]['support_mean_rise_k'] for c in cases],
        limits=['Synthetic uniform-volume Robin support, not calibrated wall convection.',
                'Insulated fluid trajectories; prescribed surface-response diagnostic has no gas debit.',
                'Reciprocal frozen-field exchange is verified separately on exactly this support.',
                'No absolute-temperature normalization, lowered heat gate or negligible-signal waiver.',
                'No ignition, material pyrolysis, live coupled qualification or large-grid convergence claim.'])
