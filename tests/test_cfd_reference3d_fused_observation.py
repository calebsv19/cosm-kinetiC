import unittest,sys
from pathlib import Path
import numpy as np
from skfem import Basis,ElementDG
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_p4 import ElementTetP4
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_quartic_pair import quartic_quadrature
from cfd_reference3d_chunked import volume_metrics
from cfd_reference3d_quartic_observation import quartic_consistency
from cfd_reference3d_fused_observation import observe_fused
class Fused(unittest.TestCase):
 def test_affine_manufactured_metrics_and_independent_old_observers(self):
  mesh,lo,hi,axes,_=domain_mesh(4.,2,False,3,1,'original')
  ub=Basis(mesh,ElementTetP4(),quadrature=quartic_quadrature(),elements=np.array([0]));pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=ub.quadrature,elements=np.array([0]))
  G=np.array([[1.,.5,0.],[0.,-.3,.2],[.4,0.,.7]]);u=G@ub.doflocs;p=1.+np.array([.3,-.2,.1])@pb.doflocs;mu=.02
  metrics,diagnostic=observe_fused(mesh,ub,pb,u,p,mu,lo,hi,512)
  oldmetrics=volume_metrics(mesh,ub,u,mu,512);old=quartic_consistency(mesh,ub,pb,u,p,mu,lo,hi,512)
  self.assertEqual(metrics,oldmetrics);self.assertEqual(diagnostic,old)
  volume=15.;e=.5*(G+G.T)
  np.testing.assert_allclose(metrics,[2*mu*np.sum(e*e)*volume,abs(np.trace(G))*volume**.5,abs(np.trace(G))],rtol=3e-12,atol=3e-12)
if __name__=='__main__':unittest.main()
