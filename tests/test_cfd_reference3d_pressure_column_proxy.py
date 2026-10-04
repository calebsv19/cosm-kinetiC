import unittest,sys
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_pressure_column_proxy import columns,stage_admission,diagnostic_reserve,select_scale
from cfd_reference3d_pressure_complement10 import BalancedPressure
from cfd_reference3d_pressure_coverage import projected
class Diagnostic(unittest.TestCase):
 def test_independent_dense_column_energy_and_residual_metrics(self):
  rng=np.random.default_rng(73);m=rng.normal(size=(20,20));A=m@m.T+np.eye(20);B=rng.normal(size=(20,12));Z=rng.normal(size=(12,10));rhs=B@Z;exact=np.linalg.solve(A,rhs);W=B.T@exact+.01*Z;x=columns(lambda x:A@x,rhs,exact,Z,W,exact,W);self.assertLess(max(x['velocity_true_relative_residuals']),1e-13);self.assertLess(x['coarse_relative_skew'],1e-13);self.assertTrue(all(t>0 for t in x['velocity_positive_work']));self.assertEqual(x['velocity_solution_relative_errors'],[0.]*10)
  y=columns(lambda x:A@x,rhs,.5*exact,Z,.5*W,exact,W);np.testing.assert_allclose(y['velocity_true_relative_residuals'],.5,atol=1e-13);self.assertAlmostEqual(y['projected_coarse_relative_error'],.5);self.assertAlmostEqual(y['schur_column_relative_error'],.5)
 def test_declared_sample_selection_both_proxies_and_invalid(self):
  r={'fixed':{5:5.,10:4.,20:2.,30:3.},'qualified_float':{5:6.,10:5.,20:3.,30:4.}};x=select_scale(r);self.assertEqual(x['selected_scale'],20);self.assertTrue(x['eligibility_gate_passed']);self.assertEqual(x['worst_relative_sampled_condition']['20'],.6);r['qualified_float'][20]=5.;r['qualified_float'][30]=5.;self.assertIsNone(select_scale(r)['selected_scale'])
  with self.assertRaises(ValueError):select_scale({'fixed':r['fixed']})
 def test_full_fresh_budget_and_same_pressure_spd_projection(self):
  a=stage_admission(100,200,300,400);self.assertEqual(a['estimated_numeric_stage_bytes'],100+200+300+400+32*2**20);self.assertTrue(a['numeric_stage_admitted']);self.assertFalse(stage_admission(1800*2**20,0,0,1)['numeric_stage_admitted']);self.assertGreater(diagnostic_reserve(100,20),8*5*100*10)
  rng=np.random.default_rng(71);Z,_=np.linalg.qr(rng.normal(size=(18,10)));M=rng.normal(size=(18,18));S=M@M.T+np.eye(18);W=S@Z
  for scale in (5,10,20,30):
   pc=BalancedPressure(Z,W,scale*np.ones(18));H=pc.apply(np.eye(18));np.testing.assert_allclose(H,H.T,atol=1e-10);self.assertGreater(np.linalg.eigvalsh(H).min(),0);np.testing.assert_allclose(pc.apply(W),Z,atol=1e-10)
 def test_real_separated_Float_and_Double_ABI_physical_action(self):
  from test_cfd_reference3d_distributed_p3 import dense_fixture,LOCAL
  from cfd_reference3d_vector_storage import VectorTriangle
  from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky
  from cfd_reference3d_distributed_p2 import ExactCoarse
  from scipy.sparse import csr_matrix,triu
  rng,A,_,_=dense_fixture();upper=triu(csr_matrix(A),format='csr');triangle=VectorTriangle(upper,LOCAL);f=MixedWorkspaceCholesky(triangle,LOCAL,pressure_control=False);d=ExactCoarse(upper,R/'build/c3d-distributed-p2/support/coarse.dylib');rhs=rng.normal(size=24);np.testing.assert_allclose(A@d.solve(rhs),rhs,rtol=1e-11,atol=1e-11);self.assertLess(np.linalg.norm(A@f.solve(rhs)-rhs)/np.linalg.norm(rhs),1e-5);self.assertTrue(f.input_unchanged() and d.input_unchanged());f.close();d.close()
 def test_nonlinear_coarse_not_averaged_and_invalid_columns(self):
  Z=np.eye(10);W=np.eye(10);W[0,1]=.1
  with self.assertRaises(ValueError):BalancedPressure(Z,W,np.ones(10))
  with self.assertRaises(ValueError):columns(lambda x:x,np.ones((10,10)),np.ones((10,9)),Z,W,np.ones((10,10)),W)
if __name__=='__main__':unittest.main()
