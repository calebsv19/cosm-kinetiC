"""Independent fixed-rig mask and topology checks; physical bounds stay in meters."""
from collections import deque
import math
from surface_sources.growth_fire_v1 import keys,require
from .open_boundaries import face
SCHEMA='physics_sim_partition_window_rig/v1'
MODEL='open_xy_partition_window_rig/v1'


def topology(config):
    g=config['rig_geometry'];keys(g,('schema','window_open','flow_profile') if 'flow_profile' in g else ('schema','window_open'))
    if 'flow_profile' in g:require(g['flow_profile']=='broad_sweeping_shear/v1','supported explicit rig flow profile')
    require(g['schema']==SCHEMA and type(g['window_open']) is bool,'fixed rig schema')
    n=config['grid'][0]
    require(config['grid']==[n]*3 and n in (16,32) and config['length_m']==[1,1,1],'unit rig grid')
    require(config['boundary_policy']['horizontal']=='ambient_open_xy' and
            config['boundary_policy']['vertical']=='solid_bottom_open_top','rig external reservoirs and floor')
    require(not any(k in config for k in ('solid_box','wind_forcing','inert_object','capacity_profile','diagnostic_transport')),'separate rig profile')
    require('open_domain_forcing' in config,'rig explicitly selects upstream port driver')
    mask=[]
    for k in range(n):
        for j in range(n):
            for i in range(n):
                port=.25<=(j+.5)/n<.75 and .25<=(k+.5)/n<.75
                shell=(j+.5)/n<.0625 or (j+.5)/n>=.9375 or (k+.5)/n>=.9375 or (((i+.5)/n<.0625 or (i+.5)/n>=.9375) and not port)
                opening=g['window_open'] and .375<=(j+.5)/n<.625 and .25<=(k+.5)/n<.75
                partition=.5<=(i+.5)/n<.625 and not opening
                mask.append(int(shell or partition))
    return mask


def blocked(config,mask,axis,i,j,k):
    n=config['grid'];a=[i,j,k];b=list(a);b[axis]-=1
    return any(all(0<=v[q]<n[q] for q in range(3)) and mask[(v[2]*n[1]+v[1])*n[0]+v[0]] for v in (a,b))


def components(config,mask):
    n=config['grid'][0];seen=set();result=[]
    for start,v in enumerate(mask):
        if v or start in seen:continue
        queue=deque([start]);seen.add(start);count=0;reservoir=False
        while queue:
            q=queue.popleft();count+=1;i=q%n;j=q//n%n;k=q//(n*n)
            reservoir |= i in (0,n-1) or j in (0,n-1) or k==n-1
            for a in range(3):
                for sign in (-1,1):
                    p=[i,j,k];p[a]+=sign
                    if not all(0<=x<n for x in p):continue
                    other=(p[2]*n+p[1])*n+p[0]
                    if not mask[other] and other not in seen:seen.add(other);queue.append(other)
        result.append(dict(cells=count,pressure_reservoir=reservoir))
    require(all(r['pressure_reservoir'] for r in result),'unanchored fluid pressure component')
    require(len(result)==(1 if config['rig_geometry']['window_open'] else 2),'rig fluid connectivity')
    return result


def check(config,data,mask=None):
    mask=topology(config) if mask is None else mask;n=config['grid'][0]
    for key in ('energy_j','smoke_kg','pressure_pa'):
        require(all(v==0 for v,solid in zip(data[key],mask) if solid),'rig excludes scalar/pressure from solids')
    for a in range(3):
        for k in range(n+(a==2)):
            for j in range(n+(a==1)):
                for i in range(n+(a==0)):
                    if blocked(config,mask,a,i,j,k):require(data['face_velocity_m_s'][face(config['grid'],a,i,j,k,True)]==0,'rig impermeable normal face')
    return mask


def probes(config,data):
    n=config['grid'][0];vel=data['face_velocity_m_s'];area=1/n**2
    window=math.fsum(vel[face(config['grid'],0,n//2,j,k,True)]*area for k in range(n//4,3*n//4) for j in range(3*n//8,5*n//8))
    west=math.fsum(vel[face(config['grid'],0,0,j,k,True)]*area for k in range(n) for j in range(n))
    east=math.fsum(vel[face(config['grid'],0,n,j,k,True)]*area for k in range(n) for j in range(n))
    return dict(window_volume_rate_m3_s=window,west_signed_volume_rate_m3_s=west,east_signed_volume_rate_m3_s=east,
                port_imbalance_m3_s=west-east,window_area_m2=.125 if config['rig_geometry']['window_open'] else 0.)


def solid_boxes(config):
    """Physical boxes whose union is the fixed mask; floor is a boundary plane."""
    result=[('y_min',[0,0,0],[1,.0625,1]),('y_max',[0,.9375,0],[1,1,1]),('ceiling',[0,0,.9375],[1,1,1])]
    for name,lo,hi in [('west',0,.0625),('east',.9375,1)]:
        result += [(name+'_y_lower',[lo,0,0],[hi,.25,1]),(name+'_y_upper',[lo,.75,0],[hi,1,1]),
                   (name+'_z_lower',[lo,.25,0],[hi,.75,.25]),(name+'_z_upper',[lo,.25,.75],[hi,.75,1])]
    if config['rig_geometry']['window_open']:
        result += [('partition_y_lower',[.5,0,0],[.625,.375,1]),('partition_y_upper',[.5,.625,0],[.625,1,1]),
                   ('partition_z_lower',[.5,.375,0],[.625,.625,.25]),('partition_z_upper',[.5,.375,.75],[.625,.625,1])]
    else:result.append(('partition_closed',[.5,0,0],[.625,1,1]))
    return result


def render_surfaces(config,plane):
    from .playback_export import box_surfaces
    result=[]
    for name,lo,hi in solid_boxes(config):
        # These display cutaways do not alter mask/connectivity or solver walls.
        if name in ('y_min','ceiling') or name.startswith('east_'):continue
        faces=box_surfaces({'fluid':{'solid_box':dict(lower_m=lo,upper_m=hi)}},plane)
        for item in faces:item['object_id']='rig_'+name+'_'+item['object_id']
        result.extend(faces)
    return result
