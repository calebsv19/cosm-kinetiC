"""Invariant shape, conforming subdivision and original-action proof controls."""
import sys
import itertools
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_corner_local_mesh import shape_inverse,edge_star_refined,local_corner_mesh,LocalGeometryRejected
from cfd_reference3d_corner_mesh import corner_mesh,CornerGeometryRejected
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_corner_attribution import trace_lift


class Geometry(unittest.TestCase):
    def test_shape_regular_scale_and_all_vertex_permutations(self):
        points=np.array([[0.,1.,.5,.5],[0.,0.,np.sqrt(3)/2,np.sqrt(3)/6],[0.,0.,0.,np.sqrt(2/3)]])
        m=MeshTet(points,np.arange(4)[:,None]);self.assertAlmostEqual(shape_inverse(m)[0],1.,places=12)
        anisotropic=np.diag([3.,.7,1.2])@points
        expected=shape_inverse(MeshTet(anisotropic,np.arange(4)[:,None]))
        for permutation in itertools.permutations(range(4)):
            for scale in (1e-5,1.,1e5):
                actual=shape_inverse(MeshTet(anisotropic*scale,np.array(permutation)[:,None]))
                np.testing.assert_allclose(actual,expected,rtol=1e-12,atol=1e-12)
    def test_edge_stars_partition_volume_and_have_no_false_walls(self):
        old=MeshTet.init_tensor([0.,1.,2.],[0.,1.],[0.,1.])
        for selected in ((0,),(0,1),(0,6)):
            new=edge_star_refined(old,selected)
            self.assertGreater(new.nelements,old.nelements);self.assertGreater(np.abs(new.mapping().detA).min(),0)
            self.assertAlmostEqual(np.abs(new.mapping().detA).sum()/6,2.,places=12)
            mid=new.p[:,new.facets[:,new.boundary_facets()]].mean(axis=1)
            self.assertTrue(np.all(np.any(np.isclose(mid,0)|np.isclose(mid,np.array([2.,1.,1.])[:,None]),axis=0)))
            self.assertEqual(len(np.unique(new.p.T,axis=0)),new.nvertices)
    def test_geometry_stops_unsuitable_candidates_before_algebra(self):
        for length in (4.,8.):
            with self.assertRaises(CornerGeometryRejected) as stopped:corner_mesh(length,.1)
            self.assertGreater(stopped.exception.metadata['controlled_corner_max_condition'],stopped.exception.metadata['original_corner_max_condition'])
        with self.assertRaises(LocalGeometryRejected) as stopped:local_corner_mesh(8.,(33,249))
        self.assertEqual(stopped.exception.metadata['refined_tetrahedra'],14016)
        self.assertGreater(stopped.exception.metadata['refined_affected_worst_shape'],stopped.exception.metadata['original_affected_worst_shape'])
        with self.assertRaises(ValueError):local_corner_mesh(8.,(-1,))
    def test_explicit_original_action_after_multi_edge_subdivision(self):
        old=MeshTet.init_tensor([0.,1.3],[0.,.8],[0.,.5])
        mesh=alfeld_split(edge_star_refined(old,(0,)));system=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        z=np.random.default_rng(986).normal(size=system.shape[0]);rhs=np.zeros(3*ub.N+pb.N)
        u,p=system.reconstruct(z,rhs);actual=full_action(mesh,ub,pb,u,p,.1,128)
        explicit=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(actual,explicit,atol=1e-9,rtol=1e-10)
    def test_signed_tracing_keeps_component_cancellation(self):
        parts={};reaction=np.array([8.,0.,0.])
        for name,raw,weak,buckets in (('pressure',5.,6.,[-2.,1.,0.,0.,0.,0.]),('viscous',2.,2.,[.5,-.5,0.,0.,0.,0.])):
            parts[name]=dict(raw_surface_load_n=[raw,0.,0.],weak_load_n=[weak,0.,0.],raw_minus_weak_n=[raw-weak,0.,0.],
                interior_jump_centroid_buckets_n=np.array([buckets,np.zeros(6),np.zeros(6)]).T.tolist(),volume_divergence_centroid_buckets_n=np.zeros((6,3)).tolist())
        row=trace_lift(dict(shell_m=.25,**parts),reaction)
        self.assertEqual(row['net_raw_minus_reaction_n'],[-1.,0.,0.]);self.assertEqual(row['nearest_centroid_bucket_to_net_x_ratio'],1.5)
        self.assertEqual(row['total_signed_centroid_buckets_n'][0],[-1.5,0.,0.])


if __name__=='__main__':unittest.main()
