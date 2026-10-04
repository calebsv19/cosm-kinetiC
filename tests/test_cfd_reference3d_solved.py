"""Solved polynomial Stokes verification of reference pressure and flat-wall stress."""
import json
import sys
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse import bmat, hstack, kron, eye
from scipy.sparse.linalg import spsolve
from skfem import MeshTet, ElementTetP2, ElementTetP1, Basis, FacetBasis, BilinearForm, LinearForm, asm
from skfem.models.poisson import laplace

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))


def solve(axis, n, datum=0):
    lengths=np.array([1.,1.5,2.]);tangent=(axis+1)%3;mu=.1
    mesh=MeshTet.init_tensor(*[np.linspace(0,L,n+1) for L in lengths])
    mesh=mesh.with_boundaries({'wall':lambda x:np.isclose(x[axis],0)})
    ub,pb=Basis(mesh,ElementTetP2(),intorder=2),Basis(mesh,ElementTetP1(),intorder=2)
    A=mu*asm(laplace,ub);B=[]
    for a in range(3):
        @BilinearForm
        def divergence(u,v,w):return -u.grad[a]*v
        B.append(asm(divergence,ub,pb))
    K=bmat([[kron(eye(3),A,format='csr'),hstack(B,format='csr').T],
            [hstack(B,format='csr'),None]],format='csr')
    coefficients=np.array([.003,-.002,.004]);force=coefficients.copy();force[tangent]-=2*mu*.07
    @LinearForm
    def constant(v,w):return v
    integral=asm(constant,ub)
    rhs=np.r_[np.concatenate([force[a]*integral for a in range(3)]),np.zeros(pb.N)]
    x=ub.doflocs;exact_u=np.zeros((3,ub.N));exact_u[tangent]=.3*x[axis]+.07*x[axis]**2
    exact_p=.02+datum+coefficients@pb.doflocs
    exact=np.r_[exact_u.ravel(),exact_p]
    boundary=ub.get_dofs().all();fixed=np.r_[np.concatenate([boundary+a*ub.N for a in range(3)]),3*ub.N]
    free=np.setdiff1d(np.arange(K.shape[0]),fixed)
    solution=exact.copy();solution[free]=spsolve(K[free][:,free],rhs[free]-K[free][:,fixed]@exact[fixed])
    residual=np.linalg.norm((K@solution-rhs)[free])/max(np.linalg.norm(rhs[free]),1e-30)
    u=solution[:3*ub.N].reshape(3,-1);p=solution[3*ub.N:]
    fb=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['wall'],intorder=4)
    fp=FacetBasis(mesh,pb.elem,facets=mesh.boundaries['wall'],quadrature=fb.quadrature)
    gradient=np.stack([fb.interpolate(v).grad for v in u]);pressure=fp.interpolate(p)
    pressure_load=np.sum(pressure*fb.normals*fb.dx,axis=(1,2))
    viscous_load=-mu*np.sum(np.einsum('ij...,j...->i...',gradient+gradient.swapaxes(0,1),fb.normals)*fb.dx,axis=(1,2))
    area=float(np.prod(np.delete(lengths,axis)));center=lengths/2;center[axis]=0
    exact_pressure=np.zeros(3);exact_pressure[axis]=-(.02+datum+coefficients@center)*area
    exact_viscous=np.zeros(3);exact_viscous[tangent]=mu*.3*area
    divergence=np.einsum('ii...->...',gradient)
    return {'axis':axis,'n':n,'datum_pa':datum,'true_residual':float(residual),
            'velocity_error':float(np.max(np.abs(u-exact_u))),
            'pressure_error_pa':float(np.max(np.abs(p-exact_p))),
            'velocity_relative_error':float(np.max(np.abs(u-exact_u))/np.max(np.abs(exact_u))),
            'pressure_relative_error':float(np.max(np.abs(p-exact_p))/np.max(np.abs(exact_p))),
            'pressure_traction_relative_error':float(np.max(np.abs(pressure_load-exact_pressure))/np.max(np.abs(exact_pressure))),
            'viscous_traction_relative_error':float(np.max(np.abs(viscous_load-exact_viscous))/np.max(np.abs(exact_viscous))),
            'pressure_traction_error_n':float(np.max(np.abs(pressure_load-exact_pressure))),
            'viscous_traction_error_n':float(np.max(np.abs(viscous_load-exact_viscous))),
            'boundary_divergence_max_s_inv':float(np.max(np.abs(divergence)))}


class SolvedReference(unittest.TestCase):
    def test_quadratic_stokes_all_axes_and_gauge(self):
        self.rows=[]
        for axis in range(3):
            for n in (2,4):
                for datum in (0,.31):
                    row=solve(axis,n,datum);self.rows.append(row)
                    for key in ('true_residual','velocity_error','pressure_error_pa','pressure_traction_error_n',
                                'viscous_traction_error_n','boundary_divergence_max_s_inv','velocity_relative_error',
                                'pressure_relative_error','pressure_traction_relative_error','viscous_traction_relative_error'):
                        self.assertLess(row[key],1e-8,(key,row))
        print(json.dumps({'passed':True,'records':self.rows,'scope':'solved P2/P1 representable Stokes, no-slip on the selected flat wall; exact Dirichlet velocity on other faces and one pressure gauge; not cube corner accuracy'}))

if __name__=='__main__':unittest.main()
