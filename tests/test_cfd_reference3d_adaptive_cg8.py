import unittest,sys,json,gc,weakref
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import cg
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_energy_cg8 import energy_cg8,new_statistics,ETA
class Adaptive(unittest.TestCase):
 def test_transformed_independent_Scipy_relative_proxy_and_counts(self):
  A=np.diag(np.linspace(1,3,24));g=np.linspace(.8,1.2,24);half=np.sqrt(g);rhs=np.ones(24);steps=[];z,info=cg(half[:,None]*A*half[None,:],half*rhs,rtol=ETA,atol=0,maxiter=8,callback=lambda x:steps.append(1));stats=new_statistics();actual=energy_cg8(lambda x:A@x,lambda x:g*x,rhs,stats);np.testing.assert_allclose(actual,half*z,rtol=1e-12,atol=1e-12);self.assertEqual(stats['steps'],len(steps));self.assertLess(stats['steps'],8);self.assertEqual(stats['factor_applications'],stats['steps']+1);self.assertEqual(stats['early_returns'],1);self.assertLessEqual(stats['last_trace'][-1]['relative_preconditioned_residual'],ETA)
 def test_cap_zero_curvature_and_bounded_work(self):
  rng=np.random.default_rng(812);Q,_=np.linalg.qr(rng.normal(size=(80,80)));A=(Q*np.geomspace(1,1000,80))@Q.T;rhs=rng.normal(size=80);stats=new_statistics();actual=energy_cg8(lambda x:A@x,lambda x:x,rhs,stats);expected,_=cg(A,rhs,rtol=ETA,atol=0,maxiter=8);np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-10);self.assertLessEqual(stats['steps'],8);self.assertLessEqual(stats['factor_applications'],8);self.assertLessEqual(len(stats['last_trace']),8)
  z=new_statistics();np.testing.assert_array_equal(energy_cg8(lambda x:x,lambda x:x,np.zeros(80),z),np.zeros(80));self.assertEqual(z['factor_applications'],0)
  with self.assertRaises(ValueError):energy_cg8(lambda x:-x,lambda x:x,rhs)
  with self.assertRaises(ValueError):energy_cg8(lambda x:x,lambda x:-x,rhs)
 def test_original_FE_fixed_pressure_unchanged_and_owners(self):
  from test_cfd_reference3d_distributed_p3_cg8 import fixture,LOCAL
  from cfd_reference3d_adaptive_cg8_pressure import AdaptiveCG8PressureFactor
  from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor
  from cfd_reference3d_vector_storage import VectorTriangle
  _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LOCAL);kw=dict(pressure_control=False,coarse_library=library_path('build/c3d-p3-cg8-scalar/support/coarse.dylib'),Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata);a=AdaptiveCG8PressureFactor(t,LOCAL,**kw);b=DistributedP3CG8ScalarPressureFactor(t,LOCAL,**kw);x=np.random.default_rng(71).normal(size=nv);np.testing.assert_allclose(a.pressure_solve(x),b.pressure_solve(x),rtol=1e-10,atol=1e-10);self.assertGreater(x@a.solve(x),0);self.assertTrue(a.input_unchanged());self.assertLessEqual(max(int(k) for k,v in a.inner_statistics['step_histogram'].items() if v),8);refs=[weakref.ref(a.linear_pressure),weakref.ref(a.balanced),weakref.ref(a.coarse_factor.starts)];a.close();b.close();del a;gc.collect();self.assertTrue(all(r() is None for r in refs))
 def test_exact_declared_factor_control_transforms_and_stats_histogram(self):
     stats = new_statistics()
     for i in range(20):
         energy_cg8(lambda x: 2 * x, lambda x: x, np.ones(12), stats)
     self.assertEqual(sum(stats['step_histogram'].values()), stats['calls'])
     self.assertEqual(stats['calls'], 20)
     self.assertLessEqual(len(stats['last_trace']), 8)
if __name__=='__main__':unittest.main()
