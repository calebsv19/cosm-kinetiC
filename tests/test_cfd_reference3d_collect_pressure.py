"""Unreachable cycle collection without live owner/FE/policy or peak changes."""
import sys,gc,json,weakref
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_collect_pressure import release_free_pages
from cfd_reference3d_collect_workspace import CollectWorkspaceCholesky
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_sparse_load import SparseLoad
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_condensed import full_action
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=library_path('build/c3d-vector-storage/support/factor.dylib')
class Node:
    def __init__(self):self.cycle=self;self.data=np.ones(1024)
class Collect(unittest.TestCase):
    def test_actual_cycle_collection_live_array_policy_and_action(self):
        enabled=gc.isenabled();threshold=gc.get_threshold();gc.disable()
        try:
            garbage=Node();dead=weakref.ref(garbage);del garbage
            live=Node();owner=weakref.ref(live);a=live.data;digest=storage_sha(a)
            row=release_free_pages((a,),action=lambda:2*a)
            self.assertIsNone(dead());self.assertIsNotNone(owner());self.assertEqual(storage_sha(a),digest)
            c=row['python_collection'];self.assertGreater(c['collected_object_count'],0);self.assertTrue(c['live_input_preserved'] and c['action_preserved'] and c['global_gc_policy_preserved'])
            self.assertFalse(gc.isenabled());self.assertEqual(gc.get_threshold(),threshold)
            self.assertGreaterEqual(c['owned_high_water_after_bytes'],c['owned_high_water_before_bytes']);json.loads(json.dumps(row))
        finally:
            if enabled:gc.enable()
    def test_live_symbolic_factor_full_rhs_reconstruction_and_original_FE(self):
        system=system_fixture();C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro);C.velocity=VectorTriangle(C.velocity.upper,LIB)
        rng=np.random.default_rng(1661);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01;load=SparseLoad(rhs);reduced=system.reduce_rhs(rhs)
        z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0]);u,p=system.reconstruct(z,rhs)
        before=full_action(system.mesh,system.ub,system.pb,u,p,.1,128);arrays=tuple(a for m in (C.coupling,C.pressure.upper) for a in (m.indptr,m.indices,m.data))+(load.indices,load.values,reduced)
        h=storage_sha(*arrays);f=CollectWorkspaceCholesky(C.velocity,LIB,live_arrays=arrays,action=lambda:C@z[system.retained_free])
        self.assertTrue(f.metadata['pressure_control']['python_collection']['live_input_preserved']);np.testing.assert_allclose(C.velocity@f.solve(np.ones(C.nv)),np.ones(C.nv),atol=1e-10,rtol=1e-10);f.close()
        self.assertEqual(storage_sha(*arrays),h);restored=load.materialize();np.testing.assert_array_equal(system.reduce_rhs(restored),reduced)
        uu,pp=system.reconstruct(z,restored);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p);np.testing.assert_array_equal(full_action(system.mesh,system.ub,system.pb,uu,pp,.1,128),before)
    def test_partial_symbolic_cleanup_and_collection_input_action_rejection(self):
        vector=VectorTriangle(csr_matrix(np.eye(6)),LIB);f=CollectWorkspaceCholesky.__new__(CollectWorkspaceCholesky)
        with patch('cfd_reference3d_collect_workspace.release_free_pages',side_effect=RuntimeError('collection unavailable')):
            with self.assertRaisesRegex(RuntimeError,'unavailable'):CollectWorkspaceCholesky.__init__(f,vector,LIB)
        self.assertIsNone(f._handle)
        a=np.ones(6)
        def alter(*args):a[0]=3.;return 0
        with patch('cfd_reference3d_collect_pressure.gc.collect',side_effect=alter):
            with self.assertRaisesRegex(ValueError,'live input'):release_free_pages((a,))
        n=[0]
        def action():n[0]+=1;return np.ones(6)*n[0]
        with self.assertRaisesRegex(ValueError,'physical action'):release_free_pages((np.ones(6),),action)
if __name__=='__main__':unittest.main()
