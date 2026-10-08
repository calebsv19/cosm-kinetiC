import hashlib,json,sys,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
import cfd_reference3d_packed_matched_profile as p
class Operator:
 shape=(3,3)
 def __matmul__(self,x):return x.copy()
class Coarse:
 def base_solve(self,x):return x.copy()
 def action(self,x):return x.copy()
 def solve(self,x):
  z=self.base_solve(x)
  for _ in range(2):z+=self.base_solve(x-self.action(z))
  return z
class Balanced:
 def __init__(self):self.velocity=Operator();self.inverse=lambda x:x.copy();self.coarse=Coarse()
 def solve(self,x):
  c=self.coarse.solve(x);z=self.inverse(x-self.velocity@c)
  return c+z-self.coarse.solve(self.velocity@z)
class Profile(unittest.TestCase):
 def fixture(self):
  b=Balanced();f=SimpleNamespace(balanced=b,coarse_factor=b.coarse,input_unchanged=lambda:True)
  pre=lambda x:np.r_[b.solve(x[:3]),-x[3:]]
  return f,pre
 def test_sequential_profile_preserves_action_counts_inputs_and_restores(self):
  f,pre=self.fixture();rhs=np.array([1.,2.,3.,4.]);before=rhs.copy();report=p.profile_actions(f,pre,rhs,3)
  np.testing.assert_array_equal(rhs,before);self.assertTrue(report['instrumentation_restored']);self.assertEqual(report['load_count'],7);self.assertEqual(report['call_counts'],dict(coarse_Float_solve=84,coarse_Double_action=56,coarse_correction=28,outer_velocity_action=28,local_inner8=14));self.assertGreaterEqual(report['remaining_balanced_and_pressure_s'],-1e-8);self.assertTrue(all(x>0 for x in report['momentum_positive_work']))
 def test_guards_and_exception_hook_restoration(self):
  f,pre=self.fixture();inverse=f.balanced.inverse;velocity=f.balanced.velocity;solve=f.coarse_factor.solve
  with self.assertRaises(ValueError):p.profile_actions(f,pre,np.array([1.,2.,3.,np.nan]),3)
  with self.assertRaises(ValueError):p.diagnostic_reserve(0)
  def fail(phase):raise RuntimeError('stop')
  with self.assertRaises(RuntimeError):p.profile_actions(f,pre,np.arange(1.,5.),3,fail)
  self.assertIs(f.balanced.inverse,inverse);self.assertIs(f.balanced.velocity,velocity);self.assertEqual(f.coarse_factor.solve,solve);self.assertEqual(p.diagnostic_reserve(4),8*8*4+2*2**20)
if __name__=='__main__':unittest.main()
