"""Exact nested coarse space, balanced SPD action and unchanged FE equations."""
import sys,json
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
from skfem import MeshTet
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor,storage_sha
from cfd_reference3d_quadratic_coarse import scalar_macro_quadratic_interpolation,macro_quadratic_interpolation,BalancedCoarseVelocity
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_bounded_condensed import fixture
LIB=ROOT/'build/c3d-cholesky/support/factor.dylib'


def system_fixture():
    parent=MeshTet.init_tensor([0.,1.3],[0.,.7],[0.,.08,.2])
    mesh=alfeld_split(parent).with_boundaries({'walls':lambda x:np.isclose(x[2],0)|np.isclose(x[2],.2)})
    return TriangleCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)


class Coarse(unittest.TestCase):
    def test_nested_quadratic_reproduction_nodal_identity_and_boundary_projection(self):
        systems=[system_fixture()]+[TriangleCondensedSystem(fixture(refined),.1,assembly_batch=7) for refined in (False,True)]
        for system in systems:
            raw,rows,positions,meta=scalar_macro_quadratic_interpolation(system)
            xyz=system.ub.doflocs[:,system.trace]
            polynomial=lambda x:np.vstack((np.ones(x.shape[1]),*x,x[0]**2,x[1]**2,x[2]**2,x[0]*x[1],x[0]*x[2],x[1]*x[2])).T
            np.testing.assert_allclose(raw@polynomial(positions),polynomial(xyz),atol=1e-13,rtol=1e-12)
            np.testing.assert_array_equal(raw[rows].toarray(),np.eye(len(rows)))
        system=systems[0];nv=len(system.retained_free)-system.nmacro;Z,metadata=macro_quadratic_interpolation(system,nv)
        self.assertEqual(np.linalg.matrix_rank(Z.toarray()),Z.shape[1])
        self.assertTrue(metadata['essential_boundary_vanishing_verified']);self.assertTrue(metadata['essential_projection_injective'])
        self.assertGreater(metadata['boundary_excluded_nodal_count'],0)
        self.assertEqual(json.loads(json.dumps(metadata))['coarse_velocity_dofs'],Z.shape[1])

    def test_dense_balanced_formula_symmetry_linearity_spd_and_exact_coarse_action(self):
        rng=np.random.default_rng(984);R=rng.normal(size=(24,24));A=R.T@R+np.eye(24)
        velocity=SymmetricTriangle(csr_matrix(np.triu(A)));Z=csr_matrix(rng.normal(size=(24,4)))
        for cycles in (1,2):
            inverse=BalancedCoarseVelocity(velocity,Z,LIB,cycles)
            P=Z.toarray();C=P@np.linalg.solve(P.T@A@P,P.T)
            S=np.column_stack([inverse.local.solve(x) for x in np.eye(24)])
            expected=C+(np.eye(24)-C@A)@S@(np.eye(24)-A@C)
            actual=np.column_stack([inverse.solve(x) for x in np.eye(24)])
            np.testing.assert_allclose(actual,expected,atol=1e-10,rtol=1e-10)
            np.testing.assert_allclose(actual,actual.T,atol=1e-10,rtol=1e-10)
            self.assertGreater(np.linalg.eigvalsh(actual).min(),0)
            np.testing.assert_allclose(actual@A@P,P,atol=1e-10,rtol=1e-10)
            x,y=rng.normal(size=(2,24));np.testing.assert_allclose(inverse.solve(x+2*y),inverse.solve(x)+2*inverse.solve(y),atol=1e-10,rtol=1e-10)
            np.testing.assert_allclose(inverse.coarse.solve(np.ones(4)),np.linalg.solve(P.T@A@P,np.ones(4)),atol=1e-10,rtol=1e-10)
            self.assertTrue(inverse.input_unchanged());inverse.close()

    def test_anisotropic_original_rhs_pressure_reconstruction_and_full_operator(self):
        system=system_fixture();C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        old=CondensedSystem(system.mesh,.1,cache_cap_bytes=0);Z,meta=macro_quadratic_interpolation(system,C.nv)
        rng=np.random.default_rng(985);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01
        z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0]);before=system.reduce_rhs(rhs)
        u,p=system.reconstruct(z,rhs);pressure=C.pressure.upper.diagonal().copy();x,y=rng.normal(size=(2,C.nv))
        inverse=BalancedCoarseVelocity(C.velocity,Z,LIB,interpolation_metadata=meta)
        bx,by=inverse.solve(x),inverse.solve(y)
        np.testing.assert_allclose(inverse.solve(x+y),bx+by,atol=1e-10,rtol=1e-10)
        self.assertAlmostEqual(float(x@by),float(y@bx),places=8);self.assertGreater(float(x@bx),0)
        self.assertTrue(inverse.input_unchanged());inverse.close()
        np.testing.assert_array_equal(system.reduce_rhs(rhs),before);uu,pp=system.reconstruct(z,rhs)
        np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p);np.testing.assert_array_equal(C.pressure.upper.diagonal(),pressure)
        np.testing.assert_array_equal(pressure,old.matrix.diagonal()[3*old.nt:])
        ub,pb,A,B=assemble_quartic(system.mesh,.1,128)
        expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)
        P=Z.toarray();expected_coarse=P.T@(C.velocity@P)
        actual_coarse=inverse.coarse_upper.toarray();actual_coarse+=actual_coarse.T-np.diag(np.diag(actual_coarse))
        np.testing.assert_allclose(actual_coarse,expected_coarse,atol=1e-10,rtol=1e-10)

    def test_rank_failure_partial_cleanup_and_invalid_closed_rhs(self):
        velocity=SymmetricTriangle(csr_matrix(np.eye(6)));Z=csr_matrix(np.eye(6)[:,:2]);completed=[]
        def factor(*args):
            f=SharedTriangleFactor(*args);completed.append(f);return f
        with patch('cfd_reference3d_coarse_velocity.SharedTriangleFactor',side_effect=factor):
            with patch('cfd_reference3d_coarse_velocity.RepeatedBlockCholesky',side_effect=ValueError('partial control')):
                with self.assertRaises(ValueError):BalancedCoarseVelocity(velocity,Z,LIB)
        self.assertIsNone(completed[0]._handle)
        with self.assertRaises(ValueError):BalancedCoarseVelocity(velocity,csr_matrix(np.zeros((6,2))),LIB)
        with self.assertRaises(ValueError):BalancedCoarseVelocity(velocity,csr_matrix(np.eye(6)),LIB)
        inverse=BalancedCoarseVelocity(velocity,Z,LIB)
        for rhs in (np.ones(5),np.full(6,np.nan)):
            with self.assertRaises(ValueError):inverse.solve(rhs)
        inverse.close();inverse.close()
        with self.assertRaises(ValueError):inverse.solve(np.ones(6))
        empty=TriangleCondensedSystem(fixture(),.1,fixed_boundaries=('walls',))
        with self.assertRaises(ValueError):macro_quadratic_interpolation(empty,1)

    def test_one_hundred_owned_cleanup_cycles(self):
        velocity=SymmetricTriangle(csr_matrix(np.eye(6)));Z=csr_matrix(np.eye(6)[:,:2])
        for _ in range(100):
            inverse=BalancedCoarseVelocity(velocity,Z,LIB)
            np.testing.assert_array_equal(inverse.solve(np.ones(6)),np.ones(6));inverse.close()


if __name__=='__main__':unittest.main()
