import unittest,sys,gc,weakref,json
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix,triu
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from test_cfd_reference3d_distributed_p3_cg8 import fixture,dense_fixture,LOCAL,COARSE
from cfd_reference3d_distributed_p3_cg8_pressure import DistributedP3CG8PressureFactor
from cfd_reference3d_distributed_p3 import DistributedP3Factor
from cfd_reference3d_distributed_p3_cg8 import inner_cg8
from cfd_reference3d_pressure_complement10 import BalancedPressure
from cfd_reference3d_vector_storage import VectorTriangle
class FixedPressure(unittest.TestCase):
 def test_fixed_pressure_columns_prior_P3_and_common_owners(self):
  old,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LOCAL);kwargs=dict(pressure_control=False,coarse_library=COARSE,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata)
  a=DistributedP3CG8PressureFactor(t,LOCAL,**kwargs);b=DistributedP3Factor(t,LOCAL,**kwargs);rng=np.random.default_rng(513);rhs=rng.normal(size=(nv,10));W=np.column_stack([a.pressure_solve(x) for x in rhs.T]);oldW=np.column_stack([b.solve(x) for x in rhs.T]);self.assertLess(np.linalg.norm(W-oldW)/np.linalg.norm(oldW),1e-10);np.testing.assert_allclose(W,oldW,rtol=1e-10,atol=1e-10);self.assertIs(a.linear_pressure.Z,a.Z);self.assertIs(a.linear_pressure.coarse,a.coarse_factor);self.assertIs(a.linear_pressure.velocity,a.owner);self.assertTrue(a.input_unchanged());self.assertGreater(rhs[:,0]@a.solve(rhs[:,0]),0);ref=weakref.ref(a.linear_pressure);a.close();b.close();self.assertIsNone(ref())
 def test_nonlinear_symmetry_rejection_and_fixed_proxy_spd(self):
  rng=np.random.default_rng(812);Q,_=np.linalg.qr(rng.normal(size=(80,80)));A=(Q*np.geomspace(1,1000,80))@Q.T;g=1/np.diag(A);B=rng.normal(size=(80,10));W=B.T@np.column_stack([inner_cg8(lambda x:A@x,lambda x:g*x,v) for v in B.T]);self.assertGreater(np.linalg.norm(W-W.T)/np.linalg.norm(W),1e-5)
  with self.assertRaises(ValueError):BalancedPressure(np.eye(10),W,np.ones(10))
  G=np.diag(g);T=3*G-3*G@A@G+G@A@G@A@G;fixed=B.T@T@B;pc=BalancedPressure(np.eye(10),fixed,np.ones(10));self.assertLess(pc.metadata['coarse_relative_skew'],1e-5);self.assertGreater(pc.metadata['coarse_smallest_eigenvalue'],0);np.testing.assert_allclose(pc.apply(fixed),np.eye(10),rtol=1e-10,atol=1e-10)
 def test_partial_and_repeated_extra_owner_cleanup(self):
  rng,A,Z,C=dense_fixture();t=VectorTriangle(triu(csr_matrix(A),format='csr'),LOCAL)
  for i in range(20):
   a=DistributedP3CG8PressureFactor(t,LOCAL,pressure_control=False,coarse_library=COARSE,Z=Z,coarse_upper=C);refs=[weakref.ref(a.linear_pressure),weakref.ref(a.balanced),weakref.ref(a.coarse_factor.starts)];a.close();a.close();del a;gc.collect();self.assertTrue(all(r() is None for r in refs))
  def stop(phase):
   if phase=='fixed_pressure_velocity_ready':raise ValueError('stopped after extra owner')
  with self.assertRaises(ValueError):DistributedP3CG8PressureFactor(t,LOCAL,pressure_control=False,coarse_library=COARSE,Z=Z,coarse_upper=C,stage_callback=stop)
 def test_exact_probe_transform_and_work_reservation_reuse(self):
     from cfd_reference3d_distributed_p3_cg8_pressure import work_reserve
     self.assertEqual(work_reserve(100, 20), 8 * (40 * 100 + 24 * 20) + 2 * 2 ** 20)
if __name__=='__main__':unittest.main()
