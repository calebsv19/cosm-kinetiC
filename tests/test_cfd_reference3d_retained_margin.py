"""Small restarts solve complete mixed equations; requested full target is strict."""
import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_flexible import flexible_gmres,basis_reservation
from cfd_reference3d_requested_target import requested_full_linear_acceptance
from cfd_reference3d_retained_margin_probe import run as numerical_run
from cfd_reference3d_retained_margin_stage_probe import run as stage_run

class Small(unittest.TestCase):
    def test_independent_mixed_solution_with_variable_float_inverse(self):
        rng=np.random.default_rng(84);nv=24;np_=4;G=rng.normal(size=(nv,nv));A=G@G.T+20*np.eye(nv);B=rng.normal(size=(nv,np_));K=np.block([[A,B],[B.T,-.2*np.eye(np_)]]);rhs=rng.normal(size=nv+np_)
        expected=np.linalg.solve(K,rhs)
        for restart in (6,):
            calls=[0]
            def pc(x):
                calls[0]+=1;p=-x[nv:];v=np.linalg.solve(A.astype(np.float32),(x[:nv]-B@p).astype(np.float32)).astype(float)
                return np.r_[v,p]*(1.001 if calls[0]%2 else .999)
            x,info,d=flexible_gmres(K,rhs,pc,1e-11,3000,restart)
            self.assertLessEqual(d['final_true_metric'],1e-11)
            self.assertTrue(requested_full_linear_acceptance(info,np.linalg.norm(K@x-rhs)/np.linalg.norm(rhs),1e-10))
            np.testing.assert_allclose(x,expected,rtol=1e-8,atol=1e-10)
            self.assertEqual(d['restart'],restart);self.assertEqual(d['basis_reservation_bytes'],basis_reservation(len(rhs),restart));self.assertGreater(d['basis_reservation_bytes'],d['basis_array_bytes'])

    def test_unselected_restarts_rejected_before_mesh_or_factor(self):
        for restart in (0,4,8,12,24,60):
            with self.assertRaises(ValueError):numerical_run(restart=restart)
            with self.assertRaises(ValueError):stage_run(restart=restart)

    def test_invalid_margins_reject_before_mesh_or_factor(self):
        for full,retained in ((1e-10,1e-10),(1e-10,1e-9),(1e-10,0.),(float('nan'),1e-11),(1e-10,float('inf'))):
            with self.assertRaises(ValueError):numerical_run(target=full,retained_target=retained)

if __name__=='__main__':unittest.main()
