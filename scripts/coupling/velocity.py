"""Native double precision MAC velocity -> Fire tangent observations.

Supports periodic XYZ and native solid-floor/open-top, periodic XY models.
Vertical ghosts follow the native solver; VF3D is never numerical authority.
"""
import itertools
import math
from surface_sources.growth_fire_v1 import require,sealed


def basis_valid(u,v):
    require(len(u)==len(v)==3 and all(math.isfinite(x) for x in (*u,*v)),'finite tangent basis')
    dot=lambda a,b:math.fsum(x*y for x,y in zip(a,b))
    require(abs(dot(u,u)-1)<1e-12 and abs(dot(v,v)-1)<1e-12 and abs(dot(u,v))<1e-12,
            'orthonormal Fire tangents')


def sample_mac(velocity,grid,length,point,*,solid_floor=False,lateral_open=False):
    """Lower-face staggered convention from cfd_cartesian3d.h; periodic wrapping."""
    n=math.prod(grid);require(len(velocity)==3*n+(grid[0]*grid[1] if solid_floor else 0)+(grid[1]*grid[2]+grid[0]*grid[2] if lateral_open else 0),'native MAC velocity dimensions')
    require(all(math.isfinite(v) for v in velocity),'finite native velocity')
    require(len(point)==3 and all(0<=x<=extent for x,extent in zip(point,length)),
            'sample outside periodic domain')
    result=[]
    for component in range(3):
        coordinates=[x/(extent/count)-(0 if axis==component else .5)
                     for axis,(x,extent,count) in enumerate(zip(point,length,grid))]
        lower=[math.floor(x) for x in coordinates];weights=[x-i for x,i in zip(coordinates,lower)]
        values=[]
        for corner in itertools.product((0,1),repeat=3):
            index=[(i+c)%size for i,c,size in zip(lower,corner,grid)]
            sign=1
            if solid_floor:
                k=lower[2]+corner[2]
                if component==2:
                    if k<0:k=-k;sign=-1
                    if k>grid[2]:k=2*grid[2]-k
                else:
                    if k<0:k=-k-1;sign=-1
                    k=min(k,grid[2]-1)
                index[2]=k
            if lateral_open:
                for a in (0,1): index[a]=max(0,min(lower[a]+corner[a],grid[a]-(a!=component)))
            from .open_boundaries import face
            q=face(grid,component,*index,lateral_open)
            weight=math.prod(t if c else 1-t for t,c in zip(weights,corner))
            values.append(sign*velocity[q]*weight)
        result.append(math.fsum(values))
    return result


def observation(cp,config,request_tick,max_age_ticks,*,height_m,basis_u=(1,0,0),basis_v=(0,1,0)):
    basis_valid(basis_u,basis_v)
    require(type(request_tick) is int and type(max_age_ticks) is int and max_age_ticks>=0,'integer observation time')
    rate=cp['evidence'].get('rate')
    require(rate is not None,'an explicitly timed accepted checkpoint is required for observation')
    tick=cp['evidence']['window_ticks'][1]
    require(tick<=request_tick<=tick+max_age_ticks,'stale or future native checkpoint')
    fluid=cp['fluid'];opened=fluid['model'].startswith('open_xy_');ground=opened or fluid['model'] in ('xy_periodic_ground_open_top_projection3d_v1','xy_periodic_ground_box_open_top_projection3d_v1')
    require(ground or fluid['model']=='periodic_evolving_passive3d_v1','unsupported velocity boundary model')
    grid=config['fluid']['grid'];length=config['fluid']['length_m'];source=config['source']
    require(math.isfinite(height_m) and 0<height_m<length[2],'positive interior sample height')
    if ground:
        boundary=config['fluid']['boundary_policy']
        require(boundary['vertical']=='solid_bottom_open_top' and boundary['predictor_velocity']=='no_slip_bottom_zero_gradient_top','solid-floor sampling policy')
        require(all(v==0 for v in fluid['fields']['face_velocity_m_s'][2*math.prod(grid):2*math.prod(grid)+grid[0]*grid[1]]),'impermeable floor velocity')
    native=fluid['state']['data']['face_velocity_m_s'] if ground else fluid['state']['data']['flow_storage'][:3*math.prod(grid)]
    require(fluid['fields']['face_velocity_m_s']==native,
            'native field/checkpoint velocity identity')
    vectors=[];tangents=[]
    for y in range(source['height']):
        for x in range(source['width']):
            point=[source['origin_m'][0]+x*source['cell_size_m'],
                   source['origin_m'][1]+y*source['cell_size_m'],height_m]
            from .solid_box import point_outside
            point_outside(config['fluid'],point)
            value=sample_mac(fluid['fields']['face_velocity_m_s'],grid,length,point,solid_floor=ground,lateral_open=opened)
            vectors.append(value)
            tangents.append([math.fsum(a*b for a,b in zip(value,basis_u)),
                             math.fsum(a*b for a,b in zip(value,basis_v))])
    return sealed({'schema':'physics_sim_fire_velocity_observation/v1','units':'m/s',
        'frame':'right_handed_z_up_meters','grid_convention':('xy_periodic_solid_floor_open_top_lower_face_mac' if ground else 'periodic_lower_face_mac'),
        'boundary_model':fluid['model'],
        'joint_checkpoint_digest':cp['digest'],'binding_digest':cp['binding_digest'],
        'native_state_digest':fluid['state']['digest'],'checkpoint_tick':tick,'request_tick':request_tick,
        'ticks_per_second':rate,'max_age_ticks':max_age_ticks,'sample_height_m':height_m,
        'basis_u':list(basis_u),'basis_v':list(basis_v),'dimensions':[source['width'],source['height']],
        'native_velocity_m_s':vectors,'tangent_velocity_m_s':tangents,
        'numerical_authority':'native_f64_checkpoint','applied_to_fire':False})
