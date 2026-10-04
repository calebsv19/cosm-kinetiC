"""Complete cubic global PRESSURE span; original equations and full modes unchanged."""
import numpy as np
from cfd_reference3d_pressure_coarse import BalancedPressure
EXPONENTS=((0,0,0),(1,0,0),(0,1,0),(0,0,1),(2,0,0),(0,2,0),(0,0,2),(1,1,0),(1,0,1),(0,1,1),
           (3,0,0),(0,3,0),(0,0,3),(2,1,0),(2,0,1),(1,2,0),(0,2,1),(1,0,2),(0,1,2),(1,1,1))

def reserve(nv,np_,m=20):
    if type(nv) is not int or type(np_) is not int or nv<1 or np_<20 or m!=20:raise ValueError('invalid cubic pressure reservation')
    return 8*(3*np_*20+8*np_+4*nv+8*20**2)+2*1024**2

def polynomial_basis(centers,volumes,mu,length):
    if not isinstance(centers,np.ndarray) or not isinstance(volumes,np.ndarray) or volumes.ndim!=1 or centers.shape!=(len(volumes),3) or len(volumes)<20 or not all(np.all(np.isfinite(a)) for a in (centers,volumes)) or np.any(volumes<=0) or not np.isfinite(mu) or mu<=0 or not np.isfinite(length) or length<=0:raise ValueError('invalid cubic pressure geometry')
    xyz=2*centers/np.array([length,2.,2.])-1
    raw=np.column_stack([np.prod(xyz**np.array(e),axis=1) for e in EXPONENTS])
    root=np.sqrt(volumes/mu);q,r=np.linalg.qr(root[:,None]*raw,mode='reduced')
    if not np.all(np.isfinite(r)) or np.min(np.abs(np.diag(r)))<np.linalg.norm(r)*1e-12:raise ValueError('rank deficient cubic pressure polynomials')
    return q/root[:,None]

def build(mesh,volumes,mu,length,coupling,pressure,velocity):
    centers=mesh.p[:,np.unique(mesh.t.max(axis=0))].T
    if centers.shape!=(len(volumes),3):raise ValueError('macro centers/pressure numbering mismatch')
    Z=polynomial_basis(centers,volumes,mu,length);W=np.empty_like(Z)
    for j in range(20):W[:,j]=coupling.T@velocity(coupling@Z[:,j])-pressure@Z[:,j]
    obj=BalancedPressure(Z,W,mu/volumes)
    obj.metadata.update(kind='balanced_mass_cubic_global_pressure',polynomial_degree=3,complete_cubic_span=True,exponents=[list(e) for e in EXPONENTS],polynomial_mass_orthogonality_error=float(np.linalg.norm(Z.T@((volumes/mu)[:,None]*Z)-np.eye(20))),coarse_reservation_bytes=reserve(coupling.shape[0],len(volumes)))
    return obj
