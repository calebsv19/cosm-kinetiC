#!/usr/bin/env python3
"""Bounded low-Mach pressure gate with retained geometry and acoustic controls."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from coupling import hotgas,hotgas_domain as domain,vortex_reference as vortex,buoyancy_reference as buoyancy,lowmach_cases as acoustic,lowmach_pressure_cases as stability
from fire_lowmach_qualification import save
ROOT=Path(__file__).resolve().parents[1]


def geometry(worker,path,steps):
    raw=json.loads(path.read_text());old=hotgas.checked(raw)
    c=copy.deepcopy(old['config']);c['domain']['flux']='lhllc_muscl'
    u=old['initial']['conserved'];source=old.get('source',[0.]*len(u));rate=old['rate']
    initial=domain.initial(c,worker,u,source,rate);current=initial;receipts=[]
    while current['tick']<steps:
        current,r=domain.advance(c,worker,current,source,min(steps,current['tick']+250),rate);receipts.append(r)
    selected=[q for q,t in enumerate(hotgas.primitive(c['gas'],u)[0]) if t>600]
    mean=math.fsum(current['conserved'][5*q+3]/current['conserved'][5*q] for q in selected)/len(selected)
    return dict(config=c,initial=initial,final=current,receipts=receipts,source=source,rate=rate,
        duration_s=steps*float(hotgas.rate(rate)),selected_region_mean_w_m_s=mean,
        retained_input_digest=raw['digest'],matched_initial_arrays=True,
        max_gas_plus_potential_residual_j=max(abs(r['gas_plus_potential_residual_j']) for r in receipts),
        physical_buoyant_accuracy_qualified=False)


def qualify(worker,output,retained,resume=False):
    paths=[Path(__file__),ROOT/'scripts/fire_lowmach_qualification.py',ROOT/'scripts/build_hotgas.py',
        ROOT/'tests/test_hotgas_lowmach_pressure.py',worker,*[ROOT/'scripts/coupling'/name for name in
        ('hotgas.py','hotgas_domain.py','hotgas_domain_cases.py','vortex_reference.py','buoyancy_reference.py','lowmach_cases.py','lowmach_pressure_cases.py')]]
    pins={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    if resume:
        before=hotgas.checked(json.loads((output/'controls.json').read_text()))
        if before['pins']!=pins:raise ValueError('resume controls drift; use fresh output')
    else:
        output.mkdir(parents=True,exist_ok=False);save(output,'controls',dict(pins=pins))
    def record(name,run):
        path=output/(name+'.json')
        if resume and path.exists():
            row=hotgas.checked(json.loads(path.read_text()))
            source=row.get('source',[0.]*len(row['initial']['conserved']))
            # Vortex metadata has a reference URL, not an applied source packet.
            if source==vortex.SOURCE:source=[0.]*len(row['initial']['conserved'])
            rate=row.get('rate',(1,1000000))
            for cp in (row['initial'],row['final']):domain.state(row['config'],worker,cp,source,rate)
        else:row=run();save(output,name,row)
        print('completed '+name,flush=True);return row
    summary={}
    def metrics(row):
        return {k:v for k,v in row.items() if k not in ('config','initial','final','receipt','receipts','source')}
    for flux in ('hllc_muscl','lhllc_muscl'):
        for mach in (.1,.01,.001):
            name=f'vortex-{flux}-{mach}';row=record(name,lambda:vortex.case(worker,flux,mach=mach));summary[name]=metrics(row)
    name='vortex-refined';row=record(name,lambda:vortex.case(worker,'lhllc_muscl',n=32,mach=.01));summary[name]=metrics(row)
    name='vortex-longer';row=record(name,lambda:vortex.case(worker,'lhllc_muscl',mach=.001,duration=(1,10)));summary[name]=metrics(row)
    for prepared in (False,True):
        name='buoyancy-prepared-'+str(prepared)
        row=record(name,lambda:buoyancy.case(worker,'lhllc_muscl',32,prepared=prepared));summary[name]=metrics(row)
    for n in (16,32,64):
        name='acoustic-'+str(n);row=record(name,lambda:acoustic.acoustic(worker,'lhllc_muscl',n));summary[name]=metrics(row)
    for boundary in ('periodic','slip_wall','reservoir'):
        name='weak-pulse-'+boundary
        row=record(name,lambda:acoustic.pressure_pulse(worker,boundary,'lhllc_muscl'));summary[name]=metrics(row)
    for gravity in (False,True):
        name='noise-'+str(gravity);row=record(name,lambda:stability.noise(worker,gravity));summary[name]=metrics(row)
    for scale in (2,3):
        name='bubble-'+str(scale)
        row=record(name,lambda:geometry(worker,retained/'fire-fluid-lowmach-20261009/accepted'/f'bubble-mesh-{scale}.json',1000));summary[name]=metrics(row)
    name='original-slab';row=record(name,lambda:geometry(worker,retained/'fire-fluid-domain-20261009/diagnosis/slab.json',5000));summary[name]=metrics(row)
    middle,_=domain.advance(row['config'],worker,row['initial'],row['source'],1700,row['rate']);current=middle
    while current['tick']<5000:
        current,_=domain.advance(row['config'],worker,json.loads(json.dumps(current)),row['source'],min(5000,current['tick']+250),row['rate'])
    if current!=row['final']:raise ValueError('nontrivial slab restart failed')
    summary[name]['exact_restart']=True
    fine=summary['vortex-lhllc_muscl-0.001'];medium=summary['vortex-lhllc_muscl-0.01']
    if fine['relative_velocity_l1']>=.1 or fine['relative_pressure_shape_l1']>=.15 or fine['kinetic_energy_ratio']<=.98:
        raise ValueError('selected low-Mach vortex gate failed')
    if fine['relative_velocity_l1']/medium['relative_velocity_l1']>=1.2:raise ValueError('Mach scaling failed')
    errors=[summary['acoustic-'+str(n)]['relative_pressure_l1_error'] for n in (16,32,64)]
    if errors[-1]>=.004 or min(errors[i]/errors[i+1] for i in (0,1))<=2.5:raise ValueError('acoustic refinement failed')
    if summary['buoyancy-prepared-True']['relative_pressure_perturbation_l1']>=.05:raise ValueError('prepared pressure reference failed')
    if max(summary['noise-'+str(g)]['acoustic_energy_ratio'] for g in (False,True))>=1.05:raise ValueError('noise growth failed')
    pulse=[summary['weak-pulse-'+b]['left_reflection_l1_ratio'] for b in ('periodic','slip_wall','reservoir')]
    if max(pulse[0],pulse[2])>=.01 or not .95<pulse[1]<1.05:raise ValueError('weak pulse characteristic gate failed')
    if pins!={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}:raise ValueError('qualification control drift')
    save(output,'assessment',dict(schema='lowmach_pressure_gate/v1',controls=pins,summary=summary,
        selected_low_mach_pressure_velocity_gate_passed=True,physical_acoustic_cfl_retained=True,
        long_time_all_mach_stability_qualified=False,multidimensional_shocks_qualified=False,
        finite_contrast_plume_accuracy_qualified=False,reciprocal_thermal_exchange_admissible=False,
        next_boundary='Continuous caloric EOS and supported cool-to-hot species energy/transport, with retained finite-contrast and longer-time numerical checks.'))
    return output


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('worker','output','retained-root'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--resume',action='store_true')
    a=p.parse_args();print(qualify(a.worker.resolve(),a.output.resolve(),a.retained_root.resolve(),a.resume))
