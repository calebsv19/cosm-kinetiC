"""Balanced mass/coarse pressure inverse; no physical equations or modes changed."""
import numpy as np
from scipy.linalg import cho_factor,cho_solve

def reserve(nv,np_,m=10):
    if nv<1 or np_<m or m!=10:raise ValueError('invalid coarse reservation')
    return 8*(3*np_*m+8*np_+4*nv+8*m*m)+2*1024**2

def polynomial_basis(centers,volumes,mu,length):
    if centers.shape!=(len(volumes),3) or len(volumes)<10 or np.any(volumes<=0) or mu<=0 or length<=0 or not np.all(np.isfinite(centers)):raise ValueError('invalid coarse geometry')
    xyz=2*centers/np.array([length,2.,2.])-1;x,y,z=xyz.T
    raw=np.column_stack((np.ones(len(x)),x,y,z,x*x,y*y,z*z,x*y,x*z,y*z))
    root=np.sqrt(volumes/mu);q,r=np.linalg.qr(root[:,None]*raw,mode='reduced')
    if np.min(np.abs(np.diag(r)))<np.linalg.norm(r)*1e-12:raise ValueError('rank deficient pressure polynomials')
    return q/root[:,None]

class BalancedPressure:
    def __init__(self,Z,W,mass_inverse):
        if Z.ndim!=2 or W.shape!=Z.shape or mass_inverse.shape!=(len(Z),) or np.any(mass_inverse<=0) or not all(np.all(np.isfinite(a)) for a in (Z,W,mass_inverse)):raise ValueError('invalid balanced pressure data')
        c=Z.T@W;skew=float(np.linalg.norm(c-c.T)/max(np.linalg.norm(c),1e-30))
        if skew>=1e-5:raise ValueError('coarse Schur asymmetry exceeds declared gate')
        c=(c+c.T)/2;factor=cho_factor(c,lower=True,check_finite=True)
        eig=np.linalg.eigvalsh(c)
        if eig[0]<=eig[-1]*1e-12:raise ValueError('near singular coarse pressure block')
        self.Z=Z.copy();self.W=W.copy();self.mass_inverse=mass_inverse.copy();self.factor=factor
        reproduction=float(np.linalg.norm(self.apply(W)-Z)/max(np.linalg.norm(Z),1e-30))
        if reproduction>=1e-5:raise ValueError('coarse reproduction exceeds declared gate')
        self.metadata=dict(kind='balanced_mass_quadratic_global_pressure',columns=Z.shape[1],constant_pressure_direction_retained=True,coarse_relative_skew=skew,coarse_reproduction_relative_error=reproduction,coarse_smallest_eigenvalue=float(eig[0]),coarse_largest_eigenvalue=float(eig[-1]),owned_pressure_array_bytes=sum(a.nbytes for a in (self.Z,self.W,self.mass_inverse,self.factor[0])),scope='preconditioner-only approximate Float velocity Schur columns plus unchanged condensed pressure block; complete physical pressure space retained')
    def apply(self,x):
        q=self.Z.T@x;a=cho_solve(self.factor,q,check_finite=False)
        scale=self.mass_inverse if x.ndim==1 else self.mass_inverse[:,None]
        t=scale*(x-self.W@a)
        return t+self.Z@cho_solve(self.factor,q-self.W.T@t,check_finite=False)

def build(mesh,volumes,mu,length,coupling,pressure,velocity):
    centers=mesh.p[:,np.unique(mesh.t.max(axis=0))].T
    if centers.shape!=(len(volumes),3):raise ValueError('macro centers/pressure numbering mismatch')
    Z=polynomial_basis(centers,volumes,mu,length);W=np.empty_like(Z)
    for j in range(Z.shape[1]):W[:,j]=coupling.T@velocity(coupling@Z[:,j])-pressure@Z[:,j]
    obj=BalancedPressure(Z,W,mu/volumes)
    obj.metadata.update(polynomial_mass_orthogonality_error=float(np.linalg.norm(Z.T@((volumes/mu)[:,None]*Z)-np.eye(10))),coarse_reservation_bytes=reserve(coupling.shape[0],len(volumes)))
    return obj
