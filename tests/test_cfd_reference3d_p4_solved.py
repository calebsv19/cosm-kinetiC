"""Solved quartic/cubic Stokes controls with independently specified forcing."""
import json
import sys
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import spsolve
from skfem import MeshTet, FacetBasis, LinearForm, asm
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_stokes_pair import mixed_matrix


def solve(axis,n,datum,natural):
    lengths=np.array([1.,1.5,2.]);tangent=(axis+1)%3;mu=.1
    mesh=alfeld_split(MeshTet.init_tensor(*[np.linspace(0,L,n+1) for L in lengths]))
    mesh=mesh.with_boundaries({'wall':lambda x:np.isclose(x[axis],0),
        'open':lambda x:np.isclose(x[tangent],0)|np.isclose(x[tangent],lengths[tangent]),
        'fixed':lambda x:np.any(np.isclose(x[np.arange(3)!=tangent],0)
                |np.isclose(x[np.arange(3)!=tangent],lengths[np.arange(3)!=tangent,None]),axis=0)})
    ub,pb,A,blocks=assemble_quartic(mesh,mu,64)
    from cfd_reference3d_chunked import cell_basis
    full_basis=cell_basis(ub,np.arange(mesh.nelements))
    all_u=np.arange(ub.N);K=mixed_matrix(A,blocks,all_u)
    coefficients=np.array([.003,-.002,.004])
    def pressure(x):return .02+datum+np.einsum('i,i...->...',coefficients,x)+.005*x[axis]**2+.003*x[axis]**3
    rhs=[]
    for component in range(3):
        @LinearForm
        def forcing(v,w):
            force=coefficients[component]+(.01*w.x[axis]+.009*w.x[axis]**2 if component==axis else 0)
            if component==tangent:force=force-mu*(.14+.12*w.x[axis]+.132*w.x[axis]**2)
            return force*v
        f=asm(forcing,full_basis)
        if natural:
            @LinearForm
            def traction(v,w):return -pressure(w.x)*w.n[component]*v
            f+=asm(traction,FacetBasis(mesh,ub.elem,facets=mesh.boundaries['open'],intorder=8))
        rhs.append(f)
    rhs=np.r_[np.concatenate(rhs),np.zeros(pb.N)]
    x=ub.doflocs;exact_u=np.zeros((3,ub.N));exact_u[tangent]=.3*x[axis]+.07*x[axis]**2+.02*x[axis]**3+.011*x[axis]**4
    exact_p=pressure(pb.doflocs);exact=np.r_[exact_u.ravel(),exact_p]
    boundary=ub.get_dofs('fixed').all() if natural else ub.get_dofs().all()
    fixed=np.concatenate([boundary+a*ub.N for a in range(3)])
    if not natural:fixed=np.r_[fixed,3*ub.N]
    free=np.setdiff1d(np.arange(K.shape[0]),fixed)
    result=exact.copy();result[free]=spsolve(K[free][:,free],rhs[free]-K[free][:,fixed]@exact[fixed])
    residual=np.linalg.norm((K@result-rhs)[free])/max(np.linalg.norm(rhs[free]),1e-30)
    u=result[:3*ub.N].reshape(3,-1);p=result[3*ub.N:]
    fb=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['wall'],intorder=6)
    fp=FacetBasis(mesh,pb.elem,facets=mesh.boundaries['wall'],quadrature=fb.quadrature)
    g=np.stack([fb.interpolate(v).grad for v in u]);pv=fp.interpolate(p)
    pforce=np.sum(pv*fb.normals*fb.dx,axis=(1,2))
    vforce=-mu*np.sum(np.einsum('ij...,j...->i...',g+g.swapaxes(0,1),fb.normals)*fb.dx,axis=(1,2))
    area=np.prod(np.delete(lengths,axis));center=lengths/2;center[axis]=0
    expected_p=np.zeros(3);expected_p[axis]=-(.02+datum+coefficients@center)*area
    expected_v=np.zeros(3);expected_v[tangent]=mu*.3*area
    from cfd_reference3d_chunked import cell_basis
    all_basis=cell_basis(ub,np.arange(mesh.nelements))
    volume_gradient=np.stack([all_basis.interpolate(v).grad for v in u])
    return dict(axis=axis,n=n,datum_pa=datum,natural_traction=natural,tetrahedra=mesh.nelements,
        velocity_error=float(np.max(np.abs(u-exact_u))),pressure_error=float(np.max(np.abs(p-exact_p))),
        pressure_traction_error=float(np.max(np.abs(pforce-expected_p))),
        viscous_traction_error=float(np.max(np.abs(vforce-expected_v))),true_residual=float(residual),
        wall_divergence_max=float(np.max(np.abs(np.einsum('ii...->...',g)))),
        volume_divergence_max=float(np.max(np.abs(np.einsum('ii...->...',volume_gradient)))))


class SolvedControls(unittest.TestCase):
    def test_polynomial_stokes(self):
        rows=[]
        for natural in (False,True):
            for axis in range(3):
                for n in (1,2):
                    for datum in (0,.31):
                        row=solve(axis,n,datum,natural);rows.append(row)
                        for key,value in row.items():
                            if key.endswith('_error') or key.endswith('_max') or key=='true_residual':
                                self.assertLess(value,1e-8,(key,row))
        print(json.dumps({'schema':'c3d_quartic_solved_controls_v1','cases':rows}),flush=True)


if __name__=='__main__':unittest.main()
