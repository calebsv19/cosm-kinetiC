#!/usr/bin/env python3
"""Retain matched first/second-order spatial comparisons; no thermal admission."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from coupling import buoyancy_reference as reference,lowmach_cases as cases,hotgas,hotgas_domain as domain
from fire_lowmach_qualification import save,brief,restart
ROOT=Path(__file__).resolve().parents[1]


def qualify(worker,output,retained,resume=False):
    output.mkdir(parents=True,exist_ok=resume)
    paths=[Path(__file__),ROOT/'scripts/coupling/buoyancy_reference.py',ROOT/'scripts/coupling/lowmach_cases.py',
           ROOT/'scripts/coupling/hotgas_domain.py',ROOT/'scripts/coupling/hotgas.py',worker]
    pins={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    save(output,'controls',dict(pins=pins,resumed_partial_evidence=resume))
    def cached(name,run):
        path=output/(name+'.json')
        if resume and path.exists():
            row=hotgas.checked(json.loads(path.read_text()))
            source=[0.]*len(row['initial']['conserved']);rate=row.get('rate',(1,1000000))
            for checkpoint in (row['initial'],row['final']):
                domain.state(row['config'],worker,checkpoint,source,rate)
            if (row['receipt']['prior_digest']!=row['initial']['digest'] or
                row['receipt']['next_digest']!=row['final']['digest']):
                raise ValueError('cached receipt lineage')
            return row
        row=run();save(output,name,row);return row
    summary={}
    for flux in ('hllc','hllc_muscl'):
        for n in (8,16,32):
            name=f'buoyancy-{flux}-{n}'
            row=cached(name,lambda:reference.case(worker,flux,n));summary[name]=brief(row)
            print(name,summary[name]['relative_velocity_l1'],flush=True)
        errors=[]
        for n in (16,32,64):
            row=cached(f'acoustic-{flux}-{n}',lambda:cases.acoustic(worker,flux,n))
            errors.append(row['relative_pressure_l1_error'])
        summary['acoustic-'+flux]=dict(errors=errors,ratios=[errors[i]/errors[i+1] for i in (0,1)])
    row=cached('prepared-buoyancy-32',lambda:reference.case(worker,'hllc_muscl',32,prepared=True))
    summary['prepared-buoyancy-32']=brief(row)
    for scale in (1,2,3):
        row=cached('bubble-'+str(scale),lambda:cases.bubble(worker,'hllc_muscl',scale=scale))
        summary['bubble-'+str(scale)]=brief(row)
        print('bubble',scale,summary['bubble-'+str(scale)]['selected_region_mean_w_m_s'],flush=True)
    raw=json.loads(retained.read_text());old=hotgas.checked(raw)
    c=copy.deepcopy(old['config']);c['domain']['flux']='hllc_muscl';u=old['initial']['conserved'];source=old['source'];rate=old['rate']
    initial=domain.initial(c,worker,u,source,rate)
    final,receipt=domain.advance(c,worker,initial,source,5000,rate)
    ids=[q for q,t in enumerate(hotgas.primitive(c['gas'],u)[0]) if t>600]
    mean=math.fsum(final['conserved'][5*q+3]/final['conserved'][5*q] for q in ids)/len(ids)
    slab=dict(config=c,initial=initial,final=final,receipt=receipt,source=source,rate=rate,
              selected_region_mean_w_m_s=mean,retained_case_digest=raw['digest'],physical_accuracy_qualified=False)
    restart(worker,slab,rate,5000);save(output,'original-slab',slab)
    summary['original-slab']=dict(mean_w_m_s=mean,exact_restart=True,physical_accuracy_qualified=False)
    if pins!={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}:raise ValueError('control drift')
    acoustic=summary['acoustic-hllc_muscl']
    if min(acoustic['ratios'])<=2.5 or acoustic['errors'][-1]>=.003:
        raise ValueError('smooth acoustic accuracy gate failed')
    if (summary['buoyancy-hllc_muscl-32']['relative_velocity_l1']>=.1 or
        summary['buoyancy-hllc_muscl-16']['relative_velocity_l1']/summary['buoyancy-hllc_muscl-32']['relative_velocity_l1']<=2):
        raise ValueError('linear buoyancy velocity gate failed')
    save(output,'assessment',dict(schema='spatial_accuracy_gate/v1',controls=pins,summary=summary,
        second_order_periodic_acoustic_qualified=True,linear_buoyancy_velocity_improved=True,
        all_speed_pressure_accuracy_qualified=False,finite_contrast_buoyant_accuracy_qualified=False,
        reciprocal_thermal_exchange_admissible=False,
        next_boundary='Independently qualify low-Mach pressure/velocity treatment before physical reacting thermal feedback.'))
    return output


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('worker','output','retained-slab'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--resume',action='store_true',help='reuse sealed partial cases only if current worker/adapter identity and lineage match')
    a=p.parse_args();print(qualify(a.worker.resolve(),a.output.resolve(),a.retained_slab.resolve(),a.resume))
