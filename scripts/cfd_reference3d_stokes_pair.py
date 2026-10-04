"""Experimental cubic/DG-quadratic Stokes assembly; no native solver dependency."""
import numpy as np
from scipy.sparse import bmat, hstack, kron, eye
from skfem import Basis, ElementDG, ElementTetP2, BilinearForm, asm
from skfem.models.poisson import laplace
from cfd_reference3d_p3 import ElementTetP3, require_sorted


def assemble_pair(mesh, mu):
    require_sorted(mesh)
    ub = Basis(mesh, ElementTetP3(), intorder=4)
    pb = Basis(mesh, ElementDG(ElementTetP2()), quadrature=ub.quadrature)
    A = mu*asm(laplace, ub)
    blocks = []
    for axis in range(3):
        @BilinearForm
        def divergence(u, v, w):
            return -u.grad[axis]*v
        blocks.append(asm(divergence, ub, pb))
    return ub, pb, A, blocks


def mixed_matrix(A, blocks, free):
    B = hstack([b[:,free] for b in blocks], format='csr')
    H = kron(eye(3), A[free][:,free], format='csr')
    return bmat([[H,B.T],[B,None]], format='csr')
