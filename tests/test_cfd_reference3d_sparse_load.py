"""Bitwise full load restoration and exact FE authority across factor lifetime."""
import sys,json,gc,weakref
import unittest
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_sparse_load import SparseLoad
from cfd_reference3d_shared_factor import storage_sha,BlockTriangle
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_vector_workspace import VectorWorkspaceCholesky
from cfd_reference3d_condensed import full_action
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=ROOT/'build/c3d-vector-storage/support/factor.dylib'
class Load(unittest.TestCase):
    def test_sparse_dense_signed_zero_tiny_and_all_zero_bits(self):
        rng=np.random.default_rng(1551)
        for x in (np.zeros(1500),rng.normal(size=99),np.r_[0.,-0.,1e-300,-1e-300,np.zeros(500),3.14]):
            h=storage_sha(x);load=SparseLoad(x);np.testing.assert_array_equal(load.materialize().view('uint64'),x.view('uint64'))
            self.assertEqual(load.metadata['dense_sha256'],h);self.assertTrue(load.unchanged());json.loads(json.dumps(load.metadata))
            self.assertFalse(load.indices.flags.writeable or load.values.flags.writeable)
    def test_dense_owner_released_and_invalid_or_corruption_rejected(self):
        x=np.zeros(1500);x[3]=1e-280;owner=weakref.ref(x);load=SparseLoad(x);del x;gc.collect();self.assertIsNone(owner())
        self.assertEqual(load.materialize()[3],1e-280);self.assertLess(load.metadata['indexed_array_bytes'],load.metadata['dense_array_bytes'])
        for x in (np.ones((2,3)),np.ones(5,dtype='float32'),np.ones(8)[::2],np.array([np.nan]),np.array([])):
            with self.assertRaises(ValueError):SparseLoad(x)
        load.values.flags.writeable=True;load.values[0]=1.
        with self.assertRaises(ValueError):load.materialize()
    def test_original_retained_rhs_reconstruction_full_action_and_live_factor(self):
        system=system_fixture();C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        rng=np.random.default_rng(1552);x=np.zeros(3*system.ub.N+system.pb.N);x[[3,11,3*system.ub.N+4]]=[.2,1e-280,.01]
        reduced=system.reduce_rhs(x);z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0]);u,p=system.reconstruct(z,x)
        expected=full_action(system.mesh,system.ub,system.pb,u,p,.1,128);original=storage_sha(x);load=SparseLoad(x);del x
        C.velocity=VectorTriangle(C.velocity.upper,LIB)
        factor=VectorWorkspaceCholesky(C.velocity,LIB,live_arrays=(load.indices,load.values,reduced),action=lambda:C@z[system.retained_free])
        self.assertTrue(factor.metadata['pressure_control']['live_input_preserved']);factor.solve(np.ones(C.nv));factor.close()
        restored=load.materialize();self.assertEqual(storage_sha(restored),original);np.testing.assert_array_equal(system.reduce_rhs(restored),reduced)
        uu,pp=system.reconstruct(z,restored);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p)
        np.testing.assert_array_equal(full_action(system.mesh,system.ub,system.pb,uu,pp,.1,128),expected)
if __name__=='__main__':unittest.main()
