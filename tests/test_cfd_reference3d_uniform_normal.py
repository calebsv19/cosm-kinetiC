"""Actual tensor-family geometry must qualify before resource/PDE experiments."""
import sys,unittest
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_uniform_normal_mesh import uniform_normal_mesh
from cfd_reference3d_corner_local_mesh import shape_inverse
from cfd_reference3d_p4 import require_sorted

class UniformNormal(unittest.TestCase):
    def test_actual_families_improve_shape_and_preserve_true_physical_surfaces(self):
        for count,expected in ((6,23616),(8,36096)):
            (mesh,lo,hi,axes,macros),meta=uniform_normal_mesh(4.,count)
            self.assertEqual(mesh.nelements,expected);self.assertEqual(expected,4*macros)
            self.assertLess(meta['refined_near_worst_shape'],meta['original_near_worst_shape'])
            self.assertLess(meta['refined_near_mean_shape'],meta['original_near_mean_shape'])
            self.assertLess(meta['refined_global_worst_shape'],meta['original_global_worst_shape'])
            self.assertLess(meta['refined_global_max_condition'],meta['original_global_max_condition'])
            vertices=mesh.p[:,mesh.t];det=np.linalg.det((vertices[:,1:]-vertices[:,:1]).transpose(2,0,1))
            self.assertGreater(np.abs(det).min(),0);self.assertAlmostEqual(np.abs(det).sum()/6,15.,places=10)
            for name,indices in mesh.boundaries.items():
                points=mesh.p[:,mesh.facets[:,indices]]
                if name=='body':
                    self.assertTrue(np.all((points>=lo[:,None,None]-1e-12)&(points<=hi[:,None,None]+1e-12)))
                    self.assertTrue(np.all(np.any(np.all(np.isclose(points,lo[:,None,None])|np.isclose(points,hi[:,None,None]),axis=1),axis=0)))
                elif name in ('inlet','outlet'):self.assertTrue(np.all(np.isclose(points[0],0 if name=='inlet' else 4)))
                else:
                    self.assertTrue(np.all(np.any(np.all(np.isclose(points[1:],0)|np.isclose(points[1:],2),axis=1),axis=0)))
                area=np.linalg.norm(np.cross((points[:,1]-points[:,0]).T,(points[:,2]-points[:,0]).T),axis=1).sum()/2
                self.assertAlmostEqual(area,{'body':6.,'inlet':4.,'outlet':4.,'walls':32.}[name],places=10)
            self.assertEqual(set(np.concatenate(list(mesh.boundaries.values()))),set(mesh.boundary_facets()))

    def test_all_original_barycenters_orientations_uniform_body_and_normal_planes(self):
        for count in (6,8):
            (mesh,lo,hi,axes,macros),meta=uniform_normal_mesh(4.,count);require_sorted(mesh)
            for a,nodes in enumerate(axes):
                inside=nodes[(nodes>=lo[a])&(nodes<=hi[a])]
                np.testing.assert_allclose(np.diff(inside),1/count,rtol=0,atol=1e-14)
                self.assertAlmostEqual(lo[a]-nodes[nodes<lo[a]][-1],1/16,places=12)
                self.assertAlmostEqual(nodes[nodes>hi[a]][0]-hi[a],1/16,places=12)
            for parent in range(macros):
                vertices=np.unique(mesh.t[:,parent+np.arange(4)*macros]);self.assertEqual(len(vertices),5)
                np.testing.assert_allclose(mesh.p[:,vertices[-1]],mesh.p[:,vertices[:-1]].mean(axis=1),rtol=0,atol=1e-12)
            self.assertFalse(meta['body_planes_preserved'])
            self.assertGreater(meta['uniform_body_interval_m'],meta['original_first_cosine_interval_m'])
        with self.assertRaises(ValueError):uniform_normal_mesh(8.,6)
        with self.assertRaises(ValueError):uniform_normal_mesh(4.,4)
        with self.assertRaises(ValueError):uniform_normal_mesh(4.,6,1.5)

if __name__=='__main__':unittest.main()
