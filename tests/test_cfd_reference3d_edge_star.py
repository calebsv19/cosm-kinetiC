import unittest,sys
from pathlib import Path
import numpy as np
from skfem import MeshTet
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_edge_star_mesh import split_edges,selected_edges
class EdgeStar(unittest.TestCase):
 def test_affine_first_moment_and_parent_volume(self):
  p=np.array([[0,0,0],[8,0,0],[0,1,0],[0,0,1]],float).T;m=MeshTet(p,np.arange(4)[:,None]);n,parent,_=split_edges(m,selected_edges(m,[0],'longest'))
  v=n.p[:,n.t];volume=np.abs(np.linalg.det((v[:,1:]-v[:,:1]).transpose(2,0,1)))/6
  self.assertAlmostEqual(volume.sum(),8/6,places=13);np.testing.assert_allclose(v.mean(axis=1)@volume,p.mean(axis=1)*(8/6),rtol=2e-13,atol=2e-13);self.assertTrue(np.all(parent==0))
 def test_complete_shared_edge_star_has_no_internal_boundary(self):
  m=MeshTet.init_tensor(*([np.array([0.,1.])]*3));n,parent,_=split_edges(m,[(0,7)])
  self.assertEqual(n.nelements,2*m.nelements);x=n.p[:,n.facets[:,n.boundary_facets()]].mean(axis=1)
  self.assertTrue(np.all(np.any(np.isclose(x,0)|np.isclose(x,1),axis=0)));np.testing.assert_array_equal(np.bincount(parent),np.full(m.nelements,2))
 def test_all_edges_cover_parent_and_do_not_mutate_input(self):
  p=np.eye(3,4);m=MeshTet(p,np.arange(4)[:,None]);before_p=m.p.copy();before_t=m.t.copy();n,parent,_=split_edges(m,selected_edges(m,[0],'all'));self.assertGreaterEqual(n.nelements,8)
  np.testing.assert_array_equal(m.p,before_p);np.testing.assert_array_equal(m.t,before_t);self.assertTrue(np.all(parent==0))
 def test_invalid_marks_and_nonexistent_edges_reject(self):
  m=MeshTet.init_tensor(*([np.array([0.,1.])]*3))
  for selected,method in (([], 'longest'),([0,0],'all'),([-1],'longest'),([0],'bad')):
   with self.assertRaises(ValueError):selected_edges(m,selected,method)
  for edges in ([],[(0,99)],[(0,0)]):
   with self.assertRaises(ValueError):split_edges(m,edges)
if __name__=='__main__':unittest.main()
