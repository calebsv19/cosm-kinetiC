import unittest,sys,math
from pathlib import Path
import numpy as np
from skfem import Basis,ElementDG
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_empty_duct_reference import conductance,baseline_diagnostics
from cfd_reference3d_empty_duct_mesh import empty_duct_mesh
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_quartic_pair import quartic_quadrature,CubicPressureMass
class EmptyDuct(unittest.TestCase):
 def test_independent_double_sine_flow_integral(self):
  odd=np.arange(1,768,2,dtype=float);m,n=np.meshgrid(odd,odd,indexing='ij')
  double=64*4/math.pi**6*np.sum(1/(m*m*n*n*(m*m/4+n*n/4)));one=conductance()['conductance_m4']
  self.assertLess(abs(one/double-1),2e-8)
 def test_positive_tail_scale_and_swap_symmetries(self):
  fine=conductance(2,2,1024)['conductance_m4']
  for terms in (32,64,128):
   c=conductance(2,2,terms);self.assertGreater(c['conductance_m4'],fine);self.assertLess(c['conductance_m4']-fine,c['positive_series_tail_bound_m4'])
  self.assertEqual(conductance(2,3),conductance(3,2));self.assertAlmostEqual(conductance(4,6)['conductance_m4']/conductance(2,3)['conductance_m4'],16,places=12)
 def test_actual_empty_mesh_volume_and_no_body(self):
  for length in (4.,8.):
   mesh,lo,hi,axes,macros=empty_duct_mesh(length,4,False,3,1,'original');self.assertEqual(mesh.nelements,24*int(length*2)*16);self.assertEqual(len(mesh.boundaries['body']),0);self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,4*length,places=12)
 def test_pressure_field_gate_detects_constant_pressure_error(self):
  mesh,*_=empty_duct_mesh(4.,4,False,3,1,'original');pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=quartic_quadrature(),elements=np.array([0]));mass=CubicPressureMass(pb,.1);P=.1*.008*4/conductance()['conductance_m4'];p=P*(1-pb.doflocs[0]/4)
  a=baseline_diagnostics(4.,.1,.008,P,P*.008,p,pb,mass);self.assertTrue(a['baseline_reference_accuracy_accepted']);self.assertEqual(a['errors']['pressure_field_mass_relative_error'],0.)
  b=baseline_diagnostics(4.,.1,.008,P,P*.008,p+.02*P,pb,mass);self.assertFalse(b['baseline_reference_accuracy_accepted']);self.assertGreater(b['errors']['pressure_field_mass_relative_error'],.001)
 def test_invalid_reference_and_mesh_reject(self):
  for h,w,terms in ((0,2,512),(2,float('nan'),512),(2,2,8),(2,2,5000)):
   with self.assertRaises(ValueError):conductance(h,w,terms)
  for args in ((4,2,False,3,1,'original'),(4,4,True,3,1,'original'),(4,4,False,3,2,'original')):
   with self.assertRaises(ValueError):empty_duct_mesh(*args)
if __name__=='__main__':unittest.main()
