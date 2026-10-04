"""Independent full pressure span, balanced SPD/complement and Schur sign checks."""
import sys,unittest
from pathlib import Path
from types import SimpleNamespace
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_pressure_cubic import polynomial_basis,reserve,build,EXPONENTS
from cfd_reference3d_pressure_coarse import BalancedPressure,polynomial_basis as quadratic
class Cubic(unittest.TestCase):
 def test_mass_orthonormal_span_contains_all_twenty_and_preserves_quadratic_space(self):
  rng=np.random.default_rng(701);c=rng.random((70,3))*[8.,2.,2.];v=.1+rng.random(70);Z=polynomial_basis(c,v,.1,8.);Q=quadratic(c,v,.1,8.)
  np.testing.assert_allclose(Z.T@((v/.1)[:,None]*Z),np.eye(20),atol=1e-14)
  x,y,z=(2*c/np.array([8.,2.,2.])-1).T
  # Independent explicit monomial enumeration, not implementation-generated columns.
  raw=np.column_stack((np.ones(70),x,y,z,x*x,y*y,z*z,x*y,x*z,y*z,x**3,y**3,z**3,x*x*y,x*x*z,x*y*y,y*y*z,x*z*z,y*z*z,x*y*z))
  np.testing.assert_allclose(Z@np.linalg.lstsq(Z,raw,rcond=None)[0],raw,atol=1e-13)
  np.testing.assert_allclose(Z@np.linalg.lstsq(Z,Q,rcond=None)[0],Q,atol=1e-13)
  self.assertEqual(len(set(EXPONENTS)),20);self.assertEqual(sum(sum(e)==3 for e in EXPONENTS),10)
 def test_balanced_inverse_spd_coarse_reproduction_and_complement_not_projected(self):
  rng=np.random.default_rng(702);n=47;A=rng.normal(size=(n,n));S=A.T@A+3*np.eye(n);Z=np.linalg.qr(rng.normal(size=(n,20)))[0];M=.1+rng.random(n)
  p=BalancedPressure(Z,S@Z,M);E=Z@np.linalg.solve(Z.T@S@Z,Z.T);I=np.eye(n);expected=E+(I-E@S)@np.diag(M)@(I-S@E)
  actual=p.apply(I);np.testing.assert_allclose(actual,expected,atol=2e-13,rtol=1e-12);np.testing.assert_allclose(actual,actual.T,atol=2e-13);self.assertGreater(np.linalg.eigvalsh(actual).min(),0)
  np.testing.assert_allclose(p.apply(S@Z),Z,atol=1e-13);complement=np.linalg.qr(Z,mode='complete')[0][:,20:];self.assertGreater(np.linalg.norm(p.apply(complement)),.1)
 def test_build_coupled_schur_sign_original_data_and_twenty_column_reserve(self):
  rng=np.random.default_rng(703);np_=36;nv=45;c=rng.random((np_,3))*[4.,2.,2.];vol=.1+rng.random(np_);A=rng.normal(size=(nv,nv));A=A.T@A+3*np.eye(nv);B=rng.normal(size=(nv,np_));D=-np.diag(1+rng.random(np_))
  # Four-cell convention only supplies actual macro center carrier numbering here.
  mesh=SimpleNamespace(p=c.T,t=np.tile(np.arange(np_),(4,1)));before=[a.copy() for a in (vol,A,B,D)];p=build(mesh,vol,.1,4.,B,D,lambda v:np.linalg.solve(A,v));S=B.T@np.linalg.solve(A,B)-D
  np.testing.assert_allclose(p.W,S@p.Z,atol=1e-13);np.testing.assert_allclose(p.apply(S@p.Z),p.Z,atol=1e-13)
  for a,b in zip((vol,A,B,D),before):np.testing.assert_array_equal(a,b)
  self.assertEqual(p.metadata['columns'],20);self.assertTrue(p.metadata['complete_cubic_span'] and p.metadata['constant_pressure_direction_retained']);self.assertEqual(p.metadata['coarse_reservation_bytes'],8*(3*np_*20+8*np_+4*nv+8*400)+2*2**20)
 def test_rank_geometry_nonfinite_and_reservation_refused(self):
  for c,v,mu,L in ((np.ones((20,3)),np.ones(20),.1,4.),(np.ones((19,3)),np.ones(19),.1,4.),(np.full((20,3),np.nan),np.ones(20),.1,4.),(np.ones((20,3)),np.full(20,np.inf),.1,4.),(np.ones((20,3)),np.zeros(20),.1,4.),(np.ones((20,3)),np.ones(20),float('inf'),4.)):
   with self.assertRaises(ValueError):polynomial_basis(c,v,mu,L)
  for args in ((10,19),(0,20),(10,20,10),(True,20)):
   with self.assertRaises(ValueError):reserve(*args)
  with self.assertRaises((ValueError,np.linalg.LinAlgError)):BalancedPressure(np.eye(20),np.zeros((20,20)),np.ones(20))
if __name__=='__main__':unittest.main()
