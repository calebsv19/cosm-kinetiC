"""Offline gust sensitivity: native MAC sampling, conservative restriction, no solver edits."""
import itertools
import numpy as np
from .flow_spatial_fields import dense,restrict_faces,pack,restrict_extensive,centered


def read_fields(path,n):
    v=np.fromfile(path,dtype='<f8');count=n**3
    if len(v)!=5*count+3*n*n or not np.isfinite(v).all() or (v[:2*count]<0).any():
        raise ValueError('finite nonnegative native binary fields')
    return {'energy_j':v[:count],'smoke_kg':v[count:2*count],'face_velocity_m_s':v[2*count:]}


def points():
    return np.asarray([[.125+x*.046875,.125+y*.046875,.125] for y in range(17) for x in range(17)])


def sample(v,n,positions):
    """Vectorized interior MAC trilinear sampling; caller verifies finite fields once.
    Matches the existing sample_mac convention, including lateral endpoint faces.
    Ground ghost sampling is excluded from this diagnostic's interior point set.
    """
    p=np.asarray(positions,dtype=float)
    if p.ndim!=2 or p.shape[1]!=3 or not np.isfinite(p).all() or (p<0).any() or (p>1).any():
        raise ValueError('physical diagnostic points in unit domain')
    fields=dense(v,n);result=np.empty_like(p)
    for axis,f in enumerate(fields):
        offset=np.full(3,.5);offset[axis]=0.;coordinates=p*n-offset;lo=np.floor(coordinates).astype(int);weights=coordinates-lo;values=np.zeros(len(p))
        if (lo[:,2]<0).any() or (lo[:,2]+1>=f.shape[0]).any():raise ValueError('interior vertical samples required')
        for corner in itertools.product((0,1),repeat=3):
            q=lo+corner;q[:,0]=np.clip(q[:,0],0,f.shape[2]-1);q[:,1]=np.clip(q[:,1],0,f.shape[1]-1)
            weight=np.prod(np.where(np.asarray(corner),weights,1-weights),axis=1)
            values+=f[q[:,2],q[:,1],q[:,0]]*weight
        result[:,axis]=values
    return result


def norm(a,b):
    delta=float(np.linalg.norm(a-b));scale=float(np.linalg.norm(b))
    return {'relative_l2':delta/scale if scale else None,'rms_difference':float(np.sqrt(np.mean((a-b)**2))),'reference_rms':float(np.sqrt(np.mean(b*b)))}


def restricted(fields,fine,coarse):
    if fine<coarse or fine%coarse:raise ValueError('nested grids')
    result=fields
    while fine>coarse:
        n=fine//2;result={k:restrict_extensive(result[k],n) for k in ('energy_j','smoke_kg')}|{'face_velocity_m_s':pack(restrict_faces(result['face_velocity_m_s'],n))};fine=n
    return result


def compare(a,b,na,nb):
    rb=restricted(b,nb,na);va=centered(dense(a['face_velocity_m_s'],na));vb=centered(dense(rb['face_velocity_m_s'],na));z=(np.arange(na)+.5)/na
    bands=[]
    for lo,hi in ((0,.125),(.125,.25),(.25,.5),(.5,1.)):
        mask=(z>=lo)&(z<hi);bands.append({'z_m':[lo,hi],'velocity':norm(va[:,mask],vb[:,mask])})
    scalar={}
    for k in ('energy_j','smoke_kg'):
        scale=float(np.sum(b[k]));scalar[k]={'relative_l1':float(np.sum(np.abs(a[k]-rb[k])))/scale if scale else None,'stored_relative_difference':float((np.sum(a[k])-scale)/scale) if scale else None}
    return {'velocity_common_grid':norm(a['face_velocity_m_s'],rb['face_velocity_m_s']),
            'fire_plane_wind':norm(sample(a['face_velocity_m_s'],na,points()),sample(b['face_velocity_m_s'],nb,points())),
            'height_bands':bands,'scalar':scalar}


def observations(fields,n):
    v=centered(dense(fields['face_velocity_m_s'],n));p=sample(fields['face_velocity_m_s'],n,points());d=1/n
    wx=np.gradient(v[2],d,axis=1)-np.gradient(v[1],d,axis=0)
    wy=np.gradient(v[0],d,axis=0)-np.gradient(v[2],d,axis=2)
    wz=np.gradient(v[1],d,axis=2)-np.gradient(v[0],d,axis=1)
    return {'fire_mean_wind_x_m_s':float(p[:,0].mean()),'fire_rms_wind_m_s':float(np.sqrt(np.mean(p*p))),
            'kinetic_energy_j':float(.5*1.2*np.sum(v*v)/n**3),
            'vorticity_rms_s_inverse':float(np.sqrt(np.mean(wx*wx+wy*wy+wz*wz))),
            'energy_j':float(np.sum(fields['energy_j'])),'smoke_kg':float(np.sum(fields['smoke_kg']))}


def wake_observation(fields,n):
    """Fixed physical downstream ROI; qualitative diagnostic, not wake accuracy."""
    v=centered(dense(fields['face_velocity_m_s'],n));q=(np.arange(n)+.5)/n
    z,y,x=np.meshgrid(q,q,q,indexing='ij');mask=(x>=.625)&(x<.875)&(y>=.375)&(y<.625)&(z>=.375)&(z<.625)
    u=v[0][mask]
    return {'region_m':[[.625,.875],[.375,.625],[.375,.625]],'mean_u_x_m_s':float(u.mean()),'reverse_volume_fraction':float(np.mean(u<0)),'velocity_rms_m_s':float(np.sqrt(np.mean(v[:,mask]**2)))}
