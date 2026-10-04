"""Same-surface remesh proves geometry/full equations without claiming old tet nesting."""
import sys,unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_second_normal_mesh import facet_keys
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_stokes_pair import mixed_matrix

class TensorNormal(unittest.TestCase):
    def test_brick_partition_but_explicitly_non_nested_tetrahedra(self):
        old=MeshTet.init_tensor([0.,1.],[0.,1.],[0.,1.]);new=MeshTet.init_tensor([0.,.5,1.],[0.,1.],[0.,1.])
        for side in (0,1):
            centers=new.p[:,new.t].mean(axis=1);mask=(centers[0]<.5) if side==0 else (centers[0]>.5)
            self.assertAlmostEqual(np.abs(new.mapping().detA[mask]).sum()/6,.5,places=14)
        crossing=0
        for t in new.t.T:
            contained=False
            for ot in old.t.T:
                v=old.p[:,ot];b=np.linalg.solve(v[:,1:]-v[:,0,None],new.p[:,t]-v[:,0,None]);b=np.vstack((1-b.sum(axis=0),b))
                contained|=b.min()>=-1e-12 and b.max()<=1+1e-12
            crossing+=not contained
        self.assertGreater(crossing,0)

    def test_actual_cube_surface_axes_volume_and_global_quality(self):
        (mesh,lo,hi,axes,n),meta=second_normal_tensor_mesh()
        old,olo,ohi,oldaxes,_=domain_mesh(4.,6,True,3,1,'held_l4')
        self.assertEqual(mesh.nelements,28416);self.assertFalse(meta['original_tetrahedron_partition_claimed'])
        self.assertEqual(facet_keys(mesh,mesh.boundaries['body']),facet_keys(old,old.boundaries['body']))
        for a,b in zip(oldaxes,axes):self.assertTrue(set(a).issubset(set(b)))
        v=mesh.p[:,mesh.t];det=np.linalg.det((v[:,1:]-v[:,0,None]).transpose(2,0,1))
        self.assertGreater(np.abs(det).min(),0);self.assertAlmostEqual(np.abs(det).sum()/6,15.,places=10)
        np.testing.assert_array_equal(lo,olo);np.testing.assert_array_equal(hi,ohi)
        self.assertLessEqual(meta['refined_global_worst_shape'],meta['original_global_worst_shape']*(1+1e-8))
        self.assertLessEqual(meta['refined_global_max_condition'],meta['original_global_max_condition']*(1+1e-8))
        with self.assertRaises(ValueError):second_normal_tensor_mesh(6.)

    def test_complete_original_fe_action_and_nonzero_load_reconstruction(self):
        macro=MeshTet.init_tensor([0.,.046875,.09375],[0.,.067],[0.,.125])
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
        macro=MeshTet.init_tensor([0.,.046875,.09375],[0.,.067],[0.,.125])
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

if __name__=='__main__':unittest.main()
