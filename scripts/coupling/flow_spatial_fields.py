"""Offline MAC area restriction and spatial localization, using NumPy.
Native fields are X-fast doubles. No solver, gate or runtime behavior changes.
"""
import numpy as np

def dense(v,n):
    c=n**3;p=n*n
    if len(v)!=3*c+3*p or not np.isfinite(v).all():raise ValueError('finite MAC dimensions')
    return [np.concatenate((v[:c].reshape(n,n,n),v[3*c+p:3*c+2*p].reshape(n,n,1)),axis=2),
            np.concatenate((v[c:2*c].reshape(n,n,n),v[3*c+2*p:].reshape(n,1,n)),axis=1),
            np.concatenate((v[2*c:3*c].reshape(n,n,n),v[3*c:3*c+p].reshape(1,n,n)),axis=0)]

def restrict_faces(v,n):
    f=dense(v,2*n)
    return [f[0][:,:,::2].reshape(n,2,n,2,n+1).mean(axis=(1,3)),
            f[1][:,::2,:].reshape(n,2,n+1,n,2).mean(axis=(1,4)),
            f[2][::2,:,:].reshape(n+1,n,2,n,2).mean(axis=(2,4))]

def pack(f):
    return np.concatenate([f[0][:,:,:-1].ravel(),f[1][:,:-1,:].ravel(),f[2][:-1,:,:].ravel(),
                           f[2][-1,:,:].ravel(),f[0][:,:,-1].ravel(),f[1][:,-1,:].ravel()])

def centered(f):
    return np.stack([(f[0][:,:,:-1]+f[0][:,:,1:])/2,
                     (f[1][:,:-1,:]+f[1][:,1:,:])/2,
                     (f[2][:-1,:,:]+f[2][1:,:,:])/2])

def restrict_extensive(v,n):
    return np.asarray(v).reshape(n,2,n,2,n,2).sum(axis=(1,3,5)).ravel()

def relative(a,b):
    scale=float(np.linalg.norm(b));delta=float(np.linalg.norm(a-b))
    return dict(relative_l2=delta/scale if scale else None,absolute_l2=delta,fine_l2=scale,
                difference_rms_m_s=float(np.sqrt(np.mean((a-b)**2))),fine_rms_m_s=float(np.sqrt(np.mean(b*b))))

def localize(a,b,n):
    ca=centered(dense(a,n));cb=centered(restrict_faces(b,n))
    roi=(slice(None),slice(0,3*n//8),slice(n//4,3*n//4),slice(n//4,3*n//4))
    bands=[]
    for lo,hi in ((0,.125),(.125,.25),(.25,.375)):
        q=(slice(None),slice(round(lo*n),round(hi*n)),slice(n//4,3*n//4),slice(n//4,3*n//4))
        bands.append(dict(z_m=[lo,hi],velocity=relative(ca[q],cb[q]),
                          components={key:relative(ca[q][i],cb[q][i]) for i,key in enumerate('xyz')}))
    return dict(heat_roi_velocity=relative(ca[roi],cb[roi]),height_bands=bands,
                interpretation='Fine normal faces area-averaged before comparison; cell-centered velocities average opposite faces. Final-state sensitivity is not causal attribution.')
