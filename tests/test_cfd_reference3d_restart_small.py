"""Small restarts solve complete mixed equations; requested full target is strict."""
import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_flexible import flexible_gmres,basis_reservation
from cfd_reference3d_requested_target import requested_full_linear_acceptance
from cfd_reference3d_restart_small_probe import run as numerical_run
from cfd_reference3d_restart_small_stage_probe import run as stage_run

class Small(unittest.TestCase):
    def test_independent_mixed_solution_with_variable_float_inverse(self):
        rng=np.random.default_rng(84);nv=24;np_=4;G=rng.normal(size=(nv,nv));A=G@G.T+20*np.eye(nv);B=rng.normal(size=(nv,np_));K=np.block([[A,B],[B.T,-.2*np.eye(np_)]]);rhs=rng.normal(size=nv+np_)
        expected=np.linalg.solve(K,rhs)
        for restart in (8,4):
            calls=[0]
            def pc(x):
                calls[0]+=1;p=-x[nv:];v=np.linalg.solve(A.astype(np.float32),(x[:nv]-B@p).astype(np.float32)).astype(float)
                return np.r_[v,p]*(1.001 if calls[0]%2 else .999)
            x,info,d=flexible_gmres(K,rhs,pc,1e-10,3000,restart)
            self.assertTrue(requested_full_linear_acceptance(info,np.linalg.norm(K@x-rhs)/np.linalg.norm(rhs),1e-10))
            np.testing.assert_allclose(x,expected,rtol=1e-8,atol=1e-10)
            self.assertEqual(d['restart'],restart);self.assertEqual(d['basis_reservation_bytes'],basis_reservation(len(rhs),restart));self.assertGreater(d['basis_reservation_bytes'],d['basis_array_bytes'])

    def test_requested_full_target_rejects_legacy_residual_pass_and_bad_values(self):
        self.assertLess(1.1e-10,1e-8);self.assertFalse(requested_full_linear_acceptance(0,1.1e-10,1e-10))
        self.assertFalse(requested_full_linear_acceptance(0,1e-10,1e-10));self.assertTrue(requested_full_linear_acceptance(0,9e-11,1e-10))
        for value in (float('nan'),float('inf'),-1.):self.assertFalse(requested_full_linear_acceptance(0,value,1e-10))
        self.assertFalse(requested_full_linear_acceptance(1,1e-12,1e-10))
        for target in (0.,1.,float('nan')):
            with self.assertRaises(ValueError):requested_full_linear_acceptance(0,0.,target)

    def test_unselected_restarts_rejected_before_mesh_or_factor(self):
        for restart in (0,12,24,60):
            with self.assertRaises(ValueError):numerical_run(restart=restart)
            with self.assertRaises(ValueError):stage_run(restart=restart)

    def test_iteration_cap_and_both_basis_saving(self):
        K=np.diag(np.linspace(1,100,35));rhs=np.ones(35)
        x,info,d=flexible_gmres(K,rhs,lambda x:x,1e-10,1,4)
        self.assertFalse(requested_full_linear_acceptance(info,d['final_true_metric'],1e-10));self.assertEqual(d['iterations'],1)
        self.assertLess(basis_reservation(300000,4),basis_reservation(300000,8));self.assertLess(basis_reservation(300000,8),basis_reservation(300000,12))

if __name__=='__main__':unittest.main()
