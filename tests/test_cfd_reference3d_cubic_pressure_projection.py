import sys,unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_cubic_pressure_projection import CubicPressureProjector,ALPHA,interval_trace
class Projection(unittest.TestCase):
 def test_all_twenty_polynomial_moments_on_anisotropic_clipped_box(self):
  mesh=MeshTet.init_tensor([0.,.7,2.],[0.,.4,1.4],[0.,.2,.7]);pb=Basis(mesh,ElementDG(ElementTetP3()),elements=np.array([0]));lower=np.array([.123,.1,.04]);upper=np.array([1.713,1.13,.64])
  for powers in ALPHA:
   pressure=np.prod(pb.doflocs**powers[:,None],axis=0);actual=CubicPressureProjector(mesh,pb,pressure).integrate_box(lower,upper)
   exact=float(np.prod((upper**(powers+1)-lower**(powers+1))/(powers+1)));self.assertAlmostEqual(actual['pressure_integral_pa_m3'],exact,delta=2e-10);self.assertAlmostEqual(actual['volume_m3'],float(np.prod(upper-lower)),delta=2e-11)
 def test_discontinuous_constants_keep_element_identity(self):
  mesh=MeshTet.init_tensor([0.,1.],[0.,1.],[0.,1.]);pb=Basis(mesh,ElementDG(ElementTetP3()),elements=np.array([0]));p=np.zeros(pb.N);labels=np.arange(mesh.nelements)+1.
  for c,label in enumerate(labels):p[pb.dofs.element_dofs[:,c]]=label
  value=CubicPressureProjector(mesh,pb,p).integrate_box(np.zeros(3),np.ones(3));expected=float(np.sum(np.abs(mesh.mapping().detA)/6*labels));self.assertAlmostEqual(value['pressure_integral_pa_m3'],expected,places=11)
 def test_native_stencil_is_quadratic_exact_and_cubic_bias_visible(self):
  h=.125
  for k in range(3):
   averages=[((j+1)**(k+1)-j**(k+1))*h**k/(k+1) for j in range(3)];self.assertAlmostEqual(interval_trace(averages),1. if k==0 else 0.,places=13);self.assertAlmostEqual(interval_trace(np.array(averages)+17),17+(1. if k==0 else 0.),places=12)
  cubic=[((j+1)**4-j**4)*h**3/4 for j in range(3)];self.assertAlmostEqual(interval_trace(cubic),1.5*h**3,places=13)
if __name__=='__main__':unittest.main()
