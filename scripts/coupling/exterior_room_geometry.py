"""Independent physical-box oracle and interior/exterior inventories for CP1."""
import math
from functools import lru_cache
from surface_sources.growth_fire_v1 import keys,require
from .open_boundaries import face
SCHEMA='physics_sim_exterior_room/v1'

def parameters(config):
    g=config['rig_geometry'];keys(g,('schema','window_profile','exterior_profile',*(('computation_profile',) if 'computation_profile' in g else ())))
    if 'computation_profile' in g:
        from .exterior_work30 import validate
        validate(config)
    require(g['schema']==SCHEMA and g['window_profile'] in ('small','large') and g['exterior_profile'] in ('near','far'),'exterior room typed profile')
    length=2. if g['exterior_profile']=='far' else 1.5;n=config['grid'][0]
    require(config['grid']==[n]*3 and n in ((32,64) if length==2 else (24,48)) and config['length_m']==[length]*3,'exterior domain/grid')
    require(config['momentum_dt_s']==.005 and config['boundary_policy']['horizontal']=='ambient_open_xy' and config['boundary_policy']['vertical']=='solid_bottom_open_top','exterior boundary/timestep')
    require(all(k['strength']==0 for k in config['open_domain_forcing']['knots']),'no imposed wind')
    return n,length,(length-1)/2,((.25,.75) if g['window_profile']=='large' else (.375,.625))

def solid_boxes(config):
    _,_,o,(lo,hi)=parameters(config)
    return [('west',[0,o,0],[.0625,o+1,1]),('y_min',[0,o,0],[1,o+.0625,1]),('y_max',[0,o+.9375,0],[1,o+1,1]),('ceiling',[0,o,.9375],[1,o+1,1]),('east_y_lower',[.9375,o,0],[1,o+lo,1]),('east_y_upper',[.9375,o+hi,0],[1,o+1,1]),('east_z_lower',[.9375,o+lo,0],[1,o+hi,.375]),('east_z_upper',[.9375,o+lo,.875],[1,o+hi,1])]

@lru_cache(maxsize=8)
def _mask(n,length,boxes):
    h=length/n
    return tuple(int(any(all(lo[a]<=p[a]<hi[a] for a in range(3)) for _,lo,hi in boxes)) for k in range(n) for j in range(n) for i in range(n) for p in [((i+.5)*h,(j+.5)*h,(k+.5)*h)])

def topology(config):
    n,length,_,_=parameters(config)
    boxes=tuple((name,tuple(lo),tuple(hi)) for name,lo,hi in solid_boxes(config))
    # Cache only the independently derived immutable geometry, never simulation data.
    return list(_mask(n,length,boxes))

def probes(config,data):
    n,length,o,(lo,hi)=parameters(config);h=length/n;v=data['face_velocity_m_s'];x=round(1/h)
    profile=[]
    for k in range(round(.375/h),round(.875/h)):
        values=[v[face(config['grid'],0,x,j,k,True)]*h*h for j in range(round((o+lo)/h),round((o+hi)/h))]
        profile.append(dict(height_m=(k+.5)*h,outflow_m3_s=math.fsum(max(q,0) for q in values),inflow_m3_s=math.fsum(max(-q,0) for q in values)))
    out=math.fsum(q['outflow_m3_s'] for q in profile);incoming=math.fsum(q['inflow_m3_s'] for q in profile)
    return dict(window_area_m2=(hi-lo)*.5,window_volume_rate_m3_s=out-incoming,window_outflow_m3_s=out,window_inflow_m3_s=incoming,window_height_profile=profile)

def observations(config,result):
    n,length,o,_=parameters(config);h=length/n;data=result['state']['data'];e=data['energy_j'];s=data['smoke_kg'];mask=topology(config)
    room=[q for q in range(n**3) if (q%n+.5)*h<1 and o<=(q//n%n+.5)*h<o+1 and (q//(n*n)+.5)*h<1 and not mask[q]]
    room_smoke=math.fsum(s[q] for q in room);room_heat=math.fsum(e[q] for q in room);total_smoke=math.fsum(s);total_heat=math.fsum(e)
    upper=math.fsum(s[q] for q in room if (q//(n*n)+.5)*h>=.75)
    budget=result['budgets'];capacity=config['properties']['density_kg_m3']*config['properties']['heat_capacity_j_kg_k']*h**3
    heat_defect=total_heat+budget['energy_j']['outflow']-budget['energy_j']['inflow']-data['initial_energy_j']-data['input_energy_j']
    smoke_defect=total_smoke+budget['smoke_kg']['outflow']-budget['smoke_kg']['inflow']-data['initial_smoke_kg']-data['input_smoke_kg']
    require(abs(heat_defect)<1e-8 and abs(smoke_defect)<1e-14 and data['open_domain_forcing_state']['external_work_j']==0,'exterior source/boundary ledger and no imposed work')
    return dict(time_s=data['time_s'],gas_steps=data['steps'],room_smoke_kg=room_smoke,exterior_smoke_kg=total_smoke-room_smoke,domain_smoke_kg=total_smoke,room_energy_j=room_heat,exterior_energy_j=total_heat-room_heat,domain_energy_j=total_heat,net_room_smoke_transfer_kg=data['input_smoke_kg']-room_smoke,net_room_heat_transfer_j=data['input_energy_j']-room_heat,room_transfer_definition='net transfer inferred from room inventory and sole-room-source; includes return flow',outer_smoke_outflow_kg=budget['smoke_kg']['outflow'],outer_smoke_inflow_kg=budget['smoke_kg']['inflow'],outer_energy_outflow_j=budget['energy_j']['outflow'],outer_energy_inflow_j=budget['energy_j']['inflow'],upper_quarter_smoke_fraction=upper/room_smoke if room_smoke else 0,smoke_centroid_z_m=math.fsum(s[q]*(q//(n*n)+.5)*h for q in room)/room_smoke if room_smoke else None,peak_temperature_k=300+max(e)/capacity,input_energy_j=data['input_energy_j'],input_smoke_kg=data['input_smoke_kg'],heat_defect_j=heat_defect,smoke_defect_kg=smoke_defect,divergence_s_inv=result['fields']['max_divergence_s_inv'],pressure_residual=result['fields']['projection_relative_residual'],external_work_j=data['open_domain_forcing_state']['external_work_j'],**probes(config,data))
