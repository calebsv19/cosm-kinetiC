import json,sys,unittest
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_accuracy_evidence import physical_comparison
from cfd_reference3d_empty_duct_reference import conductance
class Physics(unittest.TestCase):
 def field(self):
  return dict(pressure_force_n=[.6,0,0],raw_symmetric_viscous_force_n=[.4,0,0],reaction_force_n=[1.,0,0],inlet_pressure_pa=1.,physical_dissipation_w=2.)
 def test_separate_force_changes_cannot_cancel(self):
  old=self.field();new=self.field();new['pressure_force_n'][0]+=.02;new['raw_symmetric_viscous_force_n'][0]-=.02
  result=physical_comparison(old,new,'same_domain_refinement');self.assertFalse(result['force_change_passed']);self.assertTrue(result['raw_equilibrium_passed']);self.assertFalse(result['physical_accuracy_certified'])
 def test_length_dependent_scalars_remain_visible_and_refinement_is_checked(self):
  old=self.field();new=self.field();new['inlet_pressure_pa']=1.3;new['physical_dissipation_w']=2.6
  domain=physical_comparison(old,new,'domain_sensitivity');self.assertTrue(domain['force_change_passed']);self.assertIsNone(domain['scalar_refinement_passed']);self.assertAlmostEqual(domain['inlet_pressure_dissipation_relative_changes']['inlet_pressure_pa'],.3)
  same=physical_comparison(old,new,'same_domain_refinement');self.assertFalse(same['scalar_refinement_passed'])
  c=conductance()['conductance_m4'];mu=.01;Q=.008
  pressure=[mu*Q*L/c for L in (4.,8.)];power=[p*Q for p in pressure];self.assertEqual(pressure[1]/pressure[0],2.);self.assertEqual(power[1]/power[0],2.)
 def test_raw_equilibrium_failure_remains_independent(self):
  new=self.field();new['reaction_force_n'][0]=1.018
  result=physical_comparison(new,new,'same_domain_refinement');self.assertTrue(result['force_change_passed']);self.assertFalse(result['raw_equilibrium_passed']);self.assertGreater(result['raw_surface_reaction_relative_mismatch'],.01)
if __name__=='__main__':unittest.main()
