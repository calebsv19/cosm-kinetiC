import sys,unittest,copy
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_floor_edge8_mesh import body_nodes,build,symmetry,evaluate,quality,SPACINGS
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
class FloorEdge8(unittest.TestCase):
 def test_profile_minimum_and_maximum_resolution(self):
  floor=.5*(1-np.cos(np.pi/6))
  for h in SPACINGS:
   a=body_nodes(h);self.assertEqual(len(a),9);self.assertEqual(a[4],.5);self.assertGreaterEqual(np.diff(a).min(),floor-1e-14);self.assertLessEqual(np.diff(a).max(),.2+1e-14);np.testing.assert_allclose(a+a[::-1],1,atol=1e-14)
 def test_actual_dual_volume_areas_counts_reflections(self):
  for L,count in ((4.,36096),(8.,43008)):
   m,lo,hi,axes,n=build(L,.07);p=m.p[:,m.t];det=np.linalg.det((p[:,1:]-p[:,0,None]).transpose(2,0,1));self.assertGreater(abs(det).min(),0);self.assertAlmostEqual(abs(det).sum()/6,4*L-1,places=10);self.assertEqual(m.nelements,count);self.assertEqual(4*n,count);self.assertTrue(symmetry(m,L));self.assertEqual(len(m.boundaries['body']),768)
   for name,v in dict(body=6,inlet=4,outlet=4,walls=8*L).items():
    p=m.p[:,m.facets[:,m.boundaries[name]]];area=np.linalg.norm(np.cross((p[:,1]-p[:,0]).T,(p[:,2]-p[:,0]).T),axis=1).sum()/2;self.assertAlmostEqual(area,v,places=10)
 def test_far_planes_retained_and_inner_match(self):
  for L in (4.,8.):
   old=second_normal_tensor_mesh(L)[0];new=build(L,.07);a=old[3][0];b=new[3][0];lo=old[1][0];np.testing.assert_array_equal(a[a<=lo-.1875+1e-12],b[b<=lo-.1875+1e-12]);self.assertTrue(np.any(np.isclose(b,lo-.046875)));self.assertFalse(np.any(np.isclose(b,lo-.09375)))
  a=build(4.,.07);b=build(8.,.07);np.testing.assert_allclose(a[3][0][(a[3][0]>=1.3125)&(a[3][0]<=2.6875)]+2,b[3][0][(b[3][0]>=3.3125)&(b[3][0]<=4.6875)],atol=1e-14)
 def test_invalid_and_worsened_quality(self):
  for args in ((6.,.07),(4.,.055),(8.,float('nan'))):
   with self.assertRaises(ValueError):build(*args)
  m,lo,hi,axes,n=second_normal_tensor_mesh(4.)[0];old=quality(m,lo,hi,4.);new=copy.deepcopy(old);new['maximum_body_triangle_edge_m']*=.8;new['body_triangles']=768;new['bands']['edge-0.05']['weighted_mean_shape']*=.98;self.assertEqual(evaluate(old,new,4.,.07),[]);new['bands']['body-0.2']['max_condition']*=1.02;self.assertIn('body-0.2 max_condition worsened',evaluate(old,new,4.,.07))
if __name__=='__main__':unittest.main()
