"""Validate grading without changing domain, topology or Alfeld support."""
import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_spatial_mesh import spatial_mesh
from cfd_fem_reference3d_solenoidal import mesh_for_case


class SpatialMesh(unittest.TestCase):
    def test_legacy_identity(self):
        for split in (False,True):
            a,*_=spatial_mesh(count=4,split=split);b,*_=mesh_for_case(count=4,split=split)
            np.testing.assert_array_equal(a.p,b.p);np.testing.assert_array_equal(a.t,b.t)

    def test_parent_affinity_and_domain(self):
        for length in (4.,8.):
            for split in (False,True):
                m,lo,hi,axes,parents=spatial_mesh(length,count=4,split=split,normal_spacing=.03)
                old,*_=mesh_for_case(length,count=4,split=split)
                np.testing.assert_array_equal(m.t,old.t)
                self.assertAlmostEqual(np.abs(m.mapping().detA).sum()/6,4*length-1,places=10)
                # Alfeld center = mean of the four macro vertices. Global center
                # IDs follow macro vertices and are shared by exactly four children.
                center_ids=np.unique(m.t.max(axis=0));self.assertEqual(len(center_ids),parents)
                for center in center_ids:
                    cells=m.t[:,np.any(m.t==center,axis=0)]
                    vertices=np.unique(cells);vertices=vertices[vertices!=center]
                    self.assertEqual(len(vertices),4)
                    np.testing.assert_allclose(m.p[:,center],m.p[:,vertices].mean(axis=1),atol=2e-12)
                for name,facets in m.boundaries.items():
                    np.testing.assert_array_equal(facets,old.boundaries[name])
                nodes=axes[0];first=np.min(nodes[nodes>hi[0]]-hi[0])
                self.assertAlmostEqual(first,.015 if split else .03,places=12)

    def test_insert_preserves_outer_nodes_and_boundaries(self):
        old,lo,hi,old_axes,_=mesh_for_case(count=4)
        for split in (False,True):
            m,_,_,axes,_=spatial_mesh(count=4,split=split,normal_spacing=.03,insert_normal=True)
            self.assertTrue(set(old_axes[0]).issubset(set(axes[0])))
            np.testing.assert_array_equal(axes[1],old_axes[1]);np.testing.assert_array_equal(axes[2],old_axes[2])
            self.assertEqual(m.nelements,16896 if split else 13824)
            self.assertAlmostEqual(np.abs(m.mapping().detA).sum()/6,15.,places=10)
            mid=m.p[:,m.facets[:,m.boundary_facets()]].mean(axis=1)
            exterior=np.any(np.isclose(mid,0)|np.isclose(mid,np.array([4.,2.,2.])[:,None]),axis=0)
            body=np.all((mid>=lo[:,None]-1e-10)&(mid<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(mid,lo[:,None])|np.isclose(mid,hi[:,None]),axis=0)
            self.assertTrue(np.all(exterior|body))

    def test_conforming_edge_refinement(self):
        base,lo,hi,axes,_=spatial_mesh(count=6,normal_spacing=.03,insert_normal=True)
        mesh,_,_,new_axes,parents=spatial_mesh(count=6,normal_spacing=.03,insert_normal=True,edge_passes=1,edge_radius=.04)
        self.assertEqual(mesh.nelements,28224)
        self.assertGreater(mesh.nelements,base.nelements)
        self.assertEqual(mesh.nelements,4*parents)
        self.assertGreater(np.min(np.abs(mesh.mapping().detA)),0)
        self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,15.,places=10)
        for a,b in zip(axes,new_axes):np.testing.assert_array_equal(a,b)
        mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
        exterior=np.any(np.isclose(mid,0)|np.isclose(mid,np.array([4.,2.,2.])[:,None]),axis=0)
        body=np.all((mid>=lo[:,None]-1e-10)&(mid<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(mid,lo[:,None])|np.isclose(mid,hi[:,None]),axis=0)
        self.assertTrue(np.all(exterior|body))

    def test_bad_spacing(self):
        for value in (0,-1,float('nan'),float('inf'),.5):
            with self.assertRaises(ValueError):spatial_mesh(normal_spacing=value)


if __name__=='__main__':unittest.main()
