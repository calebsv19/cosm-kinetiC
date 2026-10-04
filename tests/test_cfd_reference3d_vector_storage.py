"""Complete node blocks, exact caller-owned inverse, FE and lifetime authority."""
import sys,json,gc,weakref
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_vector_workspace import VectorWorkspaceCholesky
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor,storage_sha
from cfd_reference3d_symbolic import SymbolicFactor
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_condensed import full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_coarse_velocity import system_fixture
from test_cfd_reference3d_bounded_condensed import fixture
LIB=ROOT/'build/c3d-vector-storage/support/factor.dylib'
OLDLIB=ROOT/'build/c3d-cholesky/support/factor.dylib'
SYMLIB=ROOT/'build/c3d-symbolic/support/symbolic.dylib'

class VectorStorage(unittest.TestCase):
    def test_dense_action_inverse_orientation_symbolic_cost_and_caller_storage(self):
        rng=np.random.default_rng(1441);R=rng.normal(size=(24,24));A=R.T@R+np.eye(24);upper=csr_matrix(np.triu(A))
        vector=VectorTriangle(upper,LIB,batch_rows=3);factor=VectorWorkspaceCholesky(vector,LIB,pressure_control=False)
        old=SharedTriangleFactor(upper,OLDLIB);graph=csr_matrix((np.ones(len(vector.rows)),vector.rows,vector.starts),shape=(8,8));symbolic=SymbolicFactor(graph,SYMLIB,block_size=3)
        x=rng.normal(size=24);np.testing.assert_allclose(vector@x,A@x,atol=1e-12,rtol=1e-12)
        np.testing.assert_allclose(vector@np.eye(24),A,atol=1e-12,rtol=1e-12)
        np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),atol=1e-11,rtol=1e-11)
        np.testing.assert_allclose(factor.solve(x),old.solve(x),atol=1e-11,rtol=1e-11)
        self.assertEqual(factor.metadata['symbolic_factor_storage_bytes'],symbolic.metadata['factor_storage_bytes'])
        self.assertEqual(factor.metadata['numeric_workspace_bytes'],symbolic.metadata['numeric_workspace_bytes'])
        self.assertEqual(factor.metadata['factor_input_allocation_bytes'],0);self.assertTrue(factor.metadata['user_factor_storage_verified'])
        self.assertEqual(factor.metadata['numeric_workspace_retained_bytes'],0);self.assertTrue(vector.input_unchanged() and factor.input_unchanged())
        json.loads(json.dumps(vector.metadata));json.loads(json.dumps(factor.metadata));factor.close();old.close();symbolic.close()

    def test_sparse_tiny_coefficients_padding_and_bounded_batch_equivalence(self):
        rng=np.random.default_rng(1442);A=np.eye(33)*4
        for i,j,v in ((0,12,1e-280),(7,30,.21),(10,22,-.37),(3,4,.12),(18,23,.28)):
            A[i,j]=v;A[j,i]=v
        upper=csr_matrix(np.triu(A));before=storage_sha(upper.indptr,upper.indices,upper.data)
        first=VectorTriangle(upper,LIB,batch_rows=1);second=VectorTriangle(upper,LIB,batch_rows=8)
        np.testing.assert_array_equal(first.starts,second.starts);np.testing.assert_array_equal(first.rows,second.rows);np.testing.assert_array_equal(first.values,second.values)
        np.testing.assert_array_equal(first@np.eye(33),A);self.assertGreater(first.metadata['structural_padding_values'],0)
        self.assertEqual(storage_sha(upper.indptr,upper.indices,upper.data),before)
        factor=VectorWorkspaceCholesky(first,LIB,pressure_control=False);x=rng.normal(size=33)
        np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),atol=1e-12,rtol=1e-12);factor.close()

    def test_anisotropic_refined_original_velocity_inverse(self):
        for refined in (False,True):
            system=TriangleCondensedSystem(fixture(refined),.1,fixed_boundaries=('walls',),assembly_batch=7)
            C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro);A=C.velocity@np.eye(C.nv)
            vector=VectorTriangle(C.velocity.upper,LIB,7);factor=VectorWorkspaceCholesky(vector,LIB,pressure_control=False)
            x=np.random.default_rng(1443).normal(size=C.nv)
            np.testing.assert_allclose(vector@x,A@x,atol=1e-10,rtol=1e-10)
            np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),atol=1e-9,rtol=1e-9);factor.close()

    def test_complete_mixed_rhs_pressure_reconstruction_and_full_fe_preserved(self):
        system=system_fixture();C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        rng=np.random.default_rng(1444);x=rng.normal(size=C.shape[0]);original=C@x
        arrays=tuple(a for m in (C.coupling,C.pressure.upper) for a in (m.indptr,m.indices,m.data));hashes=[storage_sha(a) for a in arrays]
        rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01;before=system.reduce_rhs(rhs)
        z=np.zeros(system.shape[0]);z[system.retained_free]=x;u,p=system.reconstruct(z,rhs)
        C.velocity=VectorTriangle(C.velocity.upper,LIB,7)
        np.testing.assert_allclose(C@x,original,atol=1e-10,rtol=1e-10)
        factor=VectorWorkspaceCholesky(C.velocity,LIB,live_arrays=(*arrays,rhs,before),action=lambda:C@x)
        self.assertTrue(factor.metadata['pressure_control']['action_preserved']);factor.close()
        self.assertEqual([storage_sha(a) for a in arrays],hashes);np.testing.assert_array_equal(system.reduce_rhs(rhs),before)
        uu,pp=system.reconstruct(z,rhs);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p)
        ub,pb,A,B=assemble_quartic(system.mesh,.1,128)
        expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)

    def test_scalar_source_released_vector_borrowed_stage_failure_and_cleanup(self):
        upper=csr_matrix(np.eye(6));source=weakref.ref(upper);vector=VectorTriangle(upper,LIB);del upper;gc.collect();self.assertIsNone(source())
        owner=weakref.ref(vector);factor=VectorWorkspaceCholesky(vector,LIB,pressure_control=False)
        for name in ('starts','rows','values'):self.assertTrue(np.shares_memory(getattr(factor,name),getattr(vector,name)))
        del vector;gc.collect();self.assertIsNotNone(owner());np.testing.assert_array_equal(factor.solve(np.ones(6)),np.ones(6))
        factor.close();del factor;gc.collect();self.assertIsNone(owner())
        failed=VectorWorkspaceCholesky.__new__(VectorWorkspaceCholesky)
        with patch('cfd_reference3d_vector_workspace.release_free_pages',side_effect=RuntimeError('pressure unavailable')):
            with self.assertRaisesRegex(RuntimeError,'pressure unavailable'):VectorWorkspaceCholesky.__init__(failed,VectorTriangle(csr_matrix(np.eye(6)),LIB),LIB)
        self.assertIsNone(failed._handle)
        failed=VectorWorkspaceCholesky.__new__(VectorWorkspaceCholesky)
        def reject(phase):raise RuntimeError('phase budget stopped')
        with self.assertRaisesRegex(RuntimeError,'phase budget'):VectorWorkspaceCholesky.__init__(failed,VectorTriangle(csr_matrix(np.eye(6)),LIB),LIB,stage_callback=reject)
        self.assertIsNone(failed._handle)

    def test_invalid_nonpositive_closed_and_one_hundred_handle_cycles(self):
        for upper in (csr_matrix(np.eye(5)),csr_matrix(np.ones((6,6)))):
            with self.assertRaises(ValueError):VectorTriangle(upper,LIB)
        with self.assertRaises(ValueError):VectorWorkspaceCholesky(VectorTriangle(csr_matrix(np.diag([1.,1.,1.,1.,1.,-1.])),LIB),LIB,pressure_control=False)
        vector=VectorTriangle(csr_matrix(np.eye(6)),LIB)
        for _ in range(100):
            factor=VectorWorkspaceCholesky(vector,LIB,pressure_control=False);np.testing.assert_array_equal(factor.solve(np.ones(6)),np.ones(6));factor.close()
        for x in (np.ones(5),np.full(6,np.nan)):
            with self.assertRaises(ValueError):vector@x
            with self.assertRaises(ValueError):factor.solve(x)
        factor.close()
        with self.assertRaises(ValueError):factor.solve(np.ones(6))

if __name__=='__main__':unittest.main()
