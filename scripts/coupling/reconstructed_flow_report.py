"""Strict thermal-transfer sensitivity and separate original fine-flow timing."""
import json,math
from pathlib import Path
import numpy as np
from .prescribed_flow_report import SOURCE,FINE_SOURCE,validate as validate_pc
from .flow_spatial_report import load,validate as validate_spatial
from .flow_spatial_fields import restrict_extensive,restrict_faces,pack,localize
from .flow_time_report import pair
from .matched_heat_report import digest
FORCING='coarse_old_time_conservative_mc_temperature'

def reconstruction(coarse,n):
    c=np.asarray(coarse).reshape(n,n,n)
    if not np.isfinite(c).all() or (c<0).any():raise ValueError('forcing admission')
    lo=c.copy();hi=c.copy();slopes=[]
    for axis in (2,1,0):
        d=np.zeros_like(c);mid=[slice(None)]*3;mid[axis]=slice(1,-1)
        lower=mid.copy();lower[axis]=slice(None,-2)
        upper=mid.copy();upper[axis]=slice(2,None)
        a=c[tuple(mid)]-c[tuple(lower)];b=c[tuple(upper)]-c[tuple(mid)]
        d[tuple(mid)]=np.where(a*b>0,np.sign(a)*np.minimum(.5*np.abs(a+b),np.minimum(2*np.abs(a),2*np.abs(b))),0)
        for shift in (-1,1):
            dest=[slice(None)]*3;src=dest.copy()
            dest[axis]=slice(1,None) if shift<0 else slice(None,-1)
            src[axis]=slice(None,-1) if shift<0 else slice(1,None)
            lo[tuple(dest)]=np.minimum(lo[tuple(dest)],c[tuple(src)])
            hi[tuple(dest)]=np.maximum(hi[tuple(dest)],c[tuple(src)])
        slopes.append(d)
    excursion=.25*(np.abs(slopes[0])+np.abs(slopes[1])+np.abs(slopes[2]))
    limit=np.ones_like(c);np.divide(np.minimum(c-lo,hi-c),excursion,out=limit,where=excursion>0);limit=np.minimum(limit,1)
    out=np.empty((2*n,)*3)
    for z in range(2):
        for y in range(2):
            for x in range(2):
                delta=.25*limit*((2*x-1)*slopes[0]+(2*y-1)*slopes[1]+(2*z-1)*slopes[2])
                out[z::2,y::2,x::2]=np.maximum(lo,np.minimum(hi,c+delta))/8
    return out.ravel()

def forcing_check(c,f,n=64):
    c,f=map(np.asarray,(c,f))
    if c.size!=n**3 or f.size!=(2*n)**3 or not np.isfinite(f).all() or (f<0).any():raise ValueError('forcing sample dimensions/positivity')
    ref=reconstruction(c,n);scale=max(float(np.max(c)),1e-300)
    err=float(np.max(np.abs(f-ref)))/scale
    closure=float(np.max(np.abs(restrict_extensive(f,n)-c)))/scale
    if err>3e-15 or closure>3e-15:raise ValueError('reconstruction oracle/conservation')
    return dict(oracle_max_relative=err,restriction_max_relative=closure,coarse_total_j=math.fsum(c),fine_total_j=math.fsum(f))

def validate_fine(s,dt):
    if s['qualification_profile']!='flow_time_128_v1' or s['dt_s']!=dt or s['flow_steps']!=round(3.2/dt) or s['scalar_cycles']!=2560:raise ValueError('fine temporal identity')
    validate_spatial(dict(s,qualification_profile='flow_spatial_fixed_scalar_v1',dt_s=.005,flow_steps=640),128)

def validate_identity(root,reconstructed):
    root=Path(root);i=json.loads((root/'identity.json').read_text())
    if (json.loads((root/'terminal.json').read_text())['returncode']!=0
        or digest(i['source'])!=SOURCE or i['source_sha256']!=SOURCE
        or digest(i['fine_source'])!=FINE_SOURCE or i['fine_source_sha256']!=FINE_SOURCE
        or digest(i['command'][0])!=i['probe_sha256']
        or i['grids']!=([64,128] if reconstructed else [128])
        or i['scalar_dt_s']!=.00125 or i['source_grid']!=64 or i['windows']!=32 or i['window_s']!=.1
        or i['forcing']!=(FORCING if reconstructed else 'own_accepted_old_time_temperature')):raise ValueError('terminal/source/probe identity')
    s=json.loads((root/'summary.json').read_text())
    if s['probe_sha256']!=i['probe_sha256']:raise ValueError('summary probe identity')
    for p,h in s['fields_sha256'].items():
        if digest(root/p)!=h:raise ValueError('native field hash')
    return i,s

