import unittest,sys,json,gc,weakref
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import cg
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from test_cfd_reference3d_distributed_p3_cg8 import fixture,LOCAL
from cfd_reference3d_p3_cg16_scalar import inner_cg16,work_reserve
from cfd_reference3d_p3_cg16_scalar_pressure import DistributedP3CG16ScalarPressureFactor
from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor
from cfd_reference3d_vector_storage import VectorTriangle
LIB=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib')
class Inner16(unittest.TestCase):
 def test_transformed_independent_CG16_and_fixed_memory(self):
  rng=np.random.default_rng(812);Q,_=np.linalg.qr(rng.normal(size=(80,80)));A=(Q*np.geomspace(1,1000,80))@Q.T;g=1/np.diag(A);half=np.sqrt(g);rhs=rng.normal(size=80);counts=[];expected,info=cg(half[:,None]*A*half[None,:],half*rhs,rtol=0,atol=0,maxiter=16,callback=lambda x:counts.append(1));actual=inner_cg16(lambda x:A@x,lambda x:g*x,rhs);np.testing.assert_allclose(actual,half*expected,rtol=1e-10,atol=1e-10);self.assertEqual(len(counts),16);self.assertEqual(info,16);self.assertGreater(rhs@actual,0);np.testing.assert_allclose(inner_cg16(lambda x:A@x,lambda x:g*x,3*rhs),3*actual,rtol=1e-10,atol=1e-10);self.assertEqual(work_reserve(100,20),8*(40*100+24*20)+2*2**20)
  with self.assertRaises(ValueError):inner_cg16(lambda x:-x,lambda x:x,rhs)
 def test_original_FE_fixed_pressure_same_inputs_and_owners(self):
  _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LOCAL);kw=dict(pressure_control=False,coarse_library=LIB,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata);a=DistributedP3CG16ScalarPressureFactor(t,LOCAL,**kw);b=DistributedP3CG8ScalarPressureFactor(t,LOCAL,**kw);x=np.random.default_rng(612).normal(size=nv);np.testing.assert_allclose(a.pressure_solve(x),b.pressure_solve(x),rtol=1e-10,atol=1e-10);self.assertEqual(a.metadata['local_inner_iteration_cap'],16);self.assertEqual(a.metadata['local_factor']['pattern'],b.metadata['local_factor']['pattern']);self.assertGreater(x@a.solve(x),0);self.assertTrue(a.input_unchanged());refs=[weakref.ref(a.linear_pressure),weakref.ref(a.balanced),weakref.ref(a.coarse_factor.starts)];a.close();b.close();del a;gc.collect();self.assertTrue(all(r() is None for r in refs))
 def test_exact_declared_loop_transform(self):
     self.assertEqual((R / 'scripts/cfd_reference3d_p3_cg16_scalar_probe.py').read_text(), (R / 'scripts/cfd_reference3d_p3_cg8_scalar_probe.py').read_text().replace('p3_cg8_scalar_pressure', 'p3_cg16_scalar_pressure').replace('DistributedP3CG8ScalarPressureFactor', 'DistributedP3CG16ScalarPressureFactor'))
if __name__=='__main__':unittest.main()
