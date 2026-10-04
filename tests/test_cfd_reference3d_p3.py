"""Independent polynomial, topology and divergence support checks for cubic pair."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet, Basis, ElementDG, ElementTetP2, BilinearForm, asm
from scipy.sparse import hstack
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import ElementTetP3, alfeld_split, require_sorted


class CubicSupport(unittest.TestCase):
    def test_nodal_and_all_monomials(self):
        element = ElementTetP3()
        nodal = np.array([element.lbasis(element.doflocs.T, i)[0] for i in range(20)])
        np.testing.assert_allclose(nodal, np.eye(20), atol=2e-14)
        rng = np.random.default_rng(53)
        bary = rng.dirichlet(np.ones(4), size=29).T; x = bary[1:]
        values = np.array([element.lbasis(x, i)[0] for i in range(20)])
        gradients = np.array([element.lbasis(x, i)[1] for i in range(20)])
        for a in range(4):
            for b in range(4-a):
                for c in range(4-a-b):
                    powers = np.array([a,b,c]); nodes = np.prod(element.doflocs**powers, axis=1)
                    exact = np.prod(x**powers[:,None], axis=0)
                    derivative = np.zeros_like(x)
                    for k in range(3):
                        if powers[k]:
                            exponent = powers.copy(); exponent[k] -= 1
                            derivative[k] = powers[k]*np.prod(x**exponent[:,None], axis=0)
                    np.testing.assert_allclose(nodes@values, exact, atol=3e-14)
                    np.testing.assert_allclose(np.einsum('i,ijq->jq',nodes,gradients), derivative, atol=8e-14)

    def test_split_global_polynomial_and_boundary(self):
        macro = MeshTet.init_tensor(*[np.linspace(0,L,3) for L in (1,1.5,2)])
        mesh = alfeld_split(macro); require_sorted(mesh)
        self.assertEqual(mesh.nelements, 4*macro.nelements)
        self.assertEqual(len(mesh.boundary_facets()), len(macro.boundary_facets()))
        self.assertAlmostEqual(mesh.mapping().detA.__abs__().sum()/6, 3., places=13)
        ub = Basis(mesh, ElementTetP3(), intorder=6)
        # Check each local mapping of every global DOF. Wrong edge orientation
        # can pass a single reference tetrahedron and fail this conformity test.
        local = mesh.mapping().F(ub.elem.doflocs.T)
        np.testing.assert_allclose(local, ub.doflocs[:,ub.element_dofs].transpose(0,2,1), atol=1e-14)
        x = ub.doflocs; coefficients = x[0]*x[1]*x[2] + x[0]**3 - 2*x[1]**2
        interpolated = ub.interpolate(coefficients); q = ub.global_coordinates()
        np.testing.assert_allclose(interpolated, q[0]*q[1]*q[2]+q[0]**3-2*q[1]**2, atol=2e-14)
        np.testing.assert_allclose(interpolated.grad, np.array([q[1]*q[2]+3*q[0]**2,q[0]*q[2]-4*q[1],q[0]*q[1]]), atol=6e-14)

    def test_local_divergence_rank(self):
        mesh = alfeld_split(MeshTet())
        ub = Basis(mesh,ElementTetP3(),intorder=4)
        pb = Basis(mesh,ElementDG(ElementTetP2()),quadrature=ub.quadrature)
        fixed = ub.get_dofs().all(); free = np.setdiff1d(np.arange(ub.N),fixed)
        blocks=[]
        for axis in range(3):
            @BilinearForm
            def divergence(u,v,w): return u.grad[axis]*v
            blocks.append(asm(divergence,ub,pb)[:,free])
        singular = np.linalg.svd(hstack(blocks).toarray(),compute_uv=False)
        self.assertEqual(np.count_nonzero(singular>1e-11),pb.N-1)
        self.assertLess(singular[-1],1e-12)
        self.assertGreater(singular[-2]/singular[0],1e-4)


if __name__ == '__main__': unittest.main()