def totals(a,s):
    if abs(math.fsum(a['energy_j'])-s['energy_j'])>=1e-9 or abs(math.fsum(a['smoke_kg'])-s['smoke_kg'])>=1e-15:raise ValueError('native total readback')

def transfer_report(root):
    root=Path(root);i,s=validate_identity(root/'reconstructed',True)
    if s['forcing']!=FORCING:raise ValueError('reconstructed profile')
    validate_pc(dict(s,forcing='coarse_old_time_piecewise_constant_temperature'))
    arrays=[load(root/'reconstructed'/f'fields-{n}.bin',n) for n in (64,128)]
    for a,row in zip(arrays,s['runs']):totals(a,row)
    forcing=forcing_check(*(np.fromfile(root/'reconstructed'/f'forcing-sample-{n}.bin') for n in (64,128)))
    prior=root.parent/'fire-fluid-prescribed-flow-20261011/run';pc=json.loads((prior/'summary.json').read_text());validate_pc(pc)
    if json.loads((prior/'terminal.json').read_text())['returncode']!=0:raise ValueError('PC terminal')
    for n in (64,128):
        if digest(prior/f'fields-{n}.bin')!=pc['fields_sha256'][str(n)]:raise ValueError('PC field drift')
    if digest(root/'reconstructed/fields-64.bin')!=pc['fields_sha256']['64']:raise ValueError('accepted coarse control drift')
    restricted={k:restrict_extensive(arrays[1][k],64) for k in ('energy_j','smoke_kg')};restricted['face_velocity_m_s']=pack(restrict_faces(arrays[1]['face_velocity_m_s'],64))
    comparison=pair(arrays[0],restricted,*s['runs']);comparison.pop('temporal_screens_passed');comparison.pop('flow_dt_s')
    # Native 64/128 wind sampling is independent of restricted representation.
    from .isolated_qualification import PROBES
    from .velocity import sample_mac
    from .flow_time_report import relative_l2
    winds=[{name:sample_mac(a['face_velocity_m_s'],[n]*3,[1.]*3,p,solid_floor=True,lateral_open=True) for name,p in PROBES.items()} for a,n in zip(arrays,(64,128))]
    comparison['wind_probes_m_s']=winds;comparison['wind_probe_norm']=relative_l2([v for name in PROBES for v in winds[0][name]],[v for name in PROBES for v in winds[1][name]])
    sensitivity=pair(load(prior/'fields-128.bin',128),arrays[1],pc['runs'][1],s['runs'][1],n=128)
    sensitivity['sensitivity_screens_passed']=sensitivity.pop('temporal_screens_passed')
    own_root=Path(i['prior_pair'])/'grid-128';own_summary=json.loads((own_root/'summary.json').read_text());validate_spatial(own_summary,128)
    if digest(own_root/'fields.bin')!=own_summary['fields_sha256'] or json.loads((own_root/'terminal.json').read_text())['returncode']!=0:raise ValueError('own-thermal fine reference')
    fine_choice=relative_l2(arrays[1]['face_velocity_m_s'],load(own_root/'fields.bin',128)['face_velocity_m_s'])
    screens=[comparison['heat']['local_heat_relative_l1'],comparison['heat']['height_profile_relative_l1'],comparison['full_mac_velocity']['relative_l2'],comparison['wind_probe_norm']['relative_l2'],comparison['full_smoke_relative_l1'],comparison['peak_rise_relative_difference']]
    passed=all(v is not None and math.isfinite(v) and v<=.1 for v in screens)
    return dict(schema='reconstructed_thermal_transfer_verdict_v1',spatial_screens_passed=passed,local_heat_gate_passed=comparison['heat']['local_heat_gate_passed'],native_fine_velocity_thermal_input_choice=fine_choice,identity=i,runs=s['runs'],forcing=forcing,coarse_control_bit_identical=True,spatial_comparison=comparison,piecewise_constant_to_reconstructed_fine_sensitivity=sensitivity,velocity_localization=localize(arrays[0]['face_velocity_m_s'],arrays[1]['face_velocity_m_s'],64),limitations=['Externally forced diagnostic; original independently coupled spatial gate remains separate.','Conservative transfer is not a heat source; no surface response or calibrated prediction proof.'])

