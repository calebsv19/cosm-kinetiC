"""Exact disk/cell support and measured fuel packet; current gas qualification gate.

The existing gas worker receives only a heat surrogate in diagnostic probes;
smoke is never repurposed as fuel mass. Full fuel-source admission remains false.
"""
import copy
import hashlib
import math
from pathlib import Path
from .methanol_reference import SPECS,table
from surface_sources.growth_fire_v1 import sealed
import open_atmosphere


def primitive(x,r):
    x=max(-r,min(r,x))
    return .5*(x*math.sqrt(max(0,r*r-x*x))+r*r*math.asin(x/r))


def disk_cell_area(x0,x1,y0,y1,r):
    lo,hi=max(x0,-r),min(x1,r)
    if lo>=hi: return 0.
    cuts=[lo,hi]
    for y in (y0,y1):
        if abs(y)<r:
            x=math.sqrt(r*r-y*y)
            cuts.extend(t for t in (-x,x) if lo<t<hi)
    cuts=sorted(set(cuts));area=0.
    for a,b in zip(cuts,cuts[1:]):
        height=math.sqrt(max(0,r*r-((a+b)/2)**2))
        top,bottom=min(y1,height),max(y0,-height)
        if top<=bottom: continue
        arc=primitive(b,r)-primitive(a,r)
        upper=y1*(b-a) if y1<height else arc
        lower=y0*(b-a) if y0>-height else -arc
        area+=upper-lower
    return max(0.,area)


def packet(reference,grid=(16,16,32),length=(1.,1.,1.5)):
    import json
    reference=Path(reference)
    provenance=json.loads((reference/'provenance.json').read_text())
    name='Methanol_30_cm_HRR_Sung_2024.csv'
    row=next(x for x in provenance['files'] if x['file']==name)
    if hashlib.sha256((reference/name).read_bytes()).hexdigest()!=row['sha256']:
        raise ValueError('measured fuel source bytes changed')
    data=table(reference/name,*SPECS[name])
    if data[0][1:]!=data[-1][1:]: raise ValueError('steady source endpoints changed')
    temperature_name='Methanol_30_cm_TC_r=0_cm_Sung_2024.csv'
    temperature_row=next(x for x in provenance['files'] if x['file']==temperature_name)
    if hashlib.sha256((reference/temperature_name).read_bytes()).hexdigest()!=temperature_row['sha256']:
        raise ValueError('measured gas temperature bytes changed')
    temperatures=table(reference/temperature_name,*SPECS[temperature_name])
    nx,ny,nz=grid;lx,ly,lz=length;r=.301/2
    if min(lx,ly)<2*r: raise ValueError('disk outside source plane')
    areas=[disk_cell_area(i*lx/nx-lx/2,(i+1)*lx/nx-lx/2,
        j*ly/ny-ly/2,(j+1)*ly/ny-ly/2,r) for j in range(ny) for i in range(nx)]
    exact=math.pi*r*r
    if not math.isclose(math.fsum(areas),exact,rel_tol=1e-12): raise ValueError('disk area budget')
    return sealed({'schema':'prescribed_methanol_fuel/v1','case_id':'nist-sung-2024-macfp-30cm',
        'measurement_sha256':row['sha256'],'reference_commit':provenance['repository_commit'],
        'temperature_measurement_sha256':temperature_row['sha256'],
        'reference_peak_temperature_k':max(row[4] for row in temperatures),
        'mass_rate_kg_s':data[0][2],'measured_ideal_hrr_w':data[0][1],
        'lower_combustion_j_kg':19.9e6,'complete_combustion_power_w':data[0][2]*19.9e6,
        'steady_reference_interval_s':[data[0][0],data[-1][0]],
        'grid':list(grid),'length_m':list(length),'source_disk_area_m2':exact,
        'source_cell_areas_m2':areas,'source_plane':'first cell layer at virtual flat liquid plane',
        'fuel_mass_is_smoke':False,'heat_probe_is_combustion':False,
        'full_fuel_admission_qualified':False})


