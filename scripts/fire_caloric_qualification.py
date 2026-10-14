#!/usr/bin/env python3
"""Frozen-mixture native caloric/energy qualification, not reacting-fire proof."""
import argparse,hashlib,json
from pathlib import Path
from coupling import caloric,caloric_cases as cases,hotgas,hotgas_domain as domain
from fire_lowmach_qualification import save
ROOT=Path(__file__).resolve().parents[1]


def qualify(worker,output,retained,probe,resume=False):
    paths=[Path(__file__),worker,probe,ROOT/'scripts/build_hotgas.py',ROOT/'tests/test_hotgas_caloric.py']
    paths += [ROOT/'scripts/coupling'/name for name in ('caloric.py','caloric_cases.py','gas_mixture.py','hotgas.py','hotgas_domain.py','hotgas_domain_cases.py')]
    pins={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    if resume:
        if hotgas.checked(json.loads((output/'controls.json').read_text()))['pins']!=pins:raise ValueError('resume control drift')
    else:
        output.mkdir(parents=True,exist_ok=False);save(output,'controls',dict(pins=pins,scope='Frozen composition sensible energy, h298.15=0; 200..1500 K; no formation/chemical energy.'))
    summary={}
    def record(name,run):
        path=output/(name+'.json')
        if resume and path.exists():
            row=hotgas.checked(json.loads(path.read_text()))
            for cp in (row['initial'],row['final']):domain.state(row['config'],worker,cp,row['source'],row['rate'])
        else:row=run();save(output,name,row)
        summary[name]={k:v for k,v in row.items() if k not in ('config','initial','final','source','receipts')}
        print('completed '+name,flush=True);return row
    for moles,name,begin in ((caloric.AIR,'air',300.),(caloric.PRODUCTS,'products',500.)):
        row=record('nist-heating-'+name,lambda:cases.heating(worker,moles,begin))
        if abs(row['final_temperature_k']-1300)>=1:raise ValueError('independent NIST heating failed')
        row=record('stationary-contact-'+name,lambda:cases.thermal_contact(worker,moles))
        if row['max_density_error']>=1e-12 or row['max_pressure_error_pa']>=1e-6:raise ValueError('thermal contact failed')
    row=record('product-cooling-to-ambient',lambda:cases.heating(worker,caloric.PRODUCTS,1300.,300.,False))
    if abs(row['final_temperature_k']-300)>1e-8:raise ValueError('native cool-to-hot continuity failed')
    errors=[]
    for n in (16,32,64):
        row=record('entropy-transport-'+str(n),lambda:cases.transport(worker,caloric.PRODUCTS,n));errors.append(row['relative_density_disturbance_l1'])
    if errors[-1]>=.01 or min(errors[i]/errors[i+1] for i in (0,1))<=2:raise ValueError('entropy transport refinement failed')
    for scale in (2,3):
        record('air-bubble-'+str(scale),lambda:cases.retained_geometry(worker,retained/'fire-fluid-lowmach-20261009/accepted'/f'bubble-mesh-{scale}.json',1000,caloric.AIR))
    row=record('air-original-slab',lambda:cases.retained_geometry(worker,retained/'fire-fluid-domain-20261009/diagnosis/slab.json',5000,caloric.AIR))
    current,_=domain.advance(row['config'],worker,row['initial'],row['source'],1700,row['rate'])
    while current['tick']<5000:
        current,_=domain.advance(row['config'],worker,json.loads(json.dumps(current)),row['source'],min(5000,current['tick']+250),row['rate'])
    if current!=row['final']:raise ValueError('exact nontrivial caloric restart failed')
    summary['air-original-slab']['exact_restart']=True
    if pins!={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}:raise ValueError('caloric control drift')
    save(output,'assessment',dict(schema='native_frozen_caloric_gate/v1',summary=summary,controls=pins,
        native_continuous_caloric_gate_passed=True,nonreacting_fixed_mixture_energy_transport_qualified=True,
        evolving_species_transport_qualified=False,condensation_qualified=False,combustion_qualified=False,
        radiation_qualified=False,measured_surface_response_qualified=False,physical_plume_accuracy_qualified=False,
        reciprocal_thermal_exchange_admissible=False,
        next_boundary='Conservative evolving species/formation energy and calibrated transport, then independent combustion/radiation/surface cases before reciprocal exchange.'))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('worker','output','retained-root','probe'):p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--resume',action='store_true')
    a=p.parse_args();qualify(a.worker.resolve(),a.output.resolve(),a.retained_root.resolve(),a.probe.resolve(),a.resume)
