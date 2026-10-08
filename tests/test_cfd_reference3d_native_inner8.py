import ctypes as ct,gc,json,sys,unittest,weakref
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
LIB=library_path('build/c3d-native-inner8/support/factor.dylib')
from cfd_reference3d_bounded_fill1 import BlockIC0
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_encoded_operator import EncodedTriangle
from cfd_reference3d_native_inner8_action import native_inner8
from cfd_reference3d_p3_cg8_scalar import inner_cg8,BalancedSparse,triple
class Native(unittest.TestCase):
    def test_independent_numpy_recurrence_dense_and_encoded_original_bits(self):
        rng=np.random.default_rng(2121);M=rng.normal(size=(45,45));A=M@M.T+2*np.eye(45);t=VectorTriangle(csr_matrix(np.triu(A)),LIB)
        for owner in (t,EncodedTriangle(t,LIB)):
            f=BlockIC0(owner,LIB,False);x=rng.normal(size=f.n);expected=inner_cg8(lambda u:owner@u,lambda r:f.solve(r),x);actual=native_inner8(f,x)
            np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-11);self.assertTrue(f.input_unchanged());self.assertGreater(x@actual,0);np.testing.assert_array_equal(native_inner8(f,np.zeros(f.n)),np.zeros(f.n));f.close()
    def test_actual_anisotropic_FE_balanced_and_fixed_pressure_equivalence_owners(self):
        from test_cfd_reference3d_distributed_p3_cg8 import fixture
        from cfd_reference3d_native_inner8_pressure import NativeInner8PressureFactor
        _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LIB);f=NativeInner8PressureFactor(t,LIB,pressure_control=False,coarse_library=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib'),Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata)
        baseline=BalancedSparse(t,f.Z,f.coarse_factor,lambda x:inner_cg8(lambda u:t@u,lambda r:BlockIC0.solve(f,r),x));fixed=BalancedSparse(t,f.Z,f.coarse_factor,lambda x:triple(lambda u:t@u,lambda r:BlockIC0.solve(f,r),x))
        for x in np.random.default_rng(2122).normal(size=(3,nv)):
            np.testing.assert_allclose(f.solve(x),baseline.solve(x),rtol=1e-10,atol=1e-9);np.testing.assert_array_equal(f.pressure_solve(x),fixed.solve(x))
        self.assertTrue(f.input_unchanged());self.assertEqual(f.metadata['native_inner_scratch_vectors'],7);self.assertEqual(f.metadata['native_inner_output_vectors'],1)
        refs=[weakref.ref(f.starts),weakref.ref(f.balanced),weakref.ref(f.linear_pressure),weakref.ref(f.coarse_factor.starts)];del baseline,fixed;f.close();del f;gc.collect();self.assertTrue(all(r() is None for r in refs))
    def test_native_short_alias_permutation_nonfinite_closed_and_curvature_rejections(self):
        t=VectorTriangle(csr_matrix(np.eye(24)),LIB);f=BlockIC0(t,LIB,False);native_inner8(f,np.ones(f.n));fn=f.library.cfd_reference_inner_cg8
        lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);rhs=np.ones(f.n);out=np.full(f.n,123.);work=np.empty(7*f.n);steps=ct.c_int();perm=f.perm.copy()
        def call(length=None,result=None,order=None):
            return fn(f._handle,t.nodes,t.starts.ctypes.data_as(lp),t.rows.ctypes.data_as(ip),t.values.ctypes.data_as(dp),None,None,(perm if order is None else order).ctypes.data_as(ip),rhs.ctypes.data_as(dp),(out if result is None else result).ctypes.data_as(dp),work.ctypes.data_as(dp),len(work) if length is None else length,0,ct.byref(steps))
        self.assertEqual(call(length=1),-501);self.assertEqual(call(result=rhs),-501);before=t.values.copy();self.assertEqual(call(result=t.values),-501);np.testing.assert_array_equal(t.values,before)
        perm[1]=perm[0];self.assertEqual(call(),-502);np.testing.assert_array_equal(out,np.full(f.n,123.))
        with self.assertRaises(ValueError):native_inner8(f,np.full(f.n,np.nan))
        positive=f.owner;negative=VectorTriangle(csr_matrix(-np.eye(24)),LIB);f.owner=negative
        with self.assertRaises(ValueError):native_inner8(f,np.ones(f.n))
        f.owner=positive;self.assertTrue(f.input_unchanged());f.close()
        with self.assertRaises(ValueError):native_inner8(f,np.ones(f.n))
    def test_current_prefix_prefix_and_work_budget(self):
        from cfd_reference3d_native_inner8_factor import work_reserve
        self.assertTrue((R / 'scripts/cfd_reference3d_native_inner8.c').read_bytes().startswith((R / 'scripts/cfd_reference3d_bounded_fill1.c').read_bytes()))
        self.assertLessEqual(8 * 8 * 33744, work_reserve(33744, 15444))
if __name__=='__main__':unittest.main()
