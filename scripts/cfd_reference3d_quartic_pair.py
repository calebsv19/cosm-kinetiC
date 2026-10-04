"""P4/DG-P3 Stokes forms and pressure mass; identical continuous PDE and boundaries."""
import numpy as np
import math
from functools import lru_cache
from skfem.quadrature import get_quadrature_tet
from scipy.sparse import csr_matrix
from skfem import Basis,ElementDG,BilinearForm,asm
from skfem.models.poisson import laplace
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_p4 import ElementTetP4,require_sorted
from cfd_reference3d_chunked import cell_basis


@lru_cache(maxsize=1)
def quartic_quadrature():
    # The library's order-6 rule is not exact for these degree-6 products.
    X,W=get_quadrature_tet(7)
    for i in range(7):
        for j in range(7-i):
            for k in range(7-i-j):
                exact=math.factorial(i)*math.factorial(j)*math.factorial(k)/math.factorial(i+j+k+3)
                assert abs(float(np.sum(X[0]**i*X[1]**j*X[2]**k*W))-exact)<1e-13
    return X,W


def assemble_quartic(mesh,mu,chunk_size=512):
    require_sorted(mesh);assert 1<=chunk_size<=2048
    ub=Basis(mesh,ElementTetP4(),quadrature=quartic_quadrature(),elements=np.array([0]))
    pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=ub.quadrature,elements=np.array([0]))
    A=csr_matrix((ub.N,ub.N));blocks=[csr_matrix((pb.N,ub.N)) for _ in range(3)]
    for begin in range(0,mesh.nelements,chunk_size):
        cells=np.arange(begin,min(begin+chunk_size,mesh.nelements));u,p=cell_basis(ub,cells),cell_basis(pb,cells)
        A=A+mu*asm(laplace,u)
        for axis in range(3):
            @BilinearForm
            def divergence(v,q,w):return -v.grad[axis]*q
            blocks[axis]=blocks[axis]+asm(divergence,u,p)
    return ub,pb,A,blocks


class CubicPressureMass:
    def __init__(self,pb,mu):
        shape=np.array([entry[0][0] for entry in pb.basis])
        self.reference=np.einsum('iq,jq,q->ij',shape,shape,pb.W)
        eigen=np.linalg.eigvalsh(self.reference)
        if eigen[0]<=eigen[-1]*1e-12:raise ValueError('under-integrated or singular cubic pressure mass')
        self.inverse=np.linalg.inv(self.reference);self.determinant=np.abs(pb.mesh.mapping().detA);self.mu=mu
        self.block_size=len(shape);assert self.block_size==20
    def solve(self,x):return ((self.inverse@x.reshape(-1,self.block_size).T)/self.determinant[None]).T.ravel()
    def apply(self,x):return ((self.reference@x.reshape(-1,self.block_size).T)*self.determinant[None]).T.ravel()
    def precondition(self,x):return self.mu*self.solve(x)
    def norm(self,x):return float(np.sqrt(max(float(x@self.apply(x)),0)))
    def dual_norm(self,x):return float(np.sqrt(max(float(x@self.solve(x)),0)))
