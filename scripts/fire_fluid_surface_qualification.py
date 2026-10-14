#!/usr/bin/env python3
"""Measure local heat and passive-boundary readiness from retained native fields."""
import argparse,hashlib,json,math
from pathlib import Path
from coupling.accuracy_report import load
from coupling.surface_observations import observe,relative_l1
from coupling.passive_surface import advance,require_burning_admission
from coupling.convergence import restrict


def response_qualification():
    parameters=dict(area_m2=.25**2,capacity_j_k=100.,h_w_m2_k=10.,cooling_w_k=.2)
    initial=dict(temperature_k=300.,time_s=0.,stored_j=0.)
    state=initial;receipts=[];saved=None
    for q in range(32):
        state,r=advance(state,310.,.1,**parameters);receipts.append(r)
        if q==15:saved=json.loads(json.dumps(state))
    replay=saved;replayed=[]
    for q in range(16):
        replay,r=advance(replay,310.,.1,**parameters);replayed.append(r)
    g=.0625*10.;total=g+.2;equilibrium=(g*310.+.2*300.)/total
    expected=equilibrium+(300.-equilibrium)*math.exp(-total*3.2/100.)
    error=abs(state['temperature_k']-expected)
    residual=max(abs(r['residual_j']) for r in receipts)
    passed=error<1e-10 and residual<1e-10 and replay==state and replayed==receipts[16:]
    return dict(passed=passed,synthetic_parameters_si=parameters,gas_temperature_k=310.,
                final=state,analytic_temperature_k=expected,temperature_error_k=error,
                max_energy_residual_j=residual,exact_recovery=replay==state and replayed==receipts[16:],
                material_calibration=False,reciprocal_gas_debit_applied=False)


def report(root,prior):
    observations={};inputs=[];comparisons={};local_detail={}
    source_sha=hashlib.sha256((prior/'matched-source-frames.json').read_bytes()).hexdigest()
    for label in ('no-box','box'):
        old=prior/('matched-'+label+'-16/fluid.json')
        f16=json.loads(old.read_text())['fields']
        selected=[('16',f16['energy_j'],old)]
        for n in (32,64):
            name=f'box-{n}-csr' if label=='box' else f'no-box-{n}-dt005'
            native,f=load(root,name,n)
            if native['source_frames_sha256']!=source_sha or native['dt_s']!=.005 or native['box']!=(label=='box'):
                raise ValueError('matched source/time-step/box identity required')
            path=root/name/'fields.bin'
            if 'fields_sha256' in native and hashlib.sha256(path.read_bytes()).hexdigest()!=native['fields_sha256']:
                raise ValueError('native field digest mismatch')
            selected.append((str(n),f['energy_j'],path))
        observations[label]={n:observe(e,int(n)) for n,e,_ in selected}
        fine=restrict(selected[2][1],[64]*3,[32]*3)
        indices=[(k*32+j)*32+i for k in range(12) for j in range(8,24) for i in range(8,24)]
        local_detail[label]=relative_l1([selected[1][1][q] for q in indices],[fine[q] for q in indices])
        inputs.extend(dict(path=str(p.resolve()),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for _,_,p in selected)
        pairs=[]
        for a,b in (('16','32'),('32','64')):
            oa=observations[label][a];ob=observations[label][b]
            fa=[v['diagnostic_robin_flux_w_m2'] for v in oa['faces'].values()]
            fb=[v['diagnostic_robin_flux_w_m2'] for v in ob['faces'].values()]
            pairs.append(dict(grids=[int(a),int(b)],local_heat_profile_relative_l1=relative_l1(
                oa['source_footprint_temperature_rise_profile_k'],ob['source_footprint_temperature_rise_profile_k']),
                diagnostic_flux_relative_l1=relative_l1(fa,fb),
                diagnostic_flux_max_absolute_difference_w_m2=max(abs(x-y) for x,y in zip(fa,fb)),
                fine_max_diagnostic_flux_w_m2=max(abs(v) for v in fb)))
        comparisons[label]=pairs
    def passes(key):
        return all(p[1][key] is not None and p[1][key]<.1 and p[0][key] is not None and p[1][key]<p[0][key] for p in comparisons.values())
    temporal=[]
    time_obs=[]
    for name in ('no-box-32-dt005','no-box-32-dt0025','no-box-32-dt00125'):
        _,f=load(root,name,32);time_obs.append(observe(f['energy_j'],32))
        p=root/name/'fields.bin';inputs.append(dict(path=str(p.resolve()),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    for a,b in zip(time_obs,time_obs[1:]):
        temporal.append(dict(profile_relative_l1=relative_l1(a['source_footprint_temperature_rise_profile_k'],b['source_footprint_temperature_rise_profile_k']),
            diagnostic_flux_relative_l1=relative_l1([v['diagnostic_robin_flux_w_m2'] for v in a['faces'].values()],[v['diagnostic_robin_flux_w_m2'] for v in b['faces'].values()])))
    response=response_qualification()
    result=dict(schema='fire_fluid_passive_surface_qualification/v1',time_s=3.2,source_frames_sha256=source_sha,
        height_profile_converged=passes('local_heat_profile_relative_l1'),
        local_heat_converged=passes('local_heat_profile_relative_l1') and all(v<.1 for v in local_detail.values()),
        source_region_native_32_partition_heat_relative_l1=local_detail,
        no_box_time_refinement=temporal,
        diagnostic_flux_converged=passes('diagnostic_flux_relative_l1'),
        gas_surface_flux_converged=False,surface_response_numerics_qualified=response['passed'],
        reciprocal_exchange_qualified=False,material_ignition_qualified=False,
        threshold_relative_l1=.1,grid_comparisons=comparisons,observations=observations,
        inputs=inputs,surface_response=response,
        flux_policy='prescribed h=10 W/m2/K, Ts=300 K, fixed offset 1/16 m; diagnostic only',
        actual_box_thermal_policy='insulated; zero native gas-to-box transfer',
        limits='Final-time spatial observations only; no flux exchange-window or transient convergence, physical coefficient calibration, radiation, ignition or reciprocal adapter.')
    try:require_burning_admission(result)
    except ValueError as exc:result.update(burning_faces_enabled=False,admission_refusal=str(exc))
    else:raise ValueError('unexpected burning admission')
    result['status']='passive_numerics_passed_coupled_surface_unqualified' if response['passed'] else 'surface_numerics_failed'
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('accuracy-root','prior-root','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=False)
    result=report(a.accuracy_root,a.prior_root)
    (a.output/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('inputs','observations')},indent=2))
