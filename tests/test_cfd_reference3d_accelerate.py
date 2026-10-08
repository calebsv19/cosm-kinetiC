"""Independent dense inverse, lifecycle and invalid physical-matrix controls."""
import os
import sys
import json
import hashlib
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse import csc_matrix,diags
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_accelerate import CholeskyFactor
ROOT=Path(__file__).resolve().parents[1]
LIB=library_path('build/c3d-cholesky/support/factor.dylib')


class SparseCholesky(unittest.TestCase):
    def test_inverse_linear_input_and_anisotropic_matrices(self):
        rng=np.random.default_rng(841)
        for n in (4,29,80):
            B=rng.normal(size=(n,n));B[np.abs(B)<1.2]=0
            scale=np.geomspace(.1,10,n);A=(B@B.T+np.eye(n)*.01)*scale[:,None]*scale[None,:]
            sparse=csc_matrix(A);before=sparse.copy();factor=CholeskyFactor(sparse,LIB)
            x=rng.normal(size=n);y=rng.normal(size=n);saved=x.copy()
            np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),rtol=2e-10,atol=2e-10)
            np.testing.assert_allclose(factor.solve(2*x+y),2*factor.solve(x)+factor.solve(y),rtol=2e-10,atol=2e-10)
            np.testing.assert_array_equal(x,saved);np.testing.assert_array_equal(sparse.data,before.data)
            inverse=np.column_stack([factor.solve(np.eye(n)[:,i]) for i in range(n)])
            np.testing.assert_allclose(inverse,inverse.T,rtol=2e-10,atol=2e-10)
            self.assertGreater(np.linalg.eigvalsh(inverse).min(),0)
            self.assertGreater(factor.metadata['symbolic_factor_storage_bytes'],0)
            factor.close()
    def test_velocity_prefix_of_indefinite_mixed_operator(self):
        rng=np.random.default_rng(14);B=rng.normal(size=(12,12));A=B@B.T+np.eye(12)*.1
        divergence=rng.normal(size=(3,12));mixed=csc_matrix(np.block([[A,divergence.T],[divergence,np.zeros((3,3))]]))
        before=mixed.copy();x=rng.normal(size=12)
        for ordering in ('amd','metis'):
            factor=CholeskyFactor(mixed,LIB,ordering,prefix=12)
            np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),rtol=1e-12,atol=1e-12)
            np.testing.assert_array_equal(mixed.data,before.data);factor.close()
        for prefix in (0,16,2.5):
            with self.assertRaises(ValueError):CholeskyFactor(mixed,LIB,prefix=prefix)

    def test_nested_dissection_inverse_and_storage(self):
        rng=np.random.default_rng(114);B=rng.normal(size=(40,40));B[np.abs(B)<1.4]=0
        A=B@B.T+np.eye(40)*.01;factor=CholeskyFactor(csc_matrix(A),LIB,'metis')
        x=rng.normal(size=40)
        np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),rtol=1e-11,atol=1e-11)
        self.assertEqual(factor.metadata['ordering'],'metis');factor.close()

    def test_reject_nonpositive_nonsymmetric_and_invalid_rhs(self):
        for A in (np.diag([1.,-1.]),np.diag([1.,0.]),np.array([[1.,2.],[2.,1.]]),np.array([[1.,2.],[0.,3.]]),np.diag([1.,np.nan])):
            with self.assertRaises(ValueError):CholeskyFactor(csc_matrix(A),LIB)
        factor=CholeskyFactor(diags([1.,2.]),LIB)
        for x in ([1.],[np.inf,2.],[[1.,2.]]):
            with self.assertRaises(ValueError):factor.solve(x)
        factor.close()
    def test_repeated_owned_cleanup_and_closed_action(self):
        for _ in range(100):
            factor=CholeskyFactor(csc_matrix([[3.,1.],[1.,2.]]),LIB)
            np.testing.assert_allclose(factor.solve(np.array([1.,2.])),[0.,1.],atol=1e-14)
            factor.close();factor.close()
            self.assertIsNone(factor._handle)
            with self.assertRaises(ValueError):factor.solve(np.ones(2))


if __name__=='__main__':
    result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(SparseCholesky))
    sources=[ROOT/'scripts/cfd_reference3d_accelerate.py',ROOT/'scripts/cfd_reference3d_accelerate.c',Path(__file__).resolve()]
    digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    receipt=dict(schema='physics_sim_c3d_cholesky_support_tests_v1',tests_run=result.testsRun,
        successful=result.wasSuccessful(),source_sha256={str(path):digest(path) for path in sources},
        library_path=str(LIB),library_sha256=digest(LIB))
    (LIB.parent.parent/'support-test-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    raise SystemExit(0 if result.wasSuccessful() else 1)
