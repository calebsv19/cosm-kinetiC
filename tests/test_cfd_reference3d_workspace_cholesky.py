"""Caller-owned symbolic/numeric lifecycle and unchanged exact FE inverse."""
import sys,json,gc,weakref
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_workspace_cholesky import WorkspaceCholesky
from cfd_reference3d_shared_factor import SharedTriangleFactor,BlockTriangle,storage_sha
from cfd_reference3d_symbolic import SymbolicFactor
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_coarse_velocity import system_fixture
from test_cfd_reference3d_bounded_condensed import fixture
LIB=ROOT/'build/c3d-workspace-cholesky/support/factor.dylib'
OLDLIB=ROOT/'build/c3d-cholesky/support/factor.dylib'
SYMLIB=ROOT/'build/c3d-symbolic/support/symbolic.dylib'


class Workspace(unittest.TestCase):
    def test_dense_exact_inverse_symbolic_bytes_caller_storage_and_scratch_release(self):
        rng=np.random.default_rng(1321);R=rng.normal(size=(24,24));A=R.T@R+np.eye(24);upper=csr_matrix(np.triu(A))
        phases=[];factor=WorkspaceCholesky(upper,LIB,pressure_control=False,stage_callback=phases.append)
        old=SharedTriangleFactor(upper,OLDLIB);symbolic=SymbolicFactor(upper,SYMLIB)
        x=rng.normal(size=24);np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),atol=1e-11,rtol=1e-11)
        self.assertEqual(factor.metadata['symbolic_factor_storage_bytes'],old.metadata['symbolic_factor_storage_bytes'])
        self.assertEqual(factor.metadata['symbolic_factor_storage_bytes'],symbolic.metadata['factor_storage_bytes'])
        self.assertEqual(factor.metadata['numeric_workspace_bytes'],symbolic.metadata['numeric_workspace_bytes'])
        self.assertTrue(factor.metadata['user_factor_storage_verified']);self.assertEqual(factor.metadata['numeric_workspace_retained_bytes'],0)
        self.assertEqual(phases,['workspace_symbolic_ready','workspace_pressure_complete','workspace_numeric_ready'])
        self.assertTrue(factor.input_unchanged());json.loads(json.dumps(factor.metadata));factor.close();old.close();symbolic.close()

    def test_anisotropic_refined_exact_original_velocity_block_inverse(self):
        for refined in (False,True):
            system=TriangleCondensedSystem(fixture(refined),.1,fixed_boundaries=('walls',),assembly_batch=7)
            C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro);upper=C.velocity.upper
            A=upper.toarray();A+=A.T-np.diag(np.diag(A));x=np.random.default_rng(1322).normal(size=C.nv)
            before=storage_sha(upper.indptr,upper.indices,upper.data)
            factor=WorkspaceCholesky(upper,LIB,pressure_control=False)
            np.testing.assert_allclose(factor.solve(x),np.linalg.solve(A,x),atol=1e-9,rtol=1e-9)
            self.assertEqual(storage_sha(upper.indptr,upper.indices,upper.data),before);factor.close()

    def test_symbolic_pressure_lifetime_all_mixed_inputs_rhs_and_full_fe_unchanged(self):
        system=system_fixture();nv=len(system.retained_free)-system.nmacro;C=BlockTriangle(system.upper_matrix,nv)
        rng=np.random.default_rng(1323);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01;before=system.reduce_rhs(rhs)
        z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0]);u,p=system.reconstruct(z,rhs)
        pressure=C.pressure.upper.diagonal().copy();probe=rng.normal(size=C.shape[0])
        arrays=tuple(a for m in (C.coupling,C.pressure.upper) for a in (m.indptr,m.indices,m.data))+(rhs,before)
        factor=WorkspaceCholesky(C.velocity.upper,LIB,live_arrays=arrays,action=lambda:C@probe)
        control=factor.metadata['pressure_control'];self.assertTrue(control['live_input_preserved'] and control['action_preserved'])
        self.assertGreaterEqual(control['owned_high_water_after_bytes'],control['owned_high_water_before_bytes'])
        self.assertEqual(control['action_max_absolute_change'],0);factor.solve(np.ones(nv));factor.close()
        np.testing.assert_array_equal(system.reduce_rhs(rhs),before);uu,pp=system.reconstruct(z,rhs)
        np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p);np.testing.assert_array_equal(C.pressure.upper.diagonal(),pressure)
        old=CondensedSystem(system.mesh,.1,cache_cap_bytes=0);np.testing.assert_array_equal(pressure,old.matrix.diagonal()[3*old.nt:])
        ub,pb,A,B=assemble_quartic(system.mesh,.1,128)
        expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)

    def test_shared_owner_and_stage_failure_cleanup(self):
        upper=csr_matrix(np.eye(6));ref=weakref.ref(upper);factor=WorkspaceCholesky(upper,LIB,pressure_control=False)
        self.assertTrue(np.shares_memory(factor.values,upper.data));self.assertTrue(np.shares_memory(factor.rows,upper.indices));del upper;gc.collect()
        self.assertIsNotNone(ref());np.testing.assert_array_equal(factor.solve(np.ones(6)),np.ones(6));factor.close();del factor;gc.collect();self.assertIsNone(ref())
        failed=WorkspaceCholesky.__new__(WorkspaceCholesky)
        with patch('cfd_reference3d_workspace_cholesky.release_free_pages',side_effect=RuntimeError('pressure unavailable')):
            with self.assertRaisesRegex(RuntimeError,'pressure unavailable'):WorkspaceCholesky.__init__(failed,csr_matrix(np.eye(6)),LIB)
        self.assertIsNone(failed._handle)
        phase_failed=WorkspaceCholesky.__new__(WorkspaceCholesky)
        def reject(phase):raise RuntimeError('phase budget stopped')
        with self.assertRaises(RuntimeError):WorkspaceCholesky.__init__(phase_failed,csr_matrix(np.eye(6)),LIB,stage_callback=reject)
        self.assertIsNone(phase_failed._handle)

    def test_nonpositive_invalid_and_closed_rejected(self):
        with self.assertRaises(ValueError):WorkspaceCholesky(csr_matrix(np.diag([1.,-1.])),LIB,pressure_control=False)
        bad=csr_matrix(np.eye(3));bad.indices[0]=-1
        with self.assertRaises(ValueError):WorkspaceCholesky(bad,LIB)
        with self.assertRaises(ValueError):WorkspaceCholesky(csr_matrix(np.eye(3)),LIB,ordering='invalid')
        factor=WorkspaceCholesky(csr_matrix(np.eye(6)),LIB,pressure_control=False)
        for x in (np.ones(5),np.full(6,np.nan)):
            with self.assertRaises(ValueError):factor.solve(x)
        factor.close();factor.close()
        with self.assertRaises(ValueError):factor.solve(np.ones(6))

    def test_one_hundred_caller_buffer_handle_cleanup_cycles(self):
        for _ in range(100):
            factor=WorkspaceCholesky(csr_matrix(np.eye(6)),LIB,pressure_control=False)
            np.testing.assert_array_equal(factor.solve(np.ones(6)),np.ones(6));factor.close()


if __name__=='__main__':unittest.main()
