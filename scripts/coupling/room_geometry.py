"""Independent SI mask and opening diagnostics for a single-window heater room."""
import math
from surface_sources.growth_fire_v1 import keys,require
from .open_boundaries import face
SCHEMA='physics_sim_single_window_room/v1'

def bounds(config):
    g=config['rig_geometry'];keys(g,('schema','window_profile'))
    require(g['schema']==SCHEMA and g['window_profile'] in ('small','large'),'room schema/profile')
    return (.25,.75) if g['window_profile']=='large' else (.375,.625)

def topology(config):
    lo,hi=bounds(config);n=config['grid'][0]
    require(config['grid']==[n]*3 and n in (16,32) and config['length_m']==[1,1,1],'unit room grid')
    require(config['boundary_policy']['horizontal']=='ambient_open_xy' and config['boundary_policy']['vertical']=='solid_bottom_open_top','room pressure/ground convention')
    w=config['open_domain_forcing'];require(all(k['strength']==0 for k in w['knots']),'room has no imposed wind')
    result=[]
    for k in range(n):
        for j in range(n):
            for i in range(n):
                x,y,z=(i+.5)/n,(j+.5)/n,(k+.5)/n
                opening=lo<=y<hi and .375<=z<.875
                result.append(int(x<.0625 or y<.0625 or y>=.9375 or z>=.9375 or (x>=.9375 and not opening)))
    return result

def solid_boxes(config):
    lo,hi=bounds(config)
    return [('west',[0,0,0],[.0625,1,1]),('y_min',[0,0,0],[1,.0625,1]),('y_max',[0,.9375,0],[1,1,1]),('ceiling',[0,0,.9375],[1,1,1]),
            ('east_y_lower',[.9375,0,0],[1,lo,1]),('east_y_upper',[.9375,hi,0],[1,1,1]),('east_z_lower',[.9375,lo,0],[1,hi,.375]),('east_z_upper',[.9375,lo,.875],[1,hi,1])]

def probes(config,data):
    n=config['grid'][0];lo,hi=bounds(config);v=data['face_velocity_m_s'];area=1/n**2
    profile=[]
    for k in range(3*n//8,7*n//8):
        values=[v[face(config['grid'],0,n,j,k,True)]*area for j in range(round(lo*n),round(hi*n))]
        profile.append(dict(height_m=(k+.5)/n,outflow_m3_s=math.fsum(max(x,0) for x in values),inflow_m3_s=math.fsum(max(-x,0) for x in values)))
    out=math.fsum(x['outflow_m3_s'] for x in profile);incoming=math.fsum(x['inflow_m3_s'] for x in profile)
    return dict(window_area_m2=(hi-lo)*.5,window_volume_rate_m3_s=out-incoming,window_outflow_m3_s=out,window_inflow_m3_s=incoming,window_height_profile=profile)

def observations(config,result):
    data=result['state']['data'];n=config['grid'][0];mask=topology(config)
    cap=config['properties']['density_kg_m3']*config['properties']['heat_capacity_j_kg_k']/n**3
    smoke=math.fsum(data['smoke_kg']);energy=math.fsum(data['energy_j'])
    vertical=[]
    for k in range(n):
        q=[(k*n+j)*n+i for j in range(n) for i in range(n) if not mask[(k*n+j)*n+i]]
        if q:vertical.append(dict(height_m=(k+.5)/n,temperature_k=config['properties']['reference_temperature_k']+math.fsum(data['energy_j'][i] for i in q)/(cap*len(q)),smoke_kg=math.fsum(data['smoke_kg'][i] for i in q)))
    centroid=math.fsum(x['smoke_kg']*x['height_m'] for x in vertical)/smoke if smoke else None
    upper=math.fsum(x['smoke_kg'] for x in vertical if x['height_m']>=.75)
    budget=result['budgets'];heat_defect=energy+budget['energy_j']['outflow']-budget['energy_j']['inflow']-data['initial_energy_j']-data['input_energy_j']
    smoke_defect=smoke+budget['smoke_kg']['outflow']-budget['smoke_kg']['inflow']-data['initial_smoke_kg']-data['input_smoke_kg']
    return dict(time_s=data['time_s'],gas_steps=data['steps'],gas_energy_j=energy,gas_smoke_kg=smoke,smoke_centroid_z_m=centroid,upper_quarter_smoke_fraction=upper/smoke if smoke else 0,vertical_profiles=vertical,peak_temperature_k=config['properties']['reference_temperature_k']+max(data['energy_j'])/cap,heat_defect_j=heat_defect,smoke_defect_kg=smoke_defect,input_energy_j=data['input_energy_j'],input_smoke_kg=data['input_smoke_kg'],energy_outflow_j=budget['energy_j']['outflow'],smoke_outflow_kg=budget['smoke_kg']['outflow'],external_work_j=data['open_domain_forcing_state']['external_work_j'],pressure_residual=result['fields']['projection_relative_residual'],divergence_s_inv=result['fields']['max_divergence_s_inv'],**probes(config,data))
