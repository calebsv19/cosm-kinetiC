"""Conforming normal-slab cuts retain physical geometry and complete mixed equations."""
import sys,unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_second_normal_mesh import cut_macro_plane,second_normal_mesh,facet_keys
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_stokes_pair import mixed_matrix

class NormalCut(unittest.TestCase):
    def test_shared_faces_parent_partition_and_unchanged_end_faces(self):
        old=MeshTet.init_tensor([0.,.09375],[0.,.067],[0.,.125])
        mesh,parents,meta=cut_macro_plane(old,.046875)
        np.testing.assert_array_equal(mesh.p[:,:old.nvertices],old.p)
        self.assertLess(meta['maximum_parent_volume_error_m3'],1e-15)
        self.assertEqual(meta['cut_parent_count'],6)
        mids=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
        self.assertTrue(np.all(np.any(np.isclose(mids,0)|np.isclose(mids,np.array([.09375,.067,.125])[:,None]),axis=0)))
        for plane in (0.,.09375):
            a=old.boundary_facets();b=mesh.boundary_facets()
            a=a[np.all(np.isclose(old.p[0,old.facets[:,a]],plane),axis=0)]
            b=b[np.all(np.isclose(mesh.p[0,mesh.facets[:,b]],plane),axis=0)]
            self.assertEqual(facet_keys(old,a),facet_keys(mesh,b))
        volumes=np.abs(mesh.mapping().detA)/6
        np.testing.assert_allclose(np.bincount(parents,weights=volumes),np.abs(old.mapping().detA)/6,rtol=1e-12,atol=1e-16)

    def test_complete_original_fe_action_and_nonzero_load_reconstruction(self):
        macro,_,_=cut_macro_plane(MeshTet.init_tensor([0.,.09375],[0.,.067],[0.,.125]),.046875)
        mesh=alfeld_split(macro);system=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        ub,pb,A,B=assemble_quartic(mesh,.1,128);K=mixed_matrix(A,B,np.arange(ub.N))
        rng=np.random.default_rng(460875);rhs=rng.normal(size=K.shape[0]);z=rng.normal(size=system.shape[0])
        u,p=system.reconstruct(z,rhs);actual=full_action(mesh,ub,pb,u,p,.1,128)
        np.testing.assert_allclose(actual,K@np.r_[u.ravel(),p],rtol=1e-10,atol=2e-9)
        residual=actual-rhs
        retained=np.r_[residual[:3*ub.N].reshape(3,-1)[:,system.trace].ravel(),[residual[3*ub.N:][ids].sum() for _,_,ids,_ in system.records]]
        np.testing.assert_allclose(retained,system.matrix@z-system.reduce_rhs(rhs),rtol=1e-10,atol=2e-9)
        for _,indices,pressure,_ in system.records:
            self.assertLess(np.max(np.abs(residual[:3*ub.N].reshape(3,-1)[:,indices[system.ref.bubble]])),2e-9)
            self.assertLess(np.max(np.abs(system.ref.Q.T@residual[3*ub.N:][pressure])),2e-9)

    def test_independently_assembled_field_load_pressure_mean_and_reconstruction(self):
        macro,_,_=cut_macro_plane(MeshTet.init_tensor([0.,.09375],[0.,.067],[0.,.125]),.046875)
        mesh=alfeld_split(macro);system=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        ub,pb,A,B=assemble_quartic(mesh,.1,128);K=mixed_matrix(A,B,np.arange(ub.N))
        rng=np.random.default_rng(946875);original_u=.01*rng.normal(size=(3,ub.N));original_p=rng.normal(size=pb.N)
        rhs=K@np.r_[original_u.ravel(),original_p];z=system.retained_field(original_u,original_p)
        self.assertGreater(np.linalg.norm(rhs),0)
        u,p=system.reconstruct(z,rhs)
        np.testing.assert_allclose(u,original_u,rtol=1e-10,atol=1e-10)
        np.testing.assert_allclose(p,original_p,rtol=1e-10,atol=1e-10)
        np.testing.assert_allclose(system.retained_field(u,p),z,rtol=1e-10,atol=1e-10)
        np.testing.assert_allclose(full_action(mesh,ub,pb,u,p,.1,128),rhs,rtol=1e-10,atol=2e-9)

    def test_actual_cube_surface_axes_and_global_quality(self):
        (mesh,lo,hi,axes,n),meta=second_normal_mesh()
        old,olo,ohi,oldaxes,_=domain_mesh(4.,6,True,3,1,'held_l4')
        self.assertEqual(mesh.nelements,41216)
        self.assertEqual(facet_keys(mesh,mesh.boundaries['body']),facet_keys(old,old.boundaries['body']))
        for a,b in zip(oldaxes,axes):self.assertTrue(set(a).issubset(set(b)))
        np.testing.assert_array_equal(lo,olo);np.testing.assert_array_equal(hi,ohi)
        self.assertLessEqual(meta['refined_global_worst_shape'],meta['original_global_worst_shape']*(1+1e-8))
        self.assertLessEqual(meta['refined_global_max_condition'],meta['original_global_max_condition']*(1+1e-8))
        self.assertEqual(meta['normal_distance_m'],.046875)
        with self.assertRaises(ValueError):second_normal_mesh(6.)

if __name__=='__main__':unittest.main()
