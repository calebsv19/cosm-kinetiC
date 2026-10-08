"""Exact principal inverses and fixed SPD coupled repeated-sweep proofs."""
import os,sys,json
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference_test_support import library_path
from test_cfd_reference3d_bounded_condensed import fixture
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor,storage_sha
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_block_cholesky import RepeatedBlockCholesky
from cfd_reference3d_quartic_pair import assemble_quartic
LIB=library_path('build/c3d-cholesky/support/factor.dylib')


def dense_velocity():
    r=np.random.default_rng(871).normal(size=(24,24));a=r.T@r+np.eye(24)*.5
    return a,SymmetricTriangle(csr_matrix(np.triu(a)))


class Blocks(unittest.TestCase):
    def test_dense_exact_sweep_spd_majorization_and_fixed_repeats(self):
        A,velocity=dense_velocity()
        for grouping,bounds in (('uv_w',(0,16,24)),('u_vw',(0,8,24))):
            D=np.zeros_like(A)
            for i in range(2):
                a,b=bounds[i:i+2];D[a:b,a:b]=A[a:b,a:b]
            L=np.tril(A,-1)-np.tril(D,-1);U=L.T
            M=(D+L)@np.linalg.solve(D,D+U)
            self.assertGreaterEqual(np.linalg.eigvalsh(M-A).min(),-1e-11)
            single=np.linalg.inv(M);exact=np.linalg.inv(A)
            for cycles in (1,2,4,8):
                inverse=RepeatedBlockCholesky(velocity,LIB,cycles,grouping=grouping)
                actual=np.column_stack([inverse.solve(x) for x in np.eye(24)])
                expected=single.copy()
                for _ in range(cycles-1):expected+=single@(np.eye(24)-A@expected)
                np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-10)
                np.testing.assert_allclose(actual,actual.T,rtol=1e-10,atol=1e-10)
                self.assertGreater(np.linalg.eigvalsh(actual).min(),0)
                self.assertGreaterEqual(np.linalg.eigvalsh(exact-actual).min(),-1e-10)
                for i,factor in enumerate(inverse.factors):
                    a,b=bounds[i:i+2];x=np.arange(b-a,dtype=float)+1
                    np.testing.assert_allclose(factor.solve(x),np.linalg.solve(D[a:b,a:b],x),rtol=1e-10,atol=1e-10)
                self.assertTrue(inverse.input_unchanged());inverse.close()

    def test_anisotropic_refined_symmetry_linearity_and_input_preservation(self):
        system=TriangleCondensedSystem(fixture(True),.1,fixed_boundaries=('walls',),assembly_batch=7)
        C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        digest=storage_sha(C.velocity.upper.indptr,C.velocity.upper.indices,C.velocity.upper.data)
        x,y=np.random.default_rng(872).normal(size=(2,C.nv))
        errors=[];dense=C.velocity.upper.toarray();dense=dense+dense.T-np.diag(np.diag(dense));exact=np.linalg.solve(dense,x)
        for cycles in (1,4):
            inverse=RepeatedBlockCholesky(C.velocity,LIB,cycles)
            bx,by=inverse.solve(x),inverse.solve(y)
            np.testing.assert_allclose(inverse.solve(x+2*y),bx+2*by,rtol=1e-10,atol=1e-10)
            self.assertAlmostEqual(float(x@by),float(y@bx),places=8)
            self.assertGreater(float(x@bx),0);self.assertTrue(inverse.input_unchanged())
            errors.append(float((exact-bx)@dense@(exact-bx)))
            self.assertEqual(json.loads(json.dumps(inverse.metadata))['fixed_sweep_count'],cycles)
            inverse.close()
        self.assertLess(errors[1],errors[0])
        self.assertEqual(storage_sha(C.velocity.upper.indptr,C.velocity.upper.indices,C.velocity.upper.data),digest)

    def test_original_rhs_pressure_reconstruction_and_full_equations_unchanged(self):
        mesh=fixture(True);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        system=TriangleCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        rng=np.random.default_rng(873);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01
        z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0])
        before=system.reduce_rhs(rhs);u,p=system.reconstruct(z,rhs);pressure=C.pressure.upper.diagonal().copy()
        inverse=RepeatedBlockCholesky(C.velocity,LIB,4);inverse.solve(z[system.retained_free][:C.nv]);inverse.close()
        np.testing.assert_array_equal(system.reduce_rhs(rhs),before)
        afteru,afterp=system.reconstruct(z,rhs);np.testing.assert_array_equal(afteru,u);np.testing.assert_array_equal(afterp,p)
        np.testing.assert_array_equal(C.pressure.upper.diagonal(),pressure)
        np.testing.assert_array_equal(pressure,old.matrix.diagonal()[3*old.nt:])
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        actual=full_action(mesh,ub,pb,u,p,.1,128)
        expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-9)

    def test_partial_nonpositive_factor_failure_cleans_completed_factors(self):
        velocity=SymmetricTriangle(csr_matrix(np.diag([1.,1.,-1.,-1.,1.,1.])))
        completed=[]
        def factory(*args):
            factor=SharedTriangleFactor(*args);completed.append(factor);return factor
        with patch('cfd_reference3d_block_cholesky.SharedTriangleFactor',side_effect=factory):
            with self.assertRaises(ValueError):RepeatedBlockCholesky(velocity,LIB,grouping='u_vw')
        self.assertEqual(len(completed),1);self.assertIsNone(completed[0]._handle)

    def test_invalid_inputs_and_closed_inverse_rejected(self):
        A,velocity=dense_velocity()
        with self.assertRaises(ValueError):RepeatedBlockCholesky(velocity,LIB,grouping='invalid')
        for cycles in (0,3,1.5):
            with self.assertRaises(ValueError):RepeatedBlockCholesky(velocity,LIB,cycles)
        with self.assertRaises(ValueError):RepeatedBlockCholesky(velocity.upper,LIB)
        with self.assertRaises(ValueError):RepeatedBlockCholesky(SymmetricTriangle(csr_matrix(np.eye(5))),LIB)
        inverse=RepeatedBlockCholesky(velocity,LIB)
        for rhs in (np.ones(23),np.full(24,np.nan)):
            with self.assertRaises(ValueError):inverse.solve(rhs)
        inverse.close();inverse.close()
        with self.assertRaises(ValueError):inverse.solve(np.ones(24))

    def test_repeated_owned_factor_cleanup(self):
        for _ in range(100):
            inverse=RepeatedBlockCholesky(SymmetricTriangle(csr_matrix(np.eye(6))),LIB,2)
            np.testing.assert_array_equal(inverse.solve(np.ones(6)),np.ones(6))
            inverse.close()


if __name__=='__main__':unittest.main()
