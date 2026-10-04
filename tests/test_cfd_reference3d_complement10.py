import sys,unittest
from pathlib import Path
from types import SimpleNamespace
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_pressure_complement10 import build,reserve,BalancedPressure
from cfd_reference3d_pressure_coarse import build as original
class Complement10(unittest.TestCase):
 def test_independent_balanced_formula_spd_and_reproduction(self):
  r=np.random.default_rng(70);n=45;A=r.normal(size=(n,n));S=A.T@A+2*np.eye(n);Z=np.linalg.qr(r.normal(size=(n,10)))[0];M=r.uniform(.01,.03,n);P=BalancedPressure(Z,S@Z,10*M);E=Z@np.linalg.solve(Z.T@S@Z,Z.T);I=np.eye(n)
  expected=E+10*(I-E@S)@np.diag(M)@(I-S@E);actual=P.apply(I);np.testing.assert_allclose(actual,expected,atol=1e-13);np.testing.assert_allclose(actual,actual.T,atol=1e-13);self.assertGreater(np.linalg.eigvalsh(actual).min(),0);np.testing.assert_allclose(P.apply(S@Z),Z,atol=1e-13)
 def test_build_same_schur_and_original_data(self):
  r=np.random.default_rng(71);np_=35;nv=50;c=r.random((np_,3))*[4,2,2];vol=r.uniform(.1,1,np_);A=r.normal(size=(nv,nv));A=A.T@A+2*np.eye(nv);B=r.normal(size=(nv,np_));D=-np.diag(r.uniform(.1,1,np_));mesh=SimpleNamespace(p=c.T,t=np.tile(np.arange(np_),(4,1)));before=[a.copy() for a in (vol,A,B,D)];G=lambda x:np.linalg.solve(A,x);new=build(mesh,vol,.1,4.,B,D,G);old=original(mesh,vol,.1,4.,B,D,G)
  np.testing.assert_array_equal(new.Z,old.Z);np.testing.assert_array_equal(new.W,old.W);np.testing.assert_allclose(new.mass_inverse,10*old.mass_inverse,rtol=1e-15)
  for a,b in zip((vol,A,B,D),before):np.testing.assert_array_equal(a.view(np.uint64),b.view(np.uint64))
  self.assertEqual(new.metadata['complementary_mass_inverse_scale'],10);self.assertEqual(new.metadata['coarse_correction_scale'],1);self.assertEqual(new.metadata['columns'],10)
 def test_complement_changed_coarse_unchanged(self):
  n=18;S=np.diag(np.arange(1,n+1.));Z=np.eye(n)[:,:10];M=np.ones(n);a=BalancedPressure(Z,S@Z,M);b=BalancedPressure(Z,S@Z,10*M)
  np.testing.assert_allclose(a.apply(S@Z),b.apply(S@Z),atol=1e-14);np.testing.assert_allclose(b.apply(np.eye(n)[:,10:]),10*a.apply(np.eye(n)[:,10:]),atol=1e-14)
 def test_rank_nonfinite_and_budget(self):
  for args in ((0,10),(20,9),(20,10,20)):
   with self.assertRaises(ValueError):reserve(*args)
  self.assertEqual(reserve(50,35),8*(3*35*10+8*35+4*50+8*100)+2*2**20)
  with self.assertRaises(ValueError):BalancedPressure(np.eye(10),np.full((10,10),np.nan),np.ones(10))
  with self.assertRaises((ValueError,np.linalg.LinAlgError)):BalancedPressure(np.eye(10),np.zeros((10,10)),np.ones(10))
if __name__=='__main__':unittest.main()
