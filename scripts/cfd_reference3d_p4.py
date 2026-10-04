"""App-owned quartic H1 tetrahedron for higher-order independent reference tests.

Multiple edge and face DOFs use sorted global tetrahedral vertex IDs. The cubic
pressure basis is the existing P3 element wrapped in DG. No native solver dependency.
"""
import numpy as np
from skfem.element import ElementH1
from skfem.refdom import RefTet


class ElementTetP4(ElementH1):
    nodal_dofs=1
    edge_dofs=3
    facet_dofs=3
    interior_dofs=1
    maxdeg=4
    dofnames=['u']*8
    refdom=RefTet
    _alpha=np.array([np.eye(4,dtype=int)[a]*4 for a in range(4)]
        +[np.eye(4,dtype=int)[a]*(4-k)+np.eye(4,dtype=int)[b]*k for a,b in RefTet.edges for k in (1,2,3)]
        +[np.eye(4,dtype=int)[face].sum(axis=0)+np.eye(4,dtype=int)[a] for face in RefTet.facets for a in sorted(face)]
        +[np.ones(4,dtype=int)])
    doflocs=_alpha[:,1:]/4.

    def lbasis(self,X,i):
        if not 0<=i<35:self._index_error()
        bary=np.concatenate(((1-X.sum(axis=0))[None],X),axis=0)
        factors=[];derivatives=[]
        for axis,count in enumerate(self._alpha[i]):
            value=np.ones_like(bary[axis]);derivative=np.zeros_like(value)
            for j in range(count):
                derivative=(derivative*(4*bary[axis]-j)+4*value)/(j+1)
                value=value*(4*bary[axis]-j)/(j+1)
            factors.append(value);derivatives.append(derivative)
        value=np.prod(factors,axis=0)
        partial=np.array([derivatives[a]*np.prod([factors[b] for b in range(4) if b!=a],axis=0) for a in range(4)])
        return value,partial[1:]-partial[0]


def require_sorted(mesh):
    if not np.all(np.diff(mesh.t,axis=0)>0):
        raise ValueError('quartic edge/face orientation requires sorted tetrahedra')
