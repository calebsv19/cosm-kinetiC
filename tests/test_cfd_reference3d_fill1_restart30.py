"""Restart parameter preserves flexible mixed solve and accounts for both bases."""
import sys,unittest
from pathlib import Path
from unittest.mock import patch
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_flexible import flexible_gmres,basis_reservation
from cfd_reference3d_fill1_restart30_probe import run as numerical_run

class Restart(unittest.TestCase):
    def test_independent_mixed_solution_with_variable_float_inverse(self):
        rng=np.random.default_rng(2460);nv=30;np_=5;G=rng.normal(size=(nv,nv));A=G@G.T+20*np.eye(nv);B=rng.normal(size=(nv,np_));K=np.block([[A,B],[B.T,-.1*np.eye(np_)]]);rhs=rng.normal(size=nv+np_)
        expected=np.linalg.solve(K,rhs)
        for restart in (30,):
            calls=[0]
            def pc(x):
                calls[0]+=1;p=-x[nv:];v=np.linalg.solve(A.astype(np.float32),(x[:nv]-B@p).astype(np.float32)).astype(float)
                return np.r_[v,p]*(1.001 if calls[0]%2 else .999)
            x,info,d=flexible_gmres(K,rhs,pc,1e-11,3000,restart)
            self.assertEqual(info,0);self.assertEqual(d['restart'],restart)
            self.assertLess(np.linalg.norm(K@x-rhs)/np.linalg.norm(rhs),1e-11)
            np.testing.assert_allclose(x,expected,rtol=1e-8,atol=1e-10)

    def test_actual_numpy_v_and_z_allocations_reserved_for_each_restart(self):
        original=np.empty;n=31
        for restart in (30,):
            allocated=[]
            def capture(*args,**kwargs):
                a=original(*args,**kwargs)
                if a.shape in ((restart+1,n),(restart,n)):allocated.append(a)
                return a
            with patch('cfd_reference3d_flexible.np.empty',side_effect=capture):
                x,info,d=flexible_gmres(np.eye(n),np.ones(n),lambda x:x,1e-10,3000,restart)
            self.assertEqual(info,0);self.assertEqual(len(allocated),2)
            self.assertGreaterEqual(d['basis_array_bytes'],sum(a.nbytes for a in allocated));self.assertGreater(d['basis_reservation_bytes'],d['basis_array_bytes'])
            self.assertEqual(d['basis_reservation_bytes'],basis_reservation(n,restart))
        self.assertLess(basis_reservation(300000,12),basis_reservation(300000,24));self.assertLess(basis_reservation(300000,24),basis_reservation(300000,60))

    def test_iteration_stop_is_not_convergence(self):
        K=np.diag(np.linspace(1,100,35));rhs=np.ones(35)
        x,info,d=flexible_gmres(K,rhs,lambda x:x,1e-10,1,12)
        self.assertEqual(info,1);self.assertEqual(d['iterations'],1);self.assertGreater(d['final_true_metric'],1e-10)
        self.assertAlmostEqual(d['final_true_metric'],np.linalg.norm(K@x-rhs)/np.linalg.norm(rhs),places=14)

    def test_probe_parameter_rejected_before_any_mesh_or_factor(self):
        for restart in (0,6,12,24,60,61):
            with self.assertRaises(ValueError):numerical_run(restart=restart)

if __name__=='__main__':unittest.main()
