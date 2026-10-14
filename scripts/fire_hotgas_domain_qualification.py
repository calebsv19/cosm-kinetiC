#!/usr/bin/env python3
"""Contained domain/gravity/conduction and experimental-height sampling proof.

Manufactured flow and a measured-power proxy; not reacting-flame calibration.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
from coupling import hotgas, hotgas_domain as domain
from coupling.hotgas_domain_cases import config, face, hydrostatic, temperature_profile
from coupling.prescribed_pool import packet
from coupling.methanol_boundary import gauge_height_constraint
from surface_sources.growth_fire_v1 import sealed
ROOT=Path(__file__).resolve().parents[1]


def save(output,name,value):
    (output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n')


def buoyancy(worker):
    c=config(grid=(12,12,32),length=(.5,.5,1.))
    c['domain']['faces']=[face('reservoir') for _ in range(6)];c['domain']['faces'][4]=face()
    u=hydrostatic(c);indices=[]
    for z in range(12,20):
        for y in range(4,8):
            for x in range(4,8):
                q=x+12*y+144*z;u[5*q]/=3;indices.append(q)
    source=[0.]*len(u);rate=(1,500000);steps=5000
    initial=domain.initial(c,worker,u,source,rate)
    final,receipt=domain.advance(c,worker,initial,source,steps,rate)
    temperatures,_=hotgas.primitive(c['gas'],final['conserved'])
    selected_velocity=math.fsum(final['conserved'][5*q+3]/final['conserved'][5*q] for q in indices)/len(indices)
    if selected_velocity<=0: raise ValueError('longer hot-region response not upward')
    mid,_=domain.advance(c,worker,initial,source,1777,rate)
    recovered,_=domain.advance(c,worker,json.loads(json.dumps(mid)),source,steps,rate)
    if recovered!=final: raise ValueError('buoyancy exact restart failed')
    return sealed(dict(schema='manufactured_buoyant_transient/v1',config=c,initial=initial,final=final,receipt=receipt,
        duration_s=.01,substeps=steps,rate=list(rate),source=source,exact_restart=True,
        initially_hot_eulerian_region_mean_vertical_velocity_m_s=selected_velocity,
        final_temperature_range_k=[min(temperatures),max(temperatures)],
        physical_buoyant_accuracy_qualified=False,
        interpretation='Localized manufactured 900 K low-density region. Sign and conservation checks only; strong low-Mach damping remains. Not fuel/species evolution or flame validation.'))


def powered_open_case(worker,reference):
    c=config(grid=(8,8,32),length=(.6,.6,.256),conductivity=.026)
    c['domain']['faces']=[face('reservoir') for _ in range(6)]
    c['domain']['faces'][4]=face(wall_temperature=300.)
    u=hydrostatic(c);source_packet=packet(reference,c['gas']['grid'],c['gas']['length_m'])
    source=[0.]*len(u);rate=(1,500000);dt=float(hotgas.rate(rate));steps=5000
    volume=math.prod(c['gas']['length_m'])/math.prod(c['gas']['grid'])
    for q,area in enumerate(source_packet['source_cell_areas_m2']):
        source[5*q+4]=source_packet['complete_combustion_power_w']*dt*area/source_packet['source_disk_area_m2']/volume
    initial=domain.initial(c,worker,u,source,rate)
    final,receipt=domain.advance(c,worker,initial,source,steps,rate)
    middle,_=domain.advance(c,worker,initial,source,1777,rate)
    recovered,_=domain.advance(c,worker,json.loads(json.dumps(middle)),source,steps,rate)
    if final!=recovered: raise ValueError('open powered exact restart failed')
    observation=domain.observe(c,worker,final,source,rate,[0.,.013])
    powers=[math.fsum(a*q for a,q in zip(source_packet['source_cell_areas_m2'],sample['conducted_downward_w_m2']))
        for sample in observation['native_samples']]
    temperatures,_=hotgas.primitive(c['gas'],final['conserved'])
    if max(temperatures)<=300 or powers[0]<=0: raise ValueError('heated open field has no positive surface conduction')
    return sealed(dict(schema='open_domain_prescribed_power_probe/v1',config=c,source_packet=source_packet,
        source=source,rate=list(rate),initial=initial,final=final,receipt=receipt,observation=observation,
        duration_s=.01,substeps=steps,source_energy_j=source_packet['complete_combustion_power_w']*.01,
        final_temperature_range_k=[min(temperatures),max(temperatures)],exact_restart=True,
        circular_support_instantaneous_conductive_power_w=powers,
        surface_temperature_k=300.,synthetic_boundary_and_air_properties=True,
        experimental_geometry_matched=False,fuel_admission_qualified=False,radiative_flux_qualified=False,
        interpretation='Measured complete-combustion power applied as prescribed heat. Flat cooled slip wall/open reservoirs; no burner lip, reacting source or physical gauge model.'))


def plane_refinement(worker):
    rows=[]
    for nz in (8,16,32):
        c=config(grid=(4,4,nz),length=(.032,.032,.032),gravity=(0.,0.,0.),conductivity=.026)
        c['domain']['faces'][4]=face(wall_temperature=300.)
        u=temperature_profile(c,curvature=20000.);source=[0.]*len(u);rate=(1,1000000)
        s=domain.initial(c,worker,u,source,rate)
        o=domain.observe(c,worker,s,source,rate,[0.,.013])
        sampled=[sample['conducted_downward_w_m2'][0] for sample in o['native_samples']]
        rows.append(dict(nz=nz,spacing_m=.032/nz,surface_flux_w_m2=sampled[0],surface_error_w_m2=abs(sampled[0]-26.),
            gauge_flux_w_m2=sampled[1],gauge_error_w_m2=abs(sampled[1]-.026*(1000+40000*.013)),observation=o))
    return sealed(dict(schema='manufactured_plane_refinement/v1',surface_exact_w_m2=26.,gauge_exact_w_m2=39.52,
        reference_temperature='300 + 1000*z + 20000*z*z K; exact cell averages',conductivity_w_m_k=.026,
        rows=rows,surface_refinement_ratios=[rows[i]['surface_error_w_m2']/rows[i+1]['surface_error_w_m2'] for i in (0,1)],
        experimental_total_flux_comparison_qualified=False))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('worker','sanitized-worker','harness','periodic-worker','reference','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    controls=[Path(__file__),ROOT/'scripts/coupling/hotgas.py',ROOT/'scripts/coupling/hotgas_domain.py',
        ROOT/'scripts/coupling/hotgas_domain_cases.py',ROOT/'scripts/coupling/prescribed_pool.py',
        ROOT/'scripts/coupling/methanol_boundary.py',ROOT/'tests/test_hotgas_domain.py',args.worker,args.sanitized_worker,args.harness,args.periodic_worker]
    pins={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in controls}
    tests=[('domain-strict',[os.sys.executable,str(ROOT/'tests/test_hotgas_domain.py'),'-v'],args.worker),
           ('domain-sanitized',[os.sys.executable,str(ROOT/'tests/test_hotgas_domain.py'),'-v'],args.sanitized_worker),
           ('native-refusal',[str(args.harness.resolve())],args.worker),
           ('periodic-regression',[os.sys.executable,str(ROOT/'tests/test_hotgas.py'),'-v'],args.worker)]
    for name,command,worker in tests:
        env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',HOTGAS_DOMAIN_WORKER=str(worker.resolve()),HOTGAS_WORKER=str(args.periodic_worker.resolve()))
        result=subprocess.run(command,text=True,capture_output=True,env=env,timeout=60)
        (output/(name+'.log')).write_text(result.stdout+result.stderr)
        if result.returncode: raise RuntimeError(name+' failed; retained log')
    b=buoyancy(args.worker);save(output,'buoyant-transient',b)
    p=powered_open_case(args.worker,args.reference);save(output,'powered-open-case',p)
    planes=plane_refinement(args.worker);save(output,'plane-refinement',planes)
    save(output,'experimental-plane-constraint',gauge_height_constraint())
    assessment=sealed(dict(schema='hotgas_domain_qualification/v1',numerical_domain_qualified=True,
        physical_pool_fire_qualified=False,reciprocal_thermal_exchange_enabled=False,
        wall='inviscid slip, zero mass leakage; prescribed-temperature or insulated thermal boundary',
        open='prescribed hydrostatic reservoir states; not qualified nonreflecting boundaries',
        gravity='local isothermal hydrostatic reconstruction and conservative gas-plus-potential ledger',
        conduction='constant specified k, explicit Fourier energy transport; combined acoustic/diffusive CFL',
        strict_tests=10,sanitized_tests=10,native_refusal_harness_passed=True,periodic_regression_tests=7,
        buoyant_velocity_m_s=b['initially_hot_eulerian_region_mean_vertical_velocity_m_s'],
        buoyant_gas_plus_potential_residual_j=b['receipt']['gas_plus_potential_residual_j'],
        open_source_energy_j=p['source_energy_j'],open_gas_plus_potential_residual_j=p['receipt']['gas_plus_potential_residual_j'],
        open_instantaneous_conductive_powers_w=p['circular_support_instantaneous_conductive_power_w'],
        native_plane_heights_m=[0.,.013],surface_refinement_ratios=planes['surface_refinement_ratios'],
        surface_radiative_component_qualified=False,experimental_gauge_response_qualified=False,
        parameters_fitted=False,
        physical_buoyant_accuracy_qualified=False,
        retained_failed_slab='../diagnosis/slab.json; fixed initial Eulerian region velocity reversed at 10 ms; cause not independently identified',
        low_mach_accuracy='Localized transient remains upward but strongly damped. No plume accuracy claim; retain slab and localized timeline for follow-up.',
        remaining=['Temperature-dependent mixture/species thermodynamics and transport',
            'Viscous no-slip burner/lip geometry and boundary-layer resolution',
            'Measured fuel/species source and combustion energy', 'Gas radiation and gauge/surface absorption',
            'Matched-domain/time/reference statistical comparison']))
    save(output,'assessment',assessment)
    if pins!={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in controls}:
        raise RuntimeError('qualification controls drifted')
    save(output,'controls',sealed(dict(sha256=pins)))
    print(json.dumps({k:assessment[k] for k in ('buoyant_velocity_m_s','open_gas_plus_potential_residual_j','open_instantaneous_conductive_powers_w','surface_refinement_ratios')},indent=2))
