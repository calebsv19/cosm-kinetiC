"""Actual Float accuracy, ideal polynomial, original FE and new owner lifecycle."""
import sys,unittest,weakref
from pathlib import Path
from types import SimpleNamespace
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_velocity_refine import RefinedVelocity,reserve
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_flexible import flexible_gmres
from cfd_reference3d_condensed import full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=library_path('build/c3d-encoded-operator/support/factor.dylib')
class Refine(unittest.TestCase):
 def test_actual_float_inverse_residual_improves_and_observation_bound(self):
  rng=np.random.default_rng(741);B=rng.normal(size=(24,24));A=B.T@B+3*np.eye(24);t=VectorTriangle(csr_matrix(np.triu(A)),LIB);f=MixedWorkspaceCholesky(t,LIB,pressure_control=False);pc=RefinedVelocity(t,f);h=t.input_sha256
  for _ in range(13):
   x=rng.normal(size=24);first=f.solve(x);z=pc.solve(x);r0=np.linalg.norm(t@first-x);r1=np.linalg.norm(t@z-x);self.assertLess(r1,r0*.001);np.testing.assert_allclose(z,np.linalg.solve(A,x),atol=1e-11,rtol=1e-11)
  self.assertEqual(pc.metadata['calls'],13);self.assertEqual(len(pc.metadata['coarse_accuracy_observations']),10);self.assertEqual(t.input_sha256,h);self.assertTrue(t.input_unchanged() and f.input_unchanged());f.close()
 def test_ideal_linear_polynomial_spd_formula_and_all_coordinates(self):
  rng=np.random.default_rng(742);B=rng.normal(size=(18,18));A=B.T@B+3*np.eye(18);G=np.linalg.inv(A+.02*np.eye(18));pc=RefinedVelocity(A,SimpleNamespace(n=18,solve=lambda x:G@x))
  actual=np.column_stack([pc.solve(x) for x in np.eye(18)]);expected=2*G-G@A@G
  np.testing.assert_allclose(actual,expected,atol=1e-15);np.testing.assert_allclose(actual,actual.T,atol=1e-15);self.assertGreater(np.linalg.eigvalsh(actual).min(),0)
 def test_anisotropic_original_fe_load_pressure_and_strict_velocity_unchanged(self):
  s=system_fixture();C=BlockTriangle(s.upper_matrix,len(s.retained_free)-s.nmacro);C.velocity=VectorTriangle(C.velocity.upper,LIB);f=MixedWorkspaceCholesky(C.velocity,LIB,pressure_control=False);pc=RefinedVelocity(C.velocity,f);rng=np.random.default_rng(743)
  x=rng.normal(size=C.shape[0]);before=C@x;rhs=rng.normal(size=3*s.ub.N+s.pb.N)*.01;reduced=s.reduce_rhs(rhs);z=np.zeros(s.shape[0]);z[s.retained_free]=x;u,p=s.reconstruct(z,rhs)
  arrays=tuple(a for m in (C.coupling,C.pressure.upper) for a in (m.indptr,m.indices,m.data));h=storage_sha(*arrays)
  corrected,info,_=flexible_gmres(C.velocity,x[:C.nv],pc.solve,target=1e-11,restart=6);self.assertEqual(info,0);self.assertLess(np.linalg.norm(C.velocity@corrected-x[:C.nv])/np.linalg.norm(x[:C.nv]),1e-11)
  np.testing.assert_array_equal(C@x,before);np.testing.assert_array_equal(s.reduce_rhs(rhs),reduced);uu,pp=s.reconstruct(z,rhs);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p);self.assertEqual(storage_sha(*arrays),h)
  ub,pb,A,B=assemble_quartic(s.mesh,.1,128);expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))];np.testing.assert_allclose(full_action(s.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10);f.close()
 def test_invalid_closed_output_and_borrowed_owner_reservation(self):
  t=VectorTriangle(csr_matrix(np.eye(6)),LIB);f=MixedWorkspaceCholesky(t,LIB,pressure_control=False);pc=RefinedVelocity(t,f);owner=weakref.ref(f);operator=weakref.ref(t);del f,t;self.assertIsNotNone(owner());self.assertIsNotNone(operator());owner().close()
  with self.assertRaises(ValueError):pc.solve(np.ones(6))
  del pc;self.assertIsNone(owner());self.assertIsNone(operator());self.assertEqual(reserve(6),64*6)
  for n in (0,-1,True,3.5):
   with self.assertRaises(ValueError):reserve(n)
  for solve in (lambda x:np.ones(2),lambda x:np.full(3,np.nan),lambda x:np.ones(3,dtype=np.float32)):
   pc=RefinedVelocity(np.eye(3),SimpleNamespace(n=3,solve=solve))
   with self.assertRaises(ValueError):pc.solve(np.ones(3))
  pc=RefinedVelocity(np.eye(3),SimpleNamespace(n=3,solve=lambda x:x.copy()))
  for x in (np.ones(2),np.full(3,np.inf)):
   with self.assertRaises(ValueError):pc.solve(x)
  with self.assertRaises(ValueError):RefinedVelocity(np.eye(3,dtype=np.float32),SimpleNamespace(n=3,solve=lambda x:x))
if __name__=='__main__':unittest.main()