def timing_report(root):
    root=Path(root);prior=root.parent/'fire-fluid-flow-spatial-20261010/paired/grid-128'
    old_identity=json.loads((prior.parent/'run-identity.json').read_text())
    if old_identity['source_sha256']!=SOURCE or old_identity['fine_source_sha256']!=FINE_SOURCE or json.loads((prior.parent/'terminal.json').read_text())['returncode']!=0:raise ValueError('accepted fine source identity')
    s0=json.loads((prior/'summary.json').read_text());validate_spatial(s0,128)
    if digest(prior/'fields.bin')!=s0['fields_sha256'] or json.loads((prior/'terminal.json').read_text())['returncode']!=0:raise ValueError('accepted fine baseline')
    arrays=[load(prior/'fields.bin',128)];summaries=[s0];identities=[]
    for dt,label in [(.0025,'fine-0025'),(.00125,'fine-00125')]:
        i,s=validate_identity(root/label,False);validate_fine(s,dt)
        if i['flow_dt_s']!=dt or (i['command'][1]!='128' or float(i['command'][2])!=dt or i['command'][3:5]!=['0',i['fine_source']]) or 'memory_live_bytes=0 rejected_allocations=0' not in (root/label/'native.log').read_text():raise ValueError('fine command/closure')
        a=load(root/label/'fields.bin',128);totals(a,s);arrays.append(a);summaries.append(s);identities.append(i)
    comparisons=[pair(arrays[k],arrays[k+1],summaries[k],summaries[k+1],n=128) for k in range(2)]
    trend={key:comparisons[1][group][key]/comparisons[0][group][key] for group,key in [('heat','local_heat_difference_j'),('full_mac_velocity','absolute_l2')]}
    coarse_root=prior.parent/'grid-64';coarse=load(coarse_root/'fields.bin',64);cs=json.loads((coarse_root/'summary.json').read_text());validate_spatial(cs,64)
    if digest(coarse_root/'fields.bin')!=cs['fields_sha256']:raise ValueError('coarse control hash')
    spatial=[]
    for a,s in zip(arrays,summaries):
        b={k:restrict_extensive(a[k],64) for k in ('energy_j','smoke_kg')};b['face_velocity_m_s']=pack(restrict_faces(a['face_velocity_m_s'],64));comparison=pair(coarse,b,cs,s)
        from .isolated_qualification import PROBES
        from .velocity import sample_mac
        from .flow_time_report import relative_l2
        winds=[{name:sample_mac(fields['face_velocity_m_s'],[n]*3,[1.]*3,p,solid_floor=True,lateral_open=True) for name,p in PROBES.items()} for fields,n in [(coarse,64),(a,128)]]
        comparison['wind_probes_m_s']=winds;comparison['wind_probe_norm']=relative_l2([v for name in PROBES for v in winds[0][name]],[v for name in PROBES for v in winds[1][name]])
        comparison.pop('temporal_screens_passed')
        screens=[comparison['heat']['local_heat_relative_l1'],comparison['heat']['height_profile_relative_l1'],comparison['full_mac_velocity']['relative_l2'],comparison['wind_probe_norm']['relative_l2'],comparison['full_smoke_relative_l1'],comparison['peak_rise_relative_difference']]
        comparison['spatial_screens_passed']=all(v is not None and math.isfinite(v) and v<=.1 for v in screens)
        spatial.append(comparison)
    passed=all(c['temporal_screens_passed'] for c in comparisons) and all(math.isfinite(v) and v<1 for v in trend.values())
    return dict(schema='fine_flow_time_verdict_v1',temporal_screen_passed=passed,identities=identities,runs=summaries,adjacent_time_comparisons=comparisons,refinement_difference_ratios=trend,coarse_5ms_to_fine_spatial_comparisons=spatial,limitations=['Fine flow changes its own old-time thermal feedback; original fine source and scalar timing fixed.','Final-state sensitivity and shrinking differences are not formal asymptotic convergence or calibrated error.','Coarse spatial baseline stays at 5 ms; do not label different flow timing a matched-timing spatial verdict.'])
