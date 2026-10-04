import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_pressure_coarse import BalancedPressure,polynomial_basis,reserve
class Coarse(unittest.TestCase):
    def test_balanced_matrix_spd_and_coarse_exactness_without_dropping_complement(self):
        rng=np.random.default_rng(304);n=31;A=rng.normal(size=(n,n));S=A.T@A+np.eye(n);Z=rng.normal(size=(n,10));mass=1+rng.random(n)
        p=BalancedPressure(Z,S@Z,mass);E=Z@np.linalg.solve(Z.T@S@Z,Z.T);I=np.eye(n);expected=E+(I-E@S)@np.diag(mass)@(I-S@E)
        actual=np.column_stack([p.apply(x) for x in I]);np.testing.assert_allclose(actual,expected,rtol=1e-12,atol=1e-12);np.testing.assert_allclose(actual,actual.T,atol=1e-13);self.assertGreater(np.linalg.eigvalsh(actual).min(),0)
        np.testing.assert_allclose(actual@S@Z,Z,atol=1e-12);q,_=np.linalg.qr(Z,mode='complete');self.assertGreater(np.linalg.norm(actual@q[:,10:]),.1)
    def test_polynomial_mass_span_contains_all_ten_and_constant(self):
        rng=np.random.default_rng(308);centers=rng.random((30,3))*np.array([4,2,2]);v=1+rng.random(30);Z=polynomial_basis(centers,v,.1,4)
        np.testing.assert_allclose(Z.T@((v/.1)[:,None]*Z),np.eye(10),atol=1e-14)
        x,y,z=(centers*2/np.array([4,2,2])-1).T;raw=np.column_stack((np.ones(30),x,y,z,x*x,y*y,z*z,x*y,x*z,y*z));np.testing.assert_allclose(Z@np.linalg.lstsq(Z,raw,rcond=None)[0],raw,atol=1e-13)
    def test_invalid_and_singular_refused(self):
        with self.assertRaises(ValueError):polynomial_basis(np.ones((10,3)),np.ones(10),.1,4)
        with self.assertRaises((ValueError,np.linalg.LinAlgError)):BalancedPressure(np.eye(10),np.zeros((10,10)),np.ones(10))
        with self.assertRaises(ValueError):BalancedPressure(np.eye(10),np.eye(10)+np.triu(np.ones((10,10)),1),np.ones(10))
        with self.assertRaises(ValueError):reserve(10,9)
    def test_coupled_schur_sign_and_complete_pressure_action(self):
        rng=np.random.default_rng(399);A=rng.normal(size=(36,36));A=A.T@A+np.eye(36);B=rng.normal(size=(36,30));D=-np.diag(1+rng.random(30));S=B.T@np.linalg.solve(A,B)-D;Z=rng.normal(size=(30,10));W=np.column_stack([B.T@np.linalg.solve(A,B@v)-D@v for v in Z.T]);p=BalancedPressure(Z,W,np.ones(30));np.testing.assert_allclose(p.apply(S@Z[:,0]),Z[:,0],atol=1e-13)
if __name__=='__main__':unittest.main()
