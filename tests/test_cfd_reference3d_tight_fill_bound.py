import ctypes as ct
import gc
import json
import sys
import unittest
import weakref
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
LIB=library_path('build/c3d-tight-fill-bound/support/factor.dylib')
class Bound(unittest.TestCase):
    def test_independent_SVD_and_omitted_pair_PSD_extreme_scales(self):
        lib=ct.CDLL(str(LIB));fn=lib.cfd_fill1_tight_bound
        fn.argtypes=(ct.POINTER(ct.c_double),);fn.restype=ct.c_double
        rng=np.random.default_rng(1725)
        matrices=[np.diag([1.,1.,1.]),np.diag([1.,.01,.0001]),np.ones((3,3))]
        matrices += [rng.normal(size=(3,3)) for _ in range(80)]
        for U in matrices:
            for scale in (1e-200,1.,1e200):
                a=np.ascontiguousarray(U*scale);bound=fn(a.ctypes.data_as(ct.POINTER(ct.c_double)))/scale
                self.assertGreaterEqual(bound,np.linalg.svd(U,compute_uv=False)[0]*(1-1e-14))
                self.assertLessEqual(bound,np.linalg.norm(U)*(1+1e-13))
                pair=np.block([[bound*np.eye(3),U],[U.T,bound*np.eye(3)]])
                self.assertGreaterEqual(np.linalg.eigvalsh(pair).min(),-1e-12*np.linalg.norm(U))
        a=np.eye(3).ravel();self.assertLess(fn(a.ctypes.data_as(ct.POINTER(ct.c_double))),np.sqrt(3)*.6)
        a[0]=np.nan;self.assertTrue(np.isnan(fn(a.ctypes.data_as(ct.POINTER(ct.c_double)))))
        self.assertTrue(np.isnan(fn(None)))
    def test_sparse_factor_model_inverse_graph_storage_and_input_preservation(self):
        from cfd_reference3d_vector_storage import VectorTriangle
        from cfd_reference3d_bounded_fill1 import BlockIC0
        from test_cfd_reference3d_bounded_fill1 import dense_lower,LIB as OLD
        rng=np.random.default_rng(1726);n=14;H=np.eye(n)*4
        for i in range(n):
            for j in ((i+1)%n,(i+4)%n):H[i,j]=H[j,i]=-.5
        U=np.array([[1.,.05,.04],[.05,1.5,.1],[.04,.1,.7]])
        A=np.kron(H,U);p=np.arange(3*n).reshape(n,3).T.ravel();A=A[np.ix_(p,p)]
        t=VectorTriangle(csr_matrix(np.triu(A)),LIB);a=BlockIC0(t,LIB,False);b=BlockIC0(t,OLD,False)
        for k in ('starts','rows','perm','inverse_permutation'):np.testing.assert_array_equal(getattr(a,k),getattr(b,k))
        self.assertEqual(a.metadata['factor_storage_bytes'],b.metadata['factor_storage_bytes'])
        self.assertEqual(a.metadata['rounded_values_sha256'],b.metadata['rounded_values_sha256'])
        L,q=dense_lower(a);model=(L@L.T)[np.ix_(q,q)];rhs=rng.normal(size=3*n)
        np.testing.assert_allclose(a.solve(rhs),np.linalg.solve(model,rhs),rtol=1e-10,atol=1e-10)
        self.assertGreater(np.linalg.eigvalsh(model).min(),0);self.assertEqual(a.metadata['shifted_pivots'],0)
        rounded=np.zeros_like(A)
        for j in range(n):
            for k in range(t.starts[j],t.starts[j+1]):
                i=t.rows[k];block=t.values.reshape(-1,3,3)[k].astype(np.float32).astype(float);ix=np.arange(3)*n+j;iy=np.arange(3)*n+i
                rounded[np.ix_(ix,iy)]=block;rounded[np.ix_(iy,ix)]=block.T
        self.assertGreaterEqual(np.linalg.eigvalsh(model-rounded).min(),-1e-10)
        self.assertTrue(a.input_unchanged() and b.input_unchanged());a.close();b.close()
    def test_actual_anisotropic_FE_factor_fixed_pressure_and_owners(self):
        from test_cfd_reference3d_distributed_p3_cg8 import fixture
        from cfd_reference3d_tight_fill_bound import TightFillBoundFactor
        from cfd_reference3d_vector_storage import VectorTriangle
        _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LIB)
        f=TightFillBoundFactor(t,LIB,pressure_control=False,coarse_library=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib'),Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata)
        x,y=np.random.default_rng(1727).normal(size=(2,nv));px,py=f.pressure_solve(x),f.pressure_solve(y)
        np.testing.assert_allclose(x@py,y@px,rtol=1e-10,atol=1e-7);self.assertGreater(x@px,0);self.assertGreater(x@f.solve(x),0)
        self.assertTrue(f.input_unchanged());self.assertEqual(f.metadata['local_inner_iteration_cap'],8)
        refs=[weakref.ref(f.linear_pressure),weakref.ref(f.balanced),weakref.ref(f.coarse_factor.starts)]
        f.close();del f;gc.collect();self.assertTrue(all(r() is None for r in refs))
    def test_exact_transform_and_encoded_operator_prefix(self):
        self.assertTrue((R / 'scripts/cfd_reference3d_tight_fill_bound.c').read_bytes().startswith((R / 'scripts/cfd_reference3d_encoded_storage.c').read_bytes()))
if __name__=='__main__':unittest.main()