def request(source,step_count=1,power_scale=1.):
    n=math.prod(source['grid']);plane=source['grid'][0]*source['grid'][1];dt=.005
    energy=[0.]*n
    for i,a in enumerate(source['source_cell_areas_m2']):
        energy[i]=source['complete_combustion_power_w']*power_scale*dt*a/source['source_disk_area_m2']
    return {'schema':open_atmosphere.SCHEMA,'grid':source['grid'],'length_m':source['length_m'],
        'properties':{'density_kg_m3':1.2,'dynamic_viscosity_pa_s':1.8e-5,
            'heat_capacity_j_kg_k':1005.,'reference_temperature_k':300.,
            'conductivity_w_m_k':.026,'tracer_diffusivity_m2_s':0.},
        'momentum_dt_s':dt,'initial_face_velocity_m_s':[0.]*(3*n+plane),
        'initial_energy_j':[0.]*n,'initial_smoke_kg':[0.]*n,
        'boundary_policy':{'schema':'physics_sim_open_reservoir_boundary/v1','horizontal':'periodic_xy',
            'vertical':'solid_bottom_open_top','predictor_velocity':'no_slip_bottom_zero_gradient_top',
            'pressure_datum_pa':[0.,0.],'inflow_temperature_k':[300.,300.],
            'inflow_smoke_concentration_kg_m3':[0.,0.],'scalar_diffusion':'zero_normal_flux'},
        'buoyancy':{'enabled':True,'gravity_m_s2':9.81,'expansion_per_k':1/300.,
            'max_temperature_contrast_fraction':.1},'state':None,
        'steps':[{'energy_j':energy,'smoke_kg':[0.]*n} for _ in range(step_count)]}


def qualify(source,worker):
    from surface_sources.growth_fire_v1 import digest
    if source.get('digest')!=digest(source): raise ValueError('prescribed source digest changed')
    # Successful ordinary command + high-source refusal, not an accuracy pass.
    low=request(source,2,.001)
    accepted=open_atmosphere.run(low,worker)
    first_full=open_atmosphere.run(request(source,1,1.),worker)
    high=request(source,2,1.)
    refused=None
    try: open_atmosphere.run(high,worker)
    except ValueError as error: refused=str(error)
    if refused is None: raise ValueError('expected declared hot-source envelope refusal absent')
    if 'open atmosphere policy, divergence, CFL, flux budget or contrast rejection' not in refused:
        raise ValueError('unclassified gas qualification failure: '+refused)
    return assessment(source,worker,accepted,refused,first_full),accepted


def assessment(source,worker,accepted,refused,first_full=None):
    return sealed({'schema':'prescribed_pool_gas_qualification/v1','status':'not_qualified',
        'worker_sha256':hashlib.sha256(Path(worker).read_bytes()).hexdigest(),
        'source_sha256':source['digest'],'prescribed_mass_rate_kg_s':source['mass_rate_kg_s'],
        'full_heat_probe_power_w':source['complete_combustion_power_w'],
        'ordinary_heat_transport_smoke':{'scale':.001,'duration_s':accepted['fields']['time_s'],
            'peak_temperature_k':max(accepted['fields']['temperature_k'])},
        'full_heat_probe_refused':True,'refusal':refused,
        'first_full_heat_step_peak_k':None if first_full is None else max(first_full['fields']['temperature_k']),
        'solver_admitted_peak_k':330.,
        'native_refusal_diagnostic':'Native terminal error groups policy/divergence/CFL/budget/contrast causes; temperature envelope independently incompatible with benchmark.',
        'reference_peak_temperature_k':source['reference_peak_temperature_k'],
        'reference_relative_contrast':(source['reference_peak_temperature_k']-300.)/300.,
        'solver_max_relative_contrast':.1,
        'missing_capabilities':['Variable-density hot-gas energy/momentum and thermal expansion',
            'Fuel/species mass injection and combustion energy release',
            'Gas radiation and surface-directed heat flux',
            'Benchmark burner/lip geometry and open lateral entrainment',
            'Steady plume and measured sampling locations/time statistics'],
        'coupled_thermal_exchange_enabled':False,'physical_calibration_qualified':False})
