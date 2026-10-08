import unittest,sys,json,ctypes as ct,gc,weakref
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from test_cfd_reference3d_distributed_p3_cg8 import fixture,dense_fixture,LOCAL,COARSE as OLDLIB
from cfd_reference3d_p3_cg8_scalar import RefinedFloatCoarse
from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor
from cfd_reference3d_distributed_p3_cg8_pressure import DistributedP3CG8PressureFactor
from cfd_reference3d_vector_storage import VectorTriangle
LIB=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib')
class ScalarAction(unittest.TestCase):
 def test_dense_sparse_action_original_input_and_Float_corrected_solve(self):
  rng,A,Z,C=dense_fixture();f=RefinedFloatCoarse(C,LIB);dense=(C+C.T).toarray()-np.diag(C.diagonal());rhs=rng.normal(size=f.n);np.testing.assert_allclose(f.action(rhs),dense@rhs,rtol=1e-12,atol=1e-12);np.testing.assert_allclose(dense@f.solve(rhs),rhs,rtol=1e-10,atol=1e-10);self.assertTrue(f.input_unchanged());self.assertEqual(f.metadata['scalar_action_abi_arguments'],6);f.close()
 def test_original_FE_corrected_inverse_fixed_pressure_CG_and_cleanup(self):
  _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LOCAL);kwargs=dict(pressure_control=False,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata);a=DistributedP3CG8ScalarPressureFactor(t,LOCAL,coarse_library=LIB,**kwargs);b=DistributedP3CG8PressureFactor(t,LOCAL,coarse_library=OLDLIB,**kwargs);rng=np.random.default_rng(812);x=rng.normal(size=nv)
  for method in ('solve','pressure_solve'):
   actual=getattr(a,method)(x);old=getattr(b,method)(x);self.assertLess(np.linalg.norm(actual-old)/np.linalg.norm(old),1e-8)
  self.assertTrue(a.input_unchanged());self.assertLess(max(a.metadata['distributed_velocity']['sampled_coarse_reproduction_relative_errors']),1e-8);refs=[weakref.ref(a.linear_pressure),weakref.ref(a.balanced),weakref.ref(a.coarse_factor.starts)];a.close();b.close();del a;gc.collect();self.assertTrue(all(r() is None for r in refs))
 def test_alias_invalid_rhs_and_source_prefix(self):
     rng, A, Z, C = dense_fixture()
     f = RefinedFloatCoarse(C, LIB)
     x = np.ones(f.n)
     lp = ct.POINTER(ct.c_long)
     ip = ct.POINTER(ct.c_int)
     dp = ct.POINTER(ct.c_double)
     self.assertEqual(f.scalar_action(f.n, f.starts.ctypes.data_as(lp), f.rows.ctypes.data_as(ip), f.owner.data.ctypes.data_as(dp), x.ctypes.data_as(dp), x.ctypes.data_as(dp)), -100)
     with self.assertRaises(ValueError):
         f.action(np.full(f.n, np.nan))
     with self.assertRaises(ValueError):
         f.action(np.ones(f.n + 1))
     f.close()
     parent = (R / 'scripts/cfd_reference3d_encoded_storage.c').read_bytes()
     self.assertTrue((R / 'scripts/cfd_reference3d_p3_coarse_scalar.c').read_bytes().startswith(parent))
if __name__=='__main__':unittest.main()
