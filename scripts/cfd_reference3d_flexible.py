"""Bounded float64 flexible right-preconditioned Arnoldi; true residual is authority."""
import numpy as np


def basis_reservation(n,restart):
    return int(8*((2*restart+1)*n+2*(restart+1)*restart+5*restart+1)+8*8*n)


def flexible_gmres(A,b,precondition,target=1e-10,maxiter=3000,restart=60,metric=None,callback=None):
    b=np.asarray(b,dtype=np.float64)
    if b.ndim!=1 or not len(b) or not np.all(np.isfinite(b)) or not 0<target<1 or not isinstance(maxiter,int) or not 1<=maxiter<=3000 or not isinstance(restart,int) or not 1<=restart<=60:raise ValueError('invalid flexible iteration request')
    n=len(b);scale=float(np.linalg.norm(b));x=np.zeros(n)
    if scale==0:return x,0,dict(iterations=0,restarts=0,basis_array_bytes=0,final_true_metric=0.,scope='zero original RHS')
    if metric is None:metric=lambda r:float(np.linalg.norm(r)/scale)
    V=np.empty((restart+1,n));Z=np.empty((restart,n));H=np.zeros((restart+1,restart));R=H.copy();g=np.zeros(restart+1);cs=np.zeros(restart);sn=cs.copy()
    allocated=sum(a.nbytes for a in (V,Z,H,R,g,cs,sn));iterations=0;restarts=0;last=float('inf')
    def action(v):
        result=np.asarray(A@v,dtype=float)
        if result.shape!=(n,) or not np.all(np.isfinite(result)):raise ValueError('invalid physical operator action')
        return result
    def check(residual):
        value=float(metric(residual))
        if not np.isfinite(value) or value<0:raise ValueError('invalid true residual metric')
        return value
    while iterations<maxiter:
        residual=b-action(x);last=check(residual)
        if last<=target:break
        beta=float(np.linalg.norm(residual));V[0]=residual/beta;H.fill(0.);R.fill(0.);g.fill(0.);g[0]=beta;restarts+=1
        end=min(restart,maxiter-iterations)
        for j in range(end):
            z=np.asarray(precondition(V[j]),dtype=float)
            if z.shape!=(n,) or not np.all(np.isfinite(z)):raise ValueError('invalid approximate preconditioner action')
            Z[j]=z;w=action(z)
            # Two passes retain orthogonality when preconditioner arithmetic varies.
            for _ in range(2):
                coefficients=V[:j+1]@w;H[:j+1,j]+=coefficients;w-=coefficients@V[:j+1]
            H[j+1,j]=np.linalg.norm(w);breakdown=H[j+1,j]<=np.finfo(float).eps*max(np.linalg.norm(H[:j+1,j]),1.)
            if not breakdown:V[j+1]=w/H[j+1,j]
            R[:j+2,j]=H[:j+2,j]
            for i in range(j):
                a,c=R[i,j],R[i+1,j];R[i,j]=cs[i]*a+sn[i]*c;R[i+1,j]=-sn[i]*a+cs[i]*c
            norm=float(np.hypot(R[j,j],R[j+1,j]));cs[j]=R[j,j]/norm if norm else 1.;sn[j]=R[j+1,j]/norm if norm else 0.
            R[j,j]=norm;R[j+1,j]=0.;g[j+1]=-sn[j]*g[j];g[j]*=cs[j];iterations+=1
            if iterations%10==0 or j==end-1 or breakdown or abs(g[j+1])/scale<=target:
                # Least squares on original Hessenberg handles rank loss honestly.
                y=np.linalg.lstsq(H[:j+2,:j+1],np.r_[beta,np.zeros(j+1)],rcond=None)[0]
                candidate=x+y@Z[:j+1];actual=b-action(candidate);last=check(actual)
                if callback:callback(candidate,iterations,last)
                if last<=target or breakdown or j==end-1:
                    x=candidate;break
        if last<=target or breakdown:break
    diagnostics=dict(iterations=iterations,restarts=restarts,restart=restart,basis_array_bytes=allocated,basis_reservation_bytes=basis_reservation(n,restart),final_true_metric=last,scope='stored flexible right-preconditioned basis with float64 true physical residual; projected Arnoldi norm cannot certify convergence')
    return x,0 if last<=target else 1,diagnostics
