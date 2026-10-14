"""Small repeatable transport reference, with explicitly authored Fire calibration."""
import math
from pathlib import Path
from .framework import load_fire


def baseline(growth,fire_worker,fluid_worker,clock_worker,*,fire_denominator=50,fluid_denominator=200,nodes=9,grid=8):
    load_fire(growth)
    from fire_timeline import physical_config
    source=physical_config(nodes,1/fire_denominator)
    source.update(run_id='framework-reference',origin_m=[0,0,0])
    nz=grid//2;n=grid*grid*nz
    return {'growth_root':str(Path(growth).resolve()),'fire_worker':str(Path(fire_worker).resolve()),
        'fluid_worker':str(Path(fluid_worker).resolve()),'clock_worker':str(Path(clock_worker).resolve()),
        'source':source,'periods':[[1,fire_denominator],[1,fluid_denominator],[1,25]],'windows':5,
        'fluid':{'grid':[grid,grid,nz],'length_m':[1,1,.5],'momentum_dt_s':1/fluid_denominator,
            'initial_face_velocity_m_s':[.2]*n+[.05]*n+[.1]*n,
            'properties':{'density_kg_m3':1.2,'dynamic_viscosity_pa_s':.000018,
                'heat_capacity_j_kg_k':1005,'reference_temperature_k':300,
                'conductivity_w_m_k':.026,'tracer_diffusivity_m2_s':.0005}},
        'receiver':{'frame':'right_handed_z_up_meters','origin_m':[0,0,0],
            'spacing_m':[1/grid,1/grid,1/grid],'dimensions':[grid,grid,nz],'layer':0,'fluid_mask':[1]*(grid*grid),
            'injection_depth_m':.125,'injection_fluid_mask':[1]*(grid*grid*math.ceil(.125*grid))}}


def metrics(cp):
    fields=cp['fluid']['fields']
    return {'time_s':fields['time_s'],'energy_j':math.fsum(fields['energy_j']),
        'smoke_kg':math.fsum(fields['smoke_kg']),
        'temperature_peak_k':max(fields['temperature_k']),
        'max_divergence_s_inv':fields['max_divergence_s_inv'],
        'momentum_relative_residual':fields['projection_relative_residual'] if 'projection_relative_residual' in fields else fields['momentum_relative_residual'],
        'budgets':cp['fluid']['budgets']}
