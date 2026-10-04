import sys,unittest,copy
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_edge_redistribution_mesh import body_nodes,build,quality,evaluate,symmetry
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
class EdgeRedistribution(unittest.TestCase):
 def test_nodes_symmetry_and_outside_planes(self):
  a=body_nodes(.06);np.testing.assert_allclose(a+a[::-1],1,atol=1e-14);self.assertEqual(len(a),7);self.assertEqual(a[3],.5);self.assertEqual(np.diff(a).max(),.25)
  old=second_normal_tensor_mesh(4.)[0];new=build(4.,.06)
  for i,(a,b) in enumerate(zip(old[3],new[3])):
   mask=(a<old[1][i]-1e-12)|(a>old[2][i]+1e-12);np.testing.assert_array_equal(a[mask],b[mask])
 def test_independent_volume_boundary_area_counts_reflections(self):
  m,lo,hi,axes,n=build(8.,.0625);p=m.p[:,m.t];det=np.linalg.det((p[:,1:]-p[:,0,None]).transpose(2,0,1));self.assertGreater(abs(det).min(),0);self.assertAlmostEqual(abs(det).sum()/6,31,places=10);self.assertEqual(m.nelements,33216);self.assertEqual(4*n,m.nelements);self.assertTrue(symmetry(m,8.))
  self.assertEqual(len(m.boundaries['body']),432)
  for name,v in dict(body=6,inlet=4,outlet=4,walls=64).items():
   p=m.p[:,m.facets[:,m.boundaries[name]]];area=np.linalg.norm(np.cross((p[:,1]-p[:,0]).T,(p[:,2]-p[:,0]).T),axis=1).sum()/2;self.assertAlmostEqual(area,v,places=10)
  center=m.p[:,m.t].mean(axis=1);self.assertFalse(np.any(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0)))
 def test_generic_quality_and_prospective_worsening_rejected(self):
  m,lo,hi,axes,n=second_normal_tensor_mesh(4.)[0];a=quality(m,lo,hi,4.);self.assertAlmostEqual(a['global_quality']['volume_m3'],15,places=10);self.assertTrue(a['physical_boundary_planes_preserved']);self.assertTrue(a['all_reflections_and_yz_exchange_preserved']);self.assertEqual(a['tetrahedra'],28416)
  b=copy.deepcopy(a);b['bands']['edge-0.05']['weighted_mean_shape']*=.98;self.assertEqual(evaluate(a,b,4.,.06),[])
  b['bands']['body-0.1']['max_condition']*=1.1;self.assertIn('body-0.1 max_condition worsened',evaluate(a,b,4.,.06));b['global_quality']['weighted_mean_shape']*=1.1;self.assertIn('global weighted_mean_shape worsened',evaluate(a,b,4.,.06))
 def test_undeclared_inputs_refused(self):
  for h in (0,.05,.07,float('nan')):
   with self.assertRaises(ValueError):body_nodes(h)
  with self.assertRaises(ValueError):build(6.,.06)
if __name__=='__main__':unittest.main()
