"""Native-field observations using the actual low obstacle and both wake sides.

Regions stay fixed in physical space during reversal. Signed X flow is reported
without relabeling a region when the external target changes. Qualitative wake
metrics do not establish grid convergence or a resolved turbulence model.
"""
import math
from surface_sources.growth_fire_v1 import require
from .velocity import sample_mac


def regions(config):
    f=config['fluid'];box=f['solid_box'];lo=box['lower_m'];hi=box['upper_m']
    width=hi[0]-lo[0]
    result={'x_min_wake':[[lo[0]-width,lo[0]],[lo[1],hi[1]],[lo[2],hi[2]]],
            'x_max_wake':[[hi[0],hi[0]+width],[lo[1],hi[1]],[lo[2],hi[2]]]}
    require(all(0<=a<b<=length for bounds in result.values()
                for (a,b),length in zip(bounds,f['length_m'])),'physical wake region inside domain')
    return result


def probes(config):
    f=config['fluid'];box=f['solid_box'];lo=box['lower_m'];hi=box['upper_m'];source=config['source']
    middle=[(a+b)/2 for a,b in zip(lo,hi)];gap=(hi[0]-lo[0])/4
    return {'source_return':[source['origin_m'][0]+(source['width']-1)*source['cell_size_m']/2,
                source['origin_m'][1]+(source['height']-1)*source['cell_size_m']/2,
                config['return_channel']['sample_height_m']],
            'x_min_side':[lo[0]-gap,middle[1],middle[2]],
            'x_max_side':[hi[0]+gap,middle[1],middle[2]],
            'above_box':[middle[0],middle[1],hi[2]+gap],
            'under_box':[middle[0],middle[1],lo[2]/2]}


def observe(fields,config):
    import numpy as np
    from .flow_spatial_fields import dense,centered
    f=config['fluid'];n=f['grid'][0]
    require(f['grid']==[n]*3 and n in (16,32,64,80,128),'bounded cubic weather diagnostic')
    velocity=np.asarray(fields['face_velocity_m_s'],dtype=float)
    require(velocity.size==3*n**3+3*n*n and np.isfinite(velocity).all(),'finite native open MAC field')
    v=centered(dense(velocity,n));dx,dy,dz=[length/n for length in f['length_m']]
    omega=np.asarray([np.gradient(v[2],dy,axis=1)-np.gradient(v[1],dz,axis=0),
                      np.gradient(v[0],dz,axis=0)-np.gradient(v[2],dx,axis=2),
                      np.gradient(v[1],dx,axis=2)-np.gradient(v[0],dy,axis=1)])
    scalar={key:np.asarray(fields[key],dtype=float) for key in ('energy_j','smoke_kg')}
    require(all(a.size==n**3 and np.isfinite(a).all() and (a>=0).all() for a in scalar.values()),
            'finite nonnegative native extensive fields')
    z,y,x=np.meshgrid((np.arange(n)+.5)*dz,(np.arange(n)+.5)*dy,(np.arange(n)+.5)*dx,indexing='ij')
    coordinates=(x,y,z);result={}
    for name,bounds in regions(config).items():
        mask=np.ones((n,n,n),dtype=bool)
        for coordinate,(a,b) in zip(coordinates,bounds):mask&=(coordinate>=a)&(coordinate<b)
        require(mask.any(),'nonempty resolved wake observation')
        u=v[0][mask]
        result[name]={'bounds_m':bounds,'cells':int(mask.sum()),'mean_u_x_m_s':float(u.mean()),
            'min_u_x_m_s':float(u.min()),'max_u_x_m_s':float(u.max()),
            'negative_x_volume_fraction':float(np.mean(u<0)),
            'speed_rms_m_s':float(np.sqrt(np.mean(np.sum(v[:,mask]**2,axis=0)))),
            'vorticity_rms_s_inv':float(np.sqrt(np.mean(np.sum(omega[:,mask]**2,axis=0)))),
            **{key:float(a.reshape(n,n,n)[mask].sum()) for key,a in scalar.items()}}
    points=probes(config)
    result['probes']={name:{'position_m':point,'velocity_m_s':sample_mac(fields['face_velocity_m_s'],
                      f['grid'],f['length_m'],point,solid_floor=True,lateral_open=True)}
                      for name,point in points.items()}
    return result
