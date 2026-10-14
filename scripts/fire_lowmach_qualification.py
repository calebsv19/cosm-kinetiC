#!/usr/bin/env python3
"""Contained contact/acoustic/buoyancy and bounded mixture-reference gate.

Retains comparison evidence; does not enable thermal exchange or claim flame
accuracy from velocity signs or agreement between numerical resolutions.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
from coupling import hotgas, hotgas_domain as domain, lowmach_cases as cases, gas_mixture as mixture
from surface_sources.growth_fire_v1 import sealed
ROOT=Path(__file__).resolve().parents[1]


def save(root,name,value):
    # Normalize coefficient tuples to JSON lists before applying the binary64
    # digest contract, so the serialized readback has the same identity.
    value=json.loads(json.dumps(value,allow_nan=False))
    (root/(name+'.json')).write_text(json.dumps(sealed(value),indent=2)+'\n')


def brief(case):
    return {k:v for k,v in case.items() if k not in ('config','initial','final','receipt')}


def restart(worker,case,rate,steps):
    source=[0.]*len(case['initial']['conserved']);c=case['config']
    middle,_=domain.advance(c,worker,case['initial'],source,steps//3,rate)
    recovered,_=domain.advance(c,worker,json.loads(json.dumps(middle)),source,steps,rate)
    if recovered!=case['final']: raise ValueError('exact recovery failed')


def slab(worker,retained):
    old=hotgas.checked(json.loads(retained.read_text()))
    c=old['config'];c['domain']['flux']='hllc';u=old['initial']['conserved'];source=old['source'];rate=old['rate']
    initial=domain.initial(c,worker,u,source,rate);current=initial;timeline=[]
    ids=[q for q,t in enumerate(hotgas.primitive(c['gas'],u)[0]) if t>600]
    for tick in (100,500,1000,2500,5000):
        current,r=domain.advance(c,worker,current,source,tick,rate)
        velocities=[current['conserved'][5*q+3]/current['conserved'][5*q] for q in ids]
        timeline.append(dict(tick=tick,mean_selected_w_m_s=math.fsum(velocities)/len(ids),
            gas_plus_potential_residual_j=r['gas_plus_potential_residual_j'],receipt=r))
    whole,_=domain.advance(c,worker,initial,source,5000,rate)
    if current!=whole: raise ValueError('slab interval recovery failed')
    return dict(config=c,initial=initial,final=current,timeline=timeline,
        retained_rusanov_w_m_s=old['mean_selected_velocity_m_s'],
        retained_case_digest=json.loads(retained.read_text())['digest'],
        physical_accuracy_qualified=False,
        interpretation='Unchanged full-width slab and metric; sign reversal is retained, not converted into a passing plume criterion.')


def qualify(args):
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    paths=[Path(__file__),ROOT/'scripts/coupling/lowmach_cases.py',ROOT/'scripts/coupling/gas_mixture.py',
           ROOT/'scripts/coupling/hotgas.py',ROOT/'scripts/coupling/hotgas_domain.py',args.worker,args.sanitized_worker,args.harness]
    pins={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    save(output,'controls',dict(pins=pins))
    for label,worker in [('strict',args.worker),('sanitized',args.sanitized_worker)]:
        env=dict(os.environ,HOTGAS_DOMAIN_WORKER=str(worker.resolve()),PYTHONDONTWRITEBYTECODE='1')
        for test in ('test_hotgas_contact.py','test_hotgas_domain.py','test_hotgas_plane_semantics.py','test_gas_mixture.py'):
            result=subprocess.run(['python3',str(ROOT/'tests'/test),'-v'],env=env,text=True,capture_output=True)
            (output/(label+'-'+test+'.log')).write_text(result.stdout+result.stderr)
            if result.returncode: raise ValueError(label+' '+test+' failed')
    native=subprocess.run([str(args.harness.resolve())],text=True,capture_output=True)
    (output/'native.log').write_text(native.stdout+native.stderr)
    if native.returncode: raise ValueError('native refusal/contact harness failed')
    summary={}
    for flux in ('rusanov','hllc'):
        contact=cases.stationary_contact(args.worker,flux);save(output,'contact-'+flux,contact)
        summary['contact-'+flux]=brief(contact)
        errors=[]
        for nx in (16,32,64):
            case=cases.acoustic(args.worker,flux,nx);save(output,'acoustic-'+flux+'-'+str(nx),case)
            errors.append(case['relative_pressure_l1_error'])
        summary['acoustic-'+flux]=dict(relative_l1_errors=errors,refinement_ratios=[errors[i]/errors[i+1] for i in (0,1)])
    for boundary in ('periodic','slip_wall','reservoir'):
        case=cases.pressure_pulse(args.worker,boundary);save(output,'pulse-'+boundary,case)
        summary['pulse-'+boundary]=brief(case)
    for scale in (1,2,3):
        case=cases.bubble(args.worker,'hllc',scale=scale);save(output,'bubble-mesh-'+str(scale),case)
        summary['bubble-mesh-'+str(scale)]=brief(case)
        print('completed matched bubble mesh '+str(scale),flush=True)
    case=cases.bubble(args.worker,'hllc',steps=2000,rate=(1,1000000));save(output,'bubble-half-dt',case)
    summary['bubble-half-dt']=brief(case)
    case=cases.bubble(args.worker,'hllc',height=2.);save(output,'bubble-taller-domain',case)
    summary['bubble-taller-domain']=brief(case)
    for flux in ('rusanov','hllc'):
        case=cases.bubble(args.worker,flux,steps=5000);restart(args.worker,case,(1,500000),5000)
        save(output,'bubble-10ms-'+flux,case);summary['bubble-10ms-'+flux]=dict(brief(case),exact_restart=True)
        print('completed 10ms bubble '+flux,flush=True)
    slab_case=slab(args.worker,args.retained_slab);save(output,'slab',slab_case)
    summary['slab']=dict(timeline=[{k:v for k,v in row.items() if k!='receipt'} for row in slab_case['timeline']],
        retained_rusanov_w_m_s=slab_case['retained_rusanov_w_m_s'],physical_accuracy_qualified=False)
    property_rows={}
    for name,composition in [('dry_air',mixture.DRY_AIR),('stoichiometric_methanol_products',mixture.METHANOL_PRODUCTS)]:
        low,high=mixture.bounds(composition)
        property_rows[name]=dict(moles=composition,bounds_k=[low,high],
            values=[dict(temperature_k=t,**mixture.properties(composition,t)) for t in (low,1000.,1290.,high)],
            sensible_delta_internal_j_kg=mixture.delta_internal(composition,low,1290.))
    save(output,'mixture-reference',dict(sources=mixture.SOURCES,coefficients=mixture.DATA,rows=property_rows,
        retrieved_utc='2026-10-10',reference='NIST WebBook / Chase 1998; N2/O2 January 2009 fits',
        provenance='Curated numerical transcription verified against displayed Cp and H tables; no downloaded HTML-byte identity claimed.',
        native_eos_connected=False,transport_properties_qualified=False,composition_is_measured=False))
    if pins!={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}:
        raise ValueError('qualification control drift')
    save(output,'assessment',dict(schema='contact_acoustic_mixture_gate/v1',summary=summary,
        exact_stationary_contact_qualified=True,linear_acoustic_refinement_qualified=True,
        native_hllc_transactional_refusals_qualified=True,mixture_reference_evaluator_qualified=True,
        physical_buoyant_accuracy_qualified=False,reacting_gas_qualified=False,
        reciprocal_thermal_exchange_admissible=False,
        next_boundary='Assess remaining mesh and geometry sensitivity; connect bounded caloric EOS with species energy and transport before reacting/radiative feedback.'))
    return output


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('worker','sanitized-worker','harness','retained-slab','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    print(qualify(parser.parse_args()))
