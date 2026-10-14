#!/usr/bin/env python3
"""Experimental-plane refusal, explicit native surface budgets and gas admission.

No fitting or reciprocal exchange. Surface forcing/coefficients in native
fixtures are synthetic, because the raised-gauge experiment cannot identify them.
"""
import argparse
import json
import math
import os
from pathlib import Path
import sys
from coupling.methanol_boundary import experimental_boundary
from coupling.prescribed_pool import packet,qualify
from surface_sources.growth_fire_v1 import sealed


def surface_cases(growth_root,worker,boundary):
    sys.path.insert(0,str(Path(growth_root).resolve()/'scripts'))
    from fire_source import thermal
    area=math.pi*(boundary['geometry']['diameter_m']/2)**2
    material=dict(area_m2=area,mass_kg=area*.14*791.01,cp_j_kg_k=81.13/.03204,
        latent_j_kg=35210/.03204,boiling_k=337.85,feed_k=293.15,minimum_k=273.15,
        combustion_j_kg=19.9e6)
    configurations=[('no_losses',0.,0.,0.),('reflection',.1,0.,0.),
        ('surface_radiation',0.,.9,0.),('cooling',0.,0.,5.),('combined',.1,.9,5.)]
    results=[]
    gauge_refused=False
    for name,reflectivity,emissivity,conductance in configurations:
        c=dict(material=material,boundary=dict(cp_slope_j_kg_k2=0.,reflectivity=reflectivity,
            emissivity=emissivity,surroundings_k=293.15,cooling_conductance_w_k=conductance,coolant_k=293.15))
        initial=thermal.initial(c,worker,337.85,tick_seconds=(1,1));state=initial;receipts=[]
        forcing=dict(location='liquid_surface',radiation_w_m2=1000/area,convection_w_m2=600/area,loss_w_m2=0.)
        try: thermal.advance(c,worker,state,dict(forcing,location='gauge_13mm_above_liquid'),100,(1,1))
        except ValueError: gauge_refused=True
        else: raise ValueError('raised-gauge forcing unexpectedly admitted')
        for tick in range(100,2001,100):
            state,r=thermal.advance(c,worker,state,forcing,tick,(1,1));receipts.append(r)
            if tick==1000: recovered=json.loads(json.dumps(state))
        replay=recovered;replayed=[]
        for tick in range(1100,2001,100):
            replay,r=thermal.advance(c,worker,replay,forcing,tick,(1,1));replayed.append(r)
        if replay!=state or replayed!=receipts[10:]: raise ValueError('thermal recovery failed')
        results.append({'case':name,'synthetic_forcing_and_coefficients':True,'config_si':c,
            'initial':initial,'final':state,'receipts':receipts,'exact_restart':True,
            'steady_mass_rate_kg_s':state['supplied_mass_kg']/2000,
            'max_budget_residual_j':max(abs(r['budget_si']['residual_j']) for r in receipts)})
    measured=.0135*area
    specific=material['latent_j_kg']+material['cp_j_kg_k']*(337.85-293.15)
    return sealed({'schema':'explicit_surface_boundary_qualification/v1',
        'experimental_case_digest':boundary['digest'],'raised_gauge_forcing_refused':gauge_refused,
        'surface_budget_numerics_qualified':True,'experimental_surface_energy_closure_qualified':False,
        'parameters_fitted':False,'cases':results,'same_study_measured_mass_rate_kg_s':measured,
        'diagnostic_if_raised_gauge_were_surface_input':{
            'admissible':False,'gauge_power_w':1600,'predicted_no_loss_mass_rate_kg_s':1600/specific,
            'relative_error':1600/specific/measured-1,
            'unassigned_difference_w':1600-measured*specific,
            'interpretation':'Includes measurement plane mismatch and possible losses; not measured burner loss.'},
        'unresolved':boundary['unresolved_boundary_parameters']})

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','thermal-worker','fluid-worker','reference','output'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
    if any(a.output.iterdir()): raise ValueError('fresh task-owned output required')
    boundary=experimental_boundary();source=packet(a.reference)
    for name,value in [('experimental-boundary',boundary),('prescribed-source',source),
                       ('surface-qualification',surface_cases(a.growth_root,a.thermal_worker,boundary))]:
        (a.output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n')
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output/'gas-attempts')
    gas,accepted=qualify(source,a.fluid_worker)
    (a.output/'gas-qualification.json').write_text(json.dumps(gas,indent=2)+'\n')
    print(json.dumps({'surface_numerics':'passed','experimental_surface_closure':'unresolved',
        'gas_qualification':gas['status'],'coupled_thermal_exchange_enabled':False},indent=2))
