"""Exact C coefficient action, shared PC input, FE and ownership authority."""
import sys,unittest,weakref,json
from pathlib import Path
from unittest.mock import patch
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_encoded_operator import EncodedTriangle
from cfd_reference3d_encoded_workspace import EncodedWorkspaceCholesky
from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_flexible import flexible_gmres
from cfd_reference3d_condensed import full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=R/'build/c3d-encoded-operator/support/factor.dylib'

def old_triangle(A):return VectorTriangle(csr_matrix(np.triu(A)),LIB,batch_rows=3)
def bits(a,b):np.testing.assert_array_equal(a.view(np.uint64),b.view(np.uint64))

class Encoded(unittest.TestCase):
    def test_c_bitwise_dense_sparse_rounding_signed_zero_and_transpose(self):
        rng=np.random.default_rng(3012)
        for n in (3,18,33):
            B=rng.normal(size=(n,n));A=B.T@B+2*np.eye(n);t=old_triangle(A)
            # Deliberately include signed zero and float rounding boundary words.
            t.values[1]=-0.;t.values[3]=-0.;t.values[2]=1.+2.**-24;t.values[6]=t.values[2]
            t.input_sha256=storage_sha(t.starts,t.rows,t.values)
            e=EncodedTriangle(t,LIB);x=rng.normal(size=n);X=rng.normal(size=(n,7))
            bits(e@x,t@x);bits(e@X,t@X);bits(e.rmatvec(x),t.rmatvec(x));bits(e@np.zeros(n),t@np.zeros(n))
            self.assertEqual(storage_sha(t.values.astype(np.float32)),e.encoding['predictor_float_sha256'])
            self.assertTrue(e.input_unchanged());self.assertFalse(e.predictor.flags.writeable or e.corrections.flags.writeable)
            self.assertEqual(e.encoding['original_value_sha256'],e.encoding['decoded_value_sha256']);json.dumps(e.metadata)

    def test_shared_float_factor_original_inverse_and_correction(self):
        rng=np.random.default_rng(3013);B=rng.normal(size=(24,24));A=B.T@B+5*np.eye(24);t=old_triangle(A);e=EncodedTriangle(t,LIB)
        old=MixedWorkspaceCholesky(t,LIB,pressure_control=False);new=EncodedWorkspaceCholesky(e,LIB,pressure_control=False);x=rng.normal(size=24)
        bits(old.solve(x),new.solve(x));self.assertIs(new.rounded,e.predictor);self.assertIs(new.values,e.corrections)
        self.assertEqual(new.metadata['factor_input_allocation_bytes'],0)
        for name in ('symbolic_factor_storage_bytes','numeric_workspace_bytes','rounded_values_sha256','solve_workspace_bytes'):
            self.assertEqual(old.metadata[name],new.metadata[name])
        self.assertTrue(new.metadata['user_factor_storage_verified']);self.assertEqual(new.metadata['numeric_workspace_retained_bytes'],0)
        z,info,_=flexible_gmres(e,x,new.solve,target=1e-11,restart=6)
        self.assertEqual(info,0);self.assertLess(np.linalg.norm(e@z-x)/np.linalg.norm(x),1e-11)
        np.testing.assert_allclose(z,np.linalg.solve(A,x),rtol=1e-10,atol=1e-10)
        self.assertTrue(new.input_unchanged() and old.input_unchanged());old.close();new.close();json.dumps(new.metadata)

    def test_full_mixed_load_pressure_and_independent_fe_unchanged(self):
        system=system_fixture();C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        t=VectorTriangle(C.velocity.upper,LIB,7);C.velocity=t;rng=np.random.default_rng(3014)
        x=rng.normal(size=C.shape[0]);before=C@x;rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01;reduced=system.reduce_rhs(rhs)
        retained=np.zeros(system.shape[0]);retained[system.retained_free]=x;u,p=system.reconstruct(retained,rhs)
        arrays=tuple(a for m in (C.coupling,C.pressure.upper) for a in (m.indptr,m.indices,m.data));h=storage_sha(*arrays)
        e=EncodedTriangle(t,LIB);C.velocity=e;bits(C@x,before)
        factor=EncodedWorkspaceCholesky(e,LIB,live_arrays=(*arrays,rhs,reduced),action=lambda:C@x)
        self.assertTrue(factor.metadata['pressure_control']['action_preserved']);self.assertTrue(factor.input_unchanged());factor.close()
        self.assertEqual(storage_sha(*arrays),h);bits(system.reduce_rhs(rhs),reduced)
        uu,pp=system.reconstruct(retained,rhs);bits(uu,u);bits(pp,p)
        ub,pb,A,B=assemble_quartic(system.mesh,.1,128)
        expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)

    def test_original_double_released_borrowed_lifetime_and_cleanup(self):
        t=old_triangle(np.eye(6));original=weakref.ref(t.values);owner=weakref.ref(t);e=EncodedTriangle(t,LIB);del t
        self.assertIsNone(original());self.assertIsNone(owner());self.assertFalse(hasattr(e,'values'))
        f=EncodedWorkspaceCholesky(e,LIB,pressure_control=False);alive=weakref.ref(e);del e;self.assertIsNotNone(alive())
        bits(f.solve(np.ones(6)),np.ones(6));f.close();f.close();del f;self.assertIsNone(alive())
        for phase in ('workspace_symbolic_ready','workspace_pressure_complete'):
            f=EncodedWorkspaceCholesky.__new__(EncodedWorkspaceCholesky)
            def stop(p):
                if p==phase:raise RuntimeError('owned phase stop')
            with self.assertRaisesRegex(RuntimeError,'owned phase stop'):EncodedWorkspaceCholesky.__init__(f,EncodedTriangle(old_triangle(np.eye(6)),LIB),LIB,stage_callback=stop)
            self.assertIsNone(f._handle)

    def test_invalid_physical_predictor_input_rhs_and_nonpositive_factor(self):
        for v in (1e-100,1e100):
            with self.assertRaises(ValueError):EncodedTriangle(old_triangle(np.diag([1.,1.,v])),LIB)
        t=old_triangle(np.eye(3));t.values[0]=2.
        with self.assertRaises(ValueError):EncodedTriangle(t,LIB)
        e=EncodedTriangle(old_triangle(np.eye(3)),LIB);f=EncodedWorkspaceCholesky(e,LIB,pressure_control=False)
        for x in (np.full(3,np.nan),np.full(3,np.inf),np.ones(2)):
            with self.assertRaises(ValueError):e@x
            with self.assertRaises(ValueError):f.solve(x)
        f.close()
        with self.assertRaises(ValueError):f.solve(np.ones(3))
        with self.assertRaises(ValueError):EncodedWorkspaceCholesky(EncodedTriangle(old_triangle(np.diag([1.,1.,-1.])),LIB),LIB,pressure_control=False)
        e.corrections.flags.writeable=True;e.corrections[0]+=1
        with self.assertRaises(ValueError):EncodedWorkspaceCholesky(e,LIB)

    def test_allocation_refusal_precedes_encoded_arrays_and_repeated_factors(self):
        t=old_triangle(np.eye(6));h=t.input_sha256;requests=[]
        def reject(phase,projected):requests.append((phase,projected));raise RuntimeError('construction cap')
        with self.assertRaisesRegex(RuntimeError,'construction cap'):EncodedTriangle(t,LIB,check=reject)
        self.assertEqual(requests[0][0],'predictor_output_allocation');self.assertEqual(t.input_sha256,h);self.assertTrue(t.input_unchanged())
        e=EncodedTriangle(t,LIB)
        for _ in range(30):
            f=EncodedWorkspaceCholesky(e,LIB,pressure_control=False);bits(f.solve(np.ones(6)),np.ones(6));f.close()

if __name__=='__main__':unittest.main()
