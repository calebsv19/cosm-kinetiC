"""Independent dense inverse, permutation and SPD checks for incomplete factors."""
import sys
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse import csc_matrix,diags
from scipy.sparse.linalg import splu,spilu
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_condensed_pc import symmetric_ilu,coupled_ilu,component_ssor,coupled_amg


class CoupledPreconditioner(unittest.TestCase):
    def test_vector_cycle_is_fixed_linear_and_preserves_input(self):
        from scipy.sparse import kron,eye
        from pyamg.gallery import poisson
        scalar=poisson((4,4,4),format='csr');A=kron(eye(3),scalar,format='csr')
        action,meta=coupled_amg(A)
        rng=np.random.default_rng(41);x=rng.normal(size=A.shape[0]);y=rng.normal(size=A.shape[0]);before=x.copy()
        np.testing.assert_allclose(action(2*x+y),2*action(x)+action(y),atol=1e-12)
        np.testing.assert_allclose(action(x),action(x),atol=1e-13)
        np.testing.assert_array_equal(x,before)
        self.assertTrue(np.all(np.isfinite(action(x))));self.assertGreater(meta['levels'],1)
        self.assertLess(np.linalg.norm(A@action(x)-x),np.linalg.norm(x))

    def test_component_ssor_matches_dense_spd_inverse(self):
        rng=np.random.default_rng(410);B=rng.normal(size=(12,12));A=B@B.T+np.eye(12)*.1
        factors=[splu(csc_matrix(A[a*4:(a+1)*4,a*4:(a+1)*4])) for a in range(3)]
        action=component_ssor(csc_matrix(A),factors)
        D=np.zeros_like(A)
        for a in range(3):D[a*4:(a+1)*4,a*4:(a+1)*4]=A[a*4:(a+1)*4,a*4:(a+1)*4]
        L=np.tril(A)-np.tril(D);M=(D+L)@np.linalg.inv(D)@(D+L.T)
        inverse=np.column_stack([action(np.eye(12)[:,i]) for i in range(12)])
        np.testing.assert_allclose(inverse,np.linalg.inv(M),atol=1e-13)
        np.testing.assert_allclose(inverse,inverse.T,atol=1e-13)
        self.assertGreater(np.linalg.eigvalsh(inverse).min(),0)
        x=rng.normal(size=12);before=x.copy();action(x);np.testing.assert_array_equal(x,before)
        self.assertGreater(np.linalg.eigvalsh(M-A).min(),-1e-12)
        for cycles in (2,4):
            repeated=component_ssor(csc_matrix(A),factors,cycles)
            dense=np.column_stack([repeated(np.eye(12)[:,i]) for i in range(12)])
            expected=np.linalg.solve(A,np.eye(12)-np.linalg.matrix_power(np.eye(12)-A@inverse,cycles))
            np.testing.assert_allclose(dense,expected,atol=1e-12)
            np.testing.assert_allclose(dense,dense.T,atol=1e-12)
            self.assertGreater(np.linalg.eigvalsh(dense).min(),0)

    def test_pivoted_incomplete_action_matches_factor_and_preserves_input(self):
        rng=np.random.default_rng(983);B=rng.normal(size=(30,30));B[np.abs(B)<1.4]=0
        A=csc_matrix(B@B.T+np.eye(30)*.01);before=A.copy()
        action,meta=coupled_ilu(A,drop_tol=.03,fill_factor=2)
        scale=1/np.sqrt(A.diagonal());D=diags(scale)
        f=spilu((D@A@D).tocsc(),drop_tol=.03,fill_factor=2,permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.)
        x=rng.normal(size=30);y=rng.normal(size=30);saved=x.copy()
        np.testing.assert_allclose(action(x),scale*f.solve(scale*x),atol=1e-13)
        np.testing.assert_allclose(action(2*x+y),2*action(x)+action(y),atol=1e-11)
        np.testing.assert_array_equal(x,saved);np.testing.assert_array_equal(A.data,before.data)
        self.assertGreater(meta['factor_storage_nnz'],0)

    def test_exact_factor_permutation_and_inverse(self):
        A=csc_matrix(np.array([[4.,1.,0.,0.],[1.,3.,1.,0.],[0.,1.,3.,1.],[0.,0.,1.,2.]]))
        action,meta=symmetric_ilu(A,drop_tol=0,fill_factor=10)
        dense=np.column_stack([action(np.eye(4)[:,i]) for i in range(4)])
        np.testing.assert_allclose(dense,np.linalg.inv(A.toarray()),rtol=1e-13,atol=1e-14)
        self.assertGreater(meta['minimum_diagonal'],0)
    def test_dropped_factor_is_symmetric_positive_and_input_preserving(self):
        rng=np.random.default_rng(204);B=rng.normal(size=(30,30));B[np.abs(B)<1.3]=0
        A=csc_matrix(B@B.T+np.eye(30)*.1)
        action,meta=symmetric_ilu(A,drop_tol=.03,fill_factor=2)
        x=rng.normal(size=30);before=x.copy();first=action(x);second=action(x)
        np.testing.assert_array_equal(x,before);np.testing.assert_allclose(first,second,atol=1e-13)
        inverse=np.column_stack([action(np.eye(30)[:,i]) for i in range(30)])
        np.testing.assert_allclose(inverse,inverse.T,atol=1e-12)
        self.assertGreater(np.linalg.eigvalsh(inverse).min(),0)
        f=spilu(A,drop_tol=.03,fill_factor=2,permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0,options={'SymmetricMode':True})
        order=np.argsort(f.perm_r);L=f.L.toarray();M=L@np.diag(f.U.diagonal())@L.T
        expected=np.empty_like(inverse);expected[np.ix_(order,order)]=np.linalg.inv(M)
        np.testing.assert_allclose(inverse,expected,atol=1e-12)


if __name__=='__main__':unittest.main()
