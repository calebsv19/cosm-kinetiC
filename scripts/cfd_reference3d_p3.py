"""App-owned cubic H1 tetrahedron and conforming Alfeld split for reference tests.

Edge DOFs require increasing global vertex order in every tetrahedron. Pressure
uses the library's discontinuous P2 element. This is experimental reference code.
"""
import numpy as np
from skfem import MeshTet
from skfem.element import ElementH1
from skfem.refdom import RefTet


class ElementTetP3(ElementH1):
    nodal_dofs = 1
    edge_dofs = 2
    facet_dofs = 1
    maxdeg = 3
    dofnames = ['u', 'u', 'u', 'u']
    refdom = RefTet
    _alpha = np.array(
        [np.eye(4, dtype=int)[a] * 3 for a in range(4)]
        + [np.eye(4, dtype=int)[a] * (3-k) + np.eye(4, dtype=int)[b] * k
           for a, b in RefTet.edges for k in (1, 2)]
        + [np.eye(4, dtype=int)[face].sum(axis=0) for face in RefTet.facets])
    doflocs = _alpha[:, 1:] / 3.

    def lbasis(self, X, i):
        if not 0 <= i < 20:
            self._index_error()
        bary = np.concatenate(((1-X.sum(axis=0))[None], X), axis=0)
        factors, derivatives = [], []
        for a, count in enumerate(self._alpha[i]):
            f = np.ones_like(bary[a]); df = np.zeros_like(f)
            for j in range(count):
                df = (df*(3*bary[a]-j) + 3*f)/(j+1)
                f = f*(3*bary[a]-j)/(j+1)
            factors.append(f); derivatives.append(df)
        value = np.prod(factors, axis=0)
        partial = np.array([derivatives[a] * np.prod(
            [factors[b] for b in range(4) if b != a], axis=0) for a in range(4)])
        return value, partial[1:]-partial[0]


def alfeld_split(mesh):
    """Add one interior barycenter per parent; retain every parent boundary face."""
    centers = mesh.p[:, mesh.t].mean(axis=1)
    ids = np.arange(mesh.nelements) + mesh.nvertices
    cells = np.concatenate([np.vstack((mesh.t[face], ids))
                            for face in RefTet.facets], axis=1)
    return MeshTet(np.concatenate((mesh.p, centers), axis=1),
                   np.sort(cells, axis=0))


def require_sorted(mesh):
    if not np.all(np.diff(mesh.t, axis=0) > 0):
        raise ValueError('cubic edge orientation requires sorted tetrahedra')
