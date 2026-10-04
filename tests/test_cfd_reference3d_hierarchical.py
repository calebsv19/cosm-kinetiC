"""Bijective hierarchy, exact congruence, positive coupled factors and full FE truth."""
import sys,json
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_hierarchical import HierarchicalCoordinates,macro_hierarchy,HierarchyCholesky
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_coarse_velocity import system_fixture
from test_cfd_reference3d_bounded_condensed import fixture
LIB=ROOT/'build/c3d-cholesky/support/factor.dylib'


def simple_hierarchy(n=24,nc=6):
    rng=np.random.default_rng(1102)
    Z=np.vstack((np.eye(nc),rng.normal(size=(n-nc,nc))))
    return HierarchicalCoordinates(csr_matrix(Z),np.arange(nc),[('vertex',[i]) for i in range(nc)])


def dense_transform(H):return np.column_stack([H.push(x) for x in np.eye(H.nv)])


class Hierarchy(unittest.TestCase):
    def test_macro_pivots_entity_rank_bijective_reconstruction_and_shared_orientation(self):
        for system in (system_fixture(),TriangleCondensedSystem(fixture(True),.1,assembly_batch=7)):
            nv=len(system.retained_free)-system.nmacro;H=macro_hierarchy(system,nv);P=dense_transform(H)
            self.assertEqual(np.linalg.matrix_rank(P),nv)
            x=np.random.default_rng(1103).normal(size=nv)
            np.testing.assert_allclose(H.pull(x),P.T@x,atol=1e-11,rtol=1e-11)
            np.testing.assert_allclose(H.push(np.linalg.solve(P,x)),x,atol=1e-11,rtol=1e-11)
            self.assertTrue(H.input_unchanged());self.assertTrue(H.metadata['entity_block_triangular_pivot_verified'])
            json.loads(json.dumps(H.metadata))

    def test_dense_congruence_exact_factors_ssor_majorization_symmetry_and_repeats(self):
        H=simple_hierarchy();P=dense_transform(H);rng=np.random.default_rng(1104);R=rng.normal(size=(24,24));A=R.T@R+np.eye(24)
        velocity=SymmetricTriangle(csr_matrix(np.triu(A)));K=P.T@A@P;nc=H.nc
        D=np.zeros_like(K);D[:nc,:nc]=K[:nc,:nc];D[nc:,nc:]=K[nc:,nc:]
        L=np.tril(K,-1)-np.tril(D,-1);M=(D+L)@np.linalg.solve(D,D+L.T)
        self.assertGreaterEqual(np.linalg.eigvalsh(M-K).min(),-1e-9)
        single=P@np.linalg.solve(M,P.T)
        for cycles in (1,2,4):
            inverse=HierarchyCholesky(velocity,H,LIB,cycles)
            actual=np.column_stack([inverse.solve(x) for x in np.eye(24)])
            expected=single.copy()
            for _ in range(cycles-1):expected+=single@(np.eye(24)-A@expected)
            np.testing.assert_allclose(actual,expected,atol=1e-10,rtol=1e-10)
            np.testing.assert_allclose(actual,actual.T,atol=1e-10,rtol=1e-10)
            self.assertGreater(np.linalg.eigvalsh(actual).min(),0)
            self.assertGreaterEqual(np.linalg.eigvalsh(np.linalg.inv(A)-actual).min(),-1e-10)
            np.testing.assert_allclose(inverse.coarse.solve(np.ones(nc)),np.linalg.solve(K[:nc,:nc],np.ones(nc)),atol=1e-10,rtol=1e-10)
            np.testing.assert_allclose(inverse.fine_factor.solve(np.ones(24-nc)),np.linalg.solve(K[nc:,nc:],np.ones(24-nc)),atol=1e-10,rtol=1e-10)
            self.assertTrue(inverse.input_unchanged());inverse.close()

    def test_anisotropic_original_fe_rhs_pressure_and_field_preservation(self):
        system=system_fixture();nv=len(system.retained_free)-system.nmacro;C=BlockTriangle(system.upper_matrix,nv);H=macro_hierarchy(system,nv)
        P=dense_transform(H);A=C.velocity.upper.toarray();A+=A.T-np.diag(np.diag(A));K=P.T@A@P
        inverse=HierarchyCholesky(C.velocity,H,LIB);nc=H.nc
        Ac=inverse.coarse_upper.toarray();Ac+=Ac.T-np.diag(np.diag(Ac));F=inverse.fine_upper.toarray();F+=F.T-np.diag(np.diag(F))
        np.testing.assert_allclose(np.block([[Ac,inverse.coupling.toarray()],[inverse.coupling.toarray().T,F]]),K,atol=1e-10,rtol=1e-10)
        rng=np.random.default_rng(1105);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01
        z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0]);before=system.reduce_rhs(rhs);u,p=system.reconstruct(z,rhs)
        pressure=C.pressure.upper.diagonal().copy();x,y=rng.normal(size=(2,nv));bx,by=inverse.solve(x),inverse.solve(y)
        np.testing.assert_allclose(inverse.solve(x+2*y),bx+2*by,atol=1e-10,rtol=1e-10)
        self.assertAlmostEqual(float(x@by),float(y@bx),places=8);self.assertGreater(float(x@bx),0)
        self.assertTrue(inverse.input_unchanged());inverse.close()
        np.testing.assert_array_equal(system.reduce_rhs(rhs),before);uu,pp=system.reconstruct(z,rhs)
        np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p);np.testing.assert_array_equal(C.pressure.upper.diagonal(),pressure)
        old=CondensedSystem(system.mesh,.1,cache_cap_bytes=0);np.testing.assert_array_equal(pressure,old.matrix.diagonal()[3*old.nt:])
        ub,pb,Au,B=assemble_quartic(system.mesh,.1,128)
        expected=np.r_[np.concatenate([Au@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)

    def test_malformed_or_noninvertible_coordinate_certificate_rejected(self):
        H=simple_hierarchy(6,2)
        for pivots in ([0,0],[0,6],[0]):
            with self.assertRaises(ValueError):HierarchicalCoordinates(H.Z,pivots,[('vertex',[0]),('vertex',[1])])
        bad=H.Z.copy();bad[0,1]=.2
        with self.assertRaises(ValueError):HierarchicalCoordinates(bad,[0,1],[('vertex',[0]),('vertex',[1])])
        with self.assertRaises(ValueError):HierarchicalCoordinates(H.Z,[0,1],[('vertex',[0])])
        with self.assertRaises(ValueError):HierarchyCholesky(SymmetricTriangle(csr_matrix(np.eye(6))),H,LIB,cycles=1.5)

    def test_partial_nonpositive_factor_cleanup_and_closed_rhs(self):
        H=HierarchicalCoordinates(csr_matrix(np.eye(6)[:,:2]),[0,1],[('vertex',[0]),('vertex',[1])]);completed=[]
        def factory(*args):
            factor=SharedTriangleFactor(*args);completed.append(factor);return factor
        with patch('cfd_reference3d_hierarchical.SharedTriangleFactor',side_effect=factory):
            with self.assertRaises(ValueError):HierarchyCholesky(SymmetricTriangle(csr_matrix(np.diag([1.,1.,-1.,1.,1.,1.]))),H,LIB)
        self.assertEqual(len(completed),1);self.assertIsNone(completed[0]._handle)
        inverse=HierarchyCholesky(SymmetricTriangle(csr_matrix(np.eye(6))),H,LIB)
        for rhs in (np.ones(5),np.full(6,np.nan)):
            with self.assertRaises(ValueError):inverse.solve(rhs)
        inverse.close();inverse.close()
        with self.assertRaises(ValueError):inverse.solve(np.ones(6))

    def test_one_hundred_owned_cleanup_cycles(self):
        H=HierarchicalCoordinates(csr_matrix(np.eye(6)[:,:2]),[0,1],[('vertex',[0]),('vertex',[1])]);velocity=SymmetricTriangle(csr_matrix(np.eye(6)))
        for _ in range(100):
            inverse=HierarchyCholesky(velocity,H,LIB);np.testing.assert_array_equal(inverse.solve(np.ones(6)),np.ones(6));inverse.close()


if __name__=='__main__':unittest.main()
