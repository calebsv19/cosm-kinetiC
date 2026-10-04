import unittest
import numpy as np
from scipy.sparse import csr_matrix
from scripts.cfd_reference3d_pressure_coverage import basis,projected,reserve
from cfd_reference3d_pressure_coarse import BalancedPressure
class PressureModes(unittest.TestCase):
 def geometry(self,n=100):
  r=np.random.default_rng(54);return r.uniform([0,0,0],[4,2,2],(n,3)),r.uniform(.01,.02,n),np.array([1.5,.5,.5]),np.array([2.5,1.5,1.5])
 def test_basis_span_and_budget(self):
  c,v,lo,hi=self.geometry();Q,m=basis(c,v,.1,4.,lo,hi)
  self.assertEqual(m['proposed_columns'],58);self.assertEqual(m['columns'],58);self.assertLess(np.linalg.norm(Q.T@Q-np.eye(58)),1e-12)
  x,y,z=(2*c/np.array([4,2,2])-1).T;root=np.sqrt(v/.1)
  raw=root[:,None]*np.column_stack((np.ones(len(c)),x,y,z,x*x,y*y,z*z,x*y,x*z,y*z))
  self.assertLess(np.linalg.norm(raw-Q[:,:10]@(Q[:,:10].T@raw)),1e-12)
  self.assertEqual(reserve(200,100),8*(8*100*64+12*200+8*64**2))
 def test_independent_dense_schur_and_outside_residual(self):
  c,v,lo,hi=self.geometry();root=np.sqrt(v/.1);Q,m=basis(c,v,.1,4.,lo,hi);r=np.random.default_rng(17);B=r.normal(size=(130,100))*.01;A=np.diag(r.uniform(1,2,130));D=-np.diag(r.uniform(.1,.2,100));S=B.T@np.linalg.solve(A,B)-D;Y=(S@(Q/root[:,None]))/root[:,None]
  Z=Q[:,:10]/root[:,None];pc=BalancedPressure(Z,S@Z,1/root**2);out=projected(Q,Y,root,pc,c,lo,hi,4.)
  K=Q.T@Y;e,U=np.linalg.eigh((K+K.T)/2);self.assertTrue(np.allclose(e,out['approximate_mass_schur_projected_eigenvalues']))
  q=Q@U[:,0];y=Y@U[:,0];self.assertAlmostEqual(np.linalg.norm(y-e[0]*q)/np.linalg.norm(y),out['lowest_sampled_modes'][0]['outside_sample_relative_residual'])
  self.assertGreater(out['lowest_sampled_modes'][0]['outside_sample_relative_residual'],.01);self.assertFalse(out['complete_spectrum_certified'])
 def test_complete_span_and_localization(self):
  n=12;c,v,lo,hi=self.geometry(n);Q=np.eye(n);root=np.ones(n);S=np.diag(np.arange(1,n+1.));pc=BalancedPressure(Q[:,:10],S@Q[:,:10],root);out=projected(Q,S,root,pc,c,lo,hi,4.)
  self.assertEqual(out['lowest_sampled_modes'][0]['outside_sample_relative_residual'],0)
  self.assertEqual(out['lowest_sampled_modes'][0]['ten_polynomial_mass_energy_fraction'],1)
  self.assertEqual(out['lowest_sampled_modes'][0]['end_mass_energy_fraction'],float(c[0,0]<.8 or c[0,0]>3.2))
 def test_refusals(self):
  for nv,np_ in ((0,10),(1,9),(1.5,20)):
   with self.assertRaises(ValueError):reserve(nv,np_)
  c,v,lo,hi=self.geometry();Q,m=basis(c,v,.1,4.,lo,hi);root=np.ones(len(c));pc=BalancedPressure(Q[:,:10],Q[:,:10],root)
  for Y in (-Q,np.full_like(Q,np.nan)):
   with self.assertRaises(ValueError):projected(Q,Y,root,pc,c,lo,hi,4.)
  with self.assertRaises(ValueError):basis(np.zeros_like(c),v,.1,4.,lo,hi)
if __name__=='__main__':unittest.main()
