import unittest,sys,json
from pathlib import Path
from unittest.mock import patch
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_pressure_coverage16 import make_basis,reserve,diagnostic_reserve,COEFFICIENT_PATH
from cfd_reference3d_pressure_complement10 import polynomial_basis,BalancedPressure
from cfd_reference3d_pressure_coverage import basis
class Coverage16(unittest.TestCase):
 def fixture(self):
  rng=np.random.default_rng(816);centers=rng.random((150,3))*np.array([4.,2.,2.]);volumes=rng.random(150)+1.;lo=np.array([1.5,.5,.5]);hi=lo+1.;return rng,centers,volumes,lo,hi
 def test_original_ten_bitwise_and_transported_rank_coverage(self):
  rng,c,v,lo,hi=self.fixture();Z,m=make_basis(c,v,.1,4.,lo,hi);np.testing.assert_array_equal(Z[:,:10],polynomial_basis(c,v,.1,4.));np.testing.assert_allclose(Z.T@((v/.1)[:,None]*Z),np.eye(16),atol=1e-10);self.assertTrue(m['original_ten_retained_bitwise']);data=json.loads(COEFFICIENT_PATH.read_text());C=np.asarray(data['coefficients']);self.assertEqual(C.shape,(58,6));np.testing.assert_array_equal(C[:10],np.zeros((10,6)));np.testing.assert_allclose(C.T@C,np.eye(6),atol=1e-10);self.assertGreater(min(data['selected_sampled_mass_coverage']),.999999)
 def test_pressure16_model_spd_reproduction_and_equations_untouched(self):
  rng,c,v,lo,hi=self.fixture();Z,_=make_basis(c,v,.1,4.,lo,hi);M=rng.normal(size=(150,150));S=M@M.T+np.eye(150);before=S.copy();W=S@Z;pc=BalancedPressure(Z,W,10*.1/v);H=pc.apply(np.eye(150));np.testing.assert_allclose(H,H.T,rtol=1e-9,atol=1e-9);self.assertGreater(np.linalg.eigvalsh(H).min(),0);np.testing.assert_allclose(pc.apply(W),Z,atol=1e-10);np.testing.assert_array_equal(S,before);self.assertEqual(pc.metadata['columns'],16)
 def test_incompatible_prescribed_names_reject_and_budgets(self):
  rng,c,v,lo,hi=self.fixture();Q,m=basis(c,v,.1,4.,lo,hi);m['kept']=list(m['kept']);m['kept'][0]='incompatible'
  with patch('cfd_reference3d_pressure_coverage16.basis',return_value=(Q,m)):
   with self.assertRaises(ValueError):make_basis(c,v,.1,4.,lo,hi)
  self.assertGreater(reserve(100,20),8*(3*20*16+8*20+4*100));self.assertGreater(diagnostic_reserve(100,20),8*4*100*16)
  with self.assertRaises(ValueError):reserve(0,20)
 def test_exact_control_and_full_probe_transforms(self):
  t=json.loads((R/'build/c3d-pressure-coverage16/control-transform-control.json').read_text());s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:self.assertIn(a,s);s=s.replace(a,b)
  self.assertEqual(s,(R/t['output']).read_text());old=(R/'scripts/cfd_reference3d_p3_cg8_scalar_probe.py').read_text();expected=old.replace('from cfd_reference3d_pressure_complement10 import build as build_pressure_coarse,reserve as coarse_reserve','from cfd_reference3d_pressure_coverage16 import build as build_pressure_coarse,reserve as coarse_reserve').replace('build_pressure_coarse(mesh,system.volumes,mu,length,C.coupling,C.pressure,exact_factor.pressure_solve)','build_pressure_coarse(mesh,system.volumes,mu,length,C.coupling,C.pressure,exact_factor.pressure_solve,lo=lo,hi=hi,sample=sample)');self.assertEqual(expected,(R/'scripts/cfd_reference3d_pressure_coverage16_probe.py').read_text())
if __name__=='__main__':unittest.main()
