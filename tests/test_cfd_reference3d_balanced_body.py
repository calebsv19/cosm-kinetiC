import sys,unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_balanced_body_mesh import body_nodes,build,symmetry
from cfd_reference3d_corner_local_mesh import shape_inverse
class BalancedBody(unittest.TestCase):
    def test_intrinsic_shape_invariant_and_regular_normalization(self):
        p=np.array([[0,1,.5,.5],[0,0,np.sqrt(3)/2,np.sqrt(3)/6],[0,0,0,np.sqrt(2/3)]])
        m=MeshTet(p,np.arange(4)[:,None],sort_t=False);np.testing.assert_allclose(shape_inverse(m),1,atol=1e-14)
        q,_=np.linalg.qr(np.random.default_rng(98).normal(size=(3,3)))
        changed=MeshTet(3*q@p+np.array([[5.],[-1.],[.7]]),np.array([2,0,3,1])[:,None],sort_t=False)
        np.testing.assert_allclose(shape_inverse(changed),shape_inverse(m),atol=1e-14)
    def test_both_profiles_have_real_midplane_and_refined_maximum_spacing(self):
        for profile in ('cos8','cos7mid'):
            a=body_nodes(profile);self.assertEqual(len(a),9);self.assertEqual(a[4],.5)
            np.testing.assert_allclose(a+a[::-1],1,atol=1e-14);self.assertLess(np.diff(a).max(),.25)
    def test_independent_actual_volume_planes_areas_and_reflections(self):
        m,lo,hi,axes,n=build('cos7mid','relocate',.046875)
        p=m.p[:,m.t];det=np.linalg.det((p[:,1:]-p[:,0,None]).transpose(2,0,1));self.assertGreater(abs(det).min(),0);self.assertAlmostEqual(abs(det).sum()/6,15,places=10)
        self.assertEqual(m.nelements,24*(16*12**2-8**3));self.assertEqual(m.nelements,4*n);self.assertTrue(symmetry(m))
        for name,expected in dict(body=6,inlet=4,outlet=4,walls=32).items():
            p=m.p[:,m.facets[:,m.boundaries[name]]];area=np.sqrt(np.sum(np.cross((p[:,1]-p[:,0]).T,(p[:,2]-p[:,0]).T)**2,axis=1)).sum()/2
            self.assertAlmostEqual(area,expected,places=10)
        self.assertEqual(len(m.boundaries['body']),768)
        centers=m.p[:,m.t].mean(axis=1);self.assertFalse(np.any(np.all((centers>lo[:,None])&(centers<hi[:,None]),axis=0)))
        boundary=m.p[:,m.facets[:,m.boundary_facets()]].mean(axis=1)
        for v in boundary.T:self.assertTrue(any(abs(v[i]-end)<1e-10 for i,end in ((0,0),(0,4),(1,0),(1,2),(2,0),(2,2))) or (np.all(v>=lo-1e-10) and np.all(v<=hi+1e-10) and np.any(np.isclose(v,lo)|np.isclose(v,hi))))
    def test_invalid_parameters_refused_before_mesh(self):
        for args in [('uniform','retain',.0625),('cos8','bad',.0625),('cos8','retain',.1)]:
            with self.assertRaises(ValueError):build(*args)
if __name__=='__main__':unittest.main()
