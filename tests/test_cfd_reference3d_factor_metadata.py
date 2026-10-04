"""Unused FE metadata releases owners and restores exact full physical authority."""
import sys,json
import unittest
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_factor_metadata import FactorMetadata
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_vector_workspace import VectorWorkspaceCholesky
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_condensed import full_action
from test_cfd_reference3d_bounded_condensed import fixture
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=ROOT/'build/c3d-vector-storage/support/factor.dylib'
class Metadata(unittest.TestCase):
    def test_anisotropic_refined_bitwise_coordinate_mapping_index_and_owner_roundtrip(self):
        for refined in (False,True):
            s=TriangleCondensedSystem(fixture(refined),.1,fixed_boundaries=('walls',),assembly_batch=7)
            before=storage_sha(s.ub.doflocs,s.pb.doflocs,s.trace_inverse,s.ub.mapping.A,s.ub.mapping.b,s.ub.mapping.detA,s.ub.mapping.invA)
            detached=FactorMetadata(s);detached.check_detached();self.assertTrue(detached.metadata['old_arrays_detached']);self.assertGreater(detached.metadata['detached_array_bytes'],0)
            self.assertFalse(hasattr(s.ub,'doflocs') or hasattr(s.pb,'doflocs') or hasattr(s,'trace_inverse'))
            detached.restore();self.assertTrue(detached.metadata['restored_bitwise'])
            self.assertEqual(before,storage_sha(s.ub.doflocs,s.pb.doflocs,s.trace_inverse,s.ub.mapping.A,s.ub.mapping.b,s.ub.mapping.detA,s.ub.mapping.invA));json.loads(json.dumps(detached.metadata))
    def test_exact_factor_mixed_rhs_full_reconstruction_original_action_and_free_identity(self):
        s=system_fixture();C=BlockTriangle(s.upper_matrix,len(s.retained_free)-s.nmacro);C.velocity=VectorTriangle(C.velocity.upper,LIB)
        rng=np.random.default_rng(1771);rhs=rng.normal(size=3*s.ub.N+s.pb.N)*.01;reduced=s.reduce_rhs(rhs)
        z=np.zeros(s.shape[0]);z[s.retained_free]=rng.normal(size=C.shape[0]);u,p=s.reconstruct(z,rhs);action=full_action(s.mesh,s.ub,s.pb,u,p,.1,128);mixed=C@z[s.retained_free]
        fixed=s.ub.get_dofs('walls').all().copy();detached=FactorMetadata(s);detached.check_detached()
        f=VectorWorkspaceCholesky(C.velocity,LIB,live_arrays=(rhs,reduced,s.mesh.p,s.mesh.t),action=lambda:C@z[s.retained_free]);np.testing.assert_allclose(C.velocity@f.solve(np.ones(C.nv)),np.ones(C.nv),atol=1e-10,rtol=1e-10);f.close()
        np.testing.assert_array_equal(C@z[s.retained_free],mixed);detached.restore();np.testing.assert_array_equal(s.ub.get_dofs('walls').all(),fixed);np.testing.assert_array_equal(s.reduce_rhs(rhs),reduced)
        uu,pp=s.reconstruct(z,rhs);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p);np.testing.assert_array_equal(full_action(s.mesh,s.ub,s.pb,uu,pp,.1,128),action)
    def test_retained_owner_mesh_mutation_manifest_corruption_and_double_restore_rejected(self):
        s=system_fixture();held=s.ub.doflocs;detached=FactorMetadata(s)
        with self.assertRaisesRegex(ValueError,'owner'):detached.check_detached()
        del held;detached.check_detached();detached.manifest['trace_inverse']['sha256']='bad'
        with self.assertRaisesRegex(ValueError,'original bits'):detached.restore()
        s=system_fixture();detached=FactorMetadata(s);s.mesh.p[0,0]+=.1
        with self.assertRaisesRegex(ValueError,'mesh/DOF'):detached.restore()
        s=system_fixture();detached=FactorMetadata(s);detached.restore()
        with self.assertRaisesRegex(ValueError,'already'):detached.restore()
if __name__=='__main__':unittest.main()
