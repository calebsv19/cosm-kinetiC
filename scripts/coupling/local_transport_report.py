"""Fixed-source candidate comparisons, without widening an accuracy gate."""
import hashlib,json,math
from pathlib import Path
from fire_fluid_accuracy import fields
from .convergence import comparison,restrict
from .surface_observations import observe,relative_l1
from .accuracy_report import probe_checks


def read(root,name,n,dt,box):
    directory=Path(root)/name;summary=json.loads((directory/'summary.json').read_text())
    path=directory/'fields.bin';sha=hashlib.sha256(path.read_bytes()).hexdigest()
    if summary['grid']!=n or summary['dt_s']!=dt or summary['time_s']!=3.2 or summary['box']!=box:
        raise ValueError('candidate run identity')
    if summary.get('fields_sha256',sha)!=sha:raise ValueError('field digest mismatch')
    if abs(summary['heat_balance_j'])>=1e-9 or abs(summary['smoke_balance_kg'])>=1e-15 or summary['divergence']>=1e-8 or summary['pressure_residual']>=1e-11 or summary['peak_temperature_k']>330:
        raise ValueError('native invariants')
    f=fields(path,n)
    if not all(math.isfinite(v) for values in f.values() for v in values) or any(v<0 for key in ('energy_j','smoke_kg') for v in f[key]):raise ValueError('finite nonnegative fields')
    if len(f['energy_j'])!=n**3 or len(f['face_velocity_m_s'])!=3*n**3+3*n*n:raise ValueError('field dimensions')
    return summary,f,dict(path=str(path.resolve()),sha256=sha)


def region_difference(a,b,n):
    indices=[(k*n+j)*n+i for k in range(3*n//8) for j in range(n//4,3*n//4) for i in range(n//4,3*n//4)]
    return relative_l1([a[q] for q in indices],[b[q] for q in indices])


def pair(a,b,na,nb):
    oa=observe(a['energy_j'],na);ob=observe(b['energy_j'],nb)
    fa=[v['diagnostic_robin_flux_w_m2'] for v in oa['faces'].values()]
    fb=[v['diagnostic_robin_flux_w_m2'] for v in ob['faces'].values()]
    return dict(grids=[na,nb],full_fields=comparison(a,b,[na]*3,[nb]*3),
        source_region_heat_relative_l1=region_difference(a['energy_j'],restrict(b['energy_j'],[nb]*3,[na]*3),na),
        height_profile_relative_l1=relative_l1(oa['source_footprint_temperature_rise_profile_k'],ob['source_footprint_temperature_rise_profile_k']),
        diagnostic_flux_relative_l1=relative_l1(fa,fb),
        diagnostic_flux_max_absolute_difference_w_m2=max(abs(x-y) for x,y in zip(fa,fb)),
        coarse_peak_diagnostic_flux_w_m2=max(fa),fine_peak_diagnostic_flux_w_m2=max(fb))


def report(root,baseline):
    inputs=[];cases={};source_sha=None
    for label,box in (('no-box',False),('box',True)):
        current=[];old=[];checks={}
        for n in (32,64):
            s,f,i=read(root,f'{label}-{n}-mc',n,.005,box)
            if s['transport_scheme']!='muscl_mc_scalar':raise ValueError('candidate scalar scheme')
            if source_sha is not None and source_sha!=s['source_frames_sha256']:raise ValueError('matched source identity')
            source_sha=s['source_frames_sha256'];inputs.append(i);current.append(f)
            old_name=f'box-{n}-csr' if box else f'no-box-{n}-dt005'
            old_s,old_f,old_i=read(baseline,old_name,n,.005,box)
            if old_s['source_frames_sha256']!=source_sha:raise ValueError('baseline source mismatch')
            old.append(old_f);inputs.append(old_i);checks[str(n)]=dict(native=s,**probe_checks(f,n,box))
        cases[label]=dict(baseline_minmod=pair(*old,32,64),candidate_mc=pair(*current,32,64),native_checks=checks)
    temporal=[];time_fields=[]
    for name,dt in (('no-box-32-mc',.005),('no-box-32-mc-dt0025',.0025),('no-box-32-mc-dt00125',.00125)):
        s,f,i=read(root,name,32,dt,False)
        if s['source_frames_sha256']!=source_sha or s['transport_scheme']!='muscl_mc_scalar':raise ValueError('time-refinement identity')
        inputs.append(i);time_fields.append(f)
    for a,b in zip(time_fields,time_fields[1:]):temporal.append(pair(a,b,32,32))
    time_pass=all(p['full_fields'][key]['relative_l1']<.01 for p in temporal for key in ('energy_j','smoke_kg')) and all(temporal[1]['full_fields'][key]['relative_l1']<.6*temporal[0]['full_fields'][key]['relative_l1'] for key in ('energy_j','smoke_kg'))
    s16,f16,i16=read(root,'no-box-16-mc',16,.005,False)
    if s16['source_frames_sha256']!=source_sha:raise ValueError('coarse source identity')
    inputs.append(i16)
    _,f32,_=read(root,'no-box-32-mc',32,.005,False)
    coarse_pair=pair(f16,f32,16,32)
    local_pass=time_pass and all(case['candidate_mc']['source_region_heat_relative_l1']<.1 for case in cases.values())
    return dict(schema='fire_fluid_local_transport_repair/v1',status='local_heat_gate_passed' if local_pass else 'local_heat_gate_failed',
        source_frames_sha256=source_sha,threshold_relative_l1=.1,local_heat_converged=local_pass,
        temporal_gate_passed=time_pass,time_refinement=temporal,no_box_coarse_grid_pair=coarse_pair,
        scalar_scheme='MC scalar spatial slopes, Euler time, minmod momentum',cases=cases,inputs=inputs,
        burning_faces_enabled=False,reciprocal_thermal_exchange=False,
        limits='Two spatial grids establish a measured difference, not an asymptotic order or physical calibration. Frozen Fire amounts from the original live 8-cubed control; no live 64-cubed feedback.')
