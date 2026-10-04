"""Independent domain/control geometry and condensed original-action proofs."""
import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_graded_mesh import graded_mesh
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic


class Domain(unittest.TestCase):
    def test_default_and_L4_controls_preserve_original_mesh(self):
        for length in (4.,8.):
            for split in (False,True):
                a,*_=domain_mesh(length,4,split);b,*_=graded_mesh(length,4,split,3)
                np.testing.assert_array_equal(a.p,b.p);np.testing.assert_array_equal(a.t,b.t)
        a,*_=domain_mesh(4.,4,True,mode='held_l4');b,*_=graded_mesh(4.,4,True,3)
        np.testing.assert_array_equal(a.p,b.p);np.testing.assert_array_equal(a.t,b.t)
    def test_held_near_planes_symmetry_volume_facets_and_macro_centers(self):
        for count in (2,4):
            for split in (False,True):
                base,_,_,base_axes,_=domain_mesh(4.,count,split)
                mesh,lo,hi,axes,macros=domain_mesh(8.,count,split,mode='held_l4')
                np.testing.assert_array_equal(mesh.t,base.t);self.assertEqual(mesh.nelements,4*macros)
                np.testing.assert_allclose(axes[0][1:-1]-4.,base_axes[0][1:-1]-2.,rtol=0,atol=1e-14)
                np.testing.assert_array_equal(axes[1],base_axes[1]);np.testing.assert_array_equal(axes[2],base_axes[2])
                self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,31.,places=10)
                vertices=set(map(tuple,np.round(mesh.p.T,10)))
                for a,L in enumerate((8.,2.,2.)):
                    reflected=mesh.p.copy();reflected[a]=L-reflected[a]
                    self.assertEqual(vertices,set(map(tuple,np.round(reflected.T,10))))
                for name,faces in mesh.boundaries.items():
                    points=mesh.p[:,mesh.facets[:,faces]]
                    area=np.linalg.norm(np.cross((points[:,1]-points[:,0]).T,(points[:,2]-points[:,0]).T),axis=1).sum()/2
                    self.assertAlmostEqual(area,{'body':6.,'inlet':4.,'outlet':4.,'walls':64.}[name],places=10)
                for center in np.unique(mesh.t.max(axis=0)):
                    cells=mesh.t[:,np.any(mesh.t==center,axis=0)];corners=np.unique(cells);corners=corners[corners!=center]
                    self.assertEqual(len(corners),4)
                    np.testing.assert_allclose(mesh.p[:,center],mesh.p[:,corners].mean(axis=1),atol=1e-12)
                c=mesh.p[:,mesh.t].mean(axis=1)
                self.assertFalse(np.any(np.all((c>lo[:,None])&(c<hi[:,None]),axis=0)))
    def test_outer_subdivision_preserves_held_planes_and_true_boundaries(self):
        old,lo,hi,axes,_=domain_mesh(8.,4,True,mode='held_l4')
        mesh,_,_,changed,macros=domain_mesh(8.,4,True,outer_layers=2,mode='held_l4')
        self.assertEqual(mesh.nelements,16896);self.assertEqual(mesh.nelements,4*macros)
        self.assertTrue(set(axes[0]).issubset(set(changed[0])))
        np.testing.assert_array_equal(changed[0][(changed[0]>=axes[0][1])&(changed[0]<=axes[0][-2])],axes[0][1:-1])
        self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,31.,places=10)
        center=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
        exterior=np.any(np.isclose(center,0)|np.isclose(center,np.array([8.,2.,2.])[:,None]),axis=0)
        body=np.all((center>=lo[:,None]-1e-10)&(center<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(center,lo[:,None])|np.isclose(center,hi[:,None]),axis=0)
        self.assertTrue(np.all(exterior|body))
        self.assertLess(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max(),np.linalg.cond(old.mapping().A.transpose(2,0,1)).max())

    def test_original_quadrature_action_matches_condensed_reconstruction(self):
        # Six anisotropic macro tetrahedra from one true outer tensor cell.
        from skfem import MeshTet
        from cfd_reference3d_p3 import alfeld_split
        mesh=alfeld_split(MeshTet.init_tensor([0.,3.3125],[0.,.0625],[0.,.0625]))
        system=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        rng=np.random.default_rng(71);z=rng.normal(size=system.shape[0]);rhs=np.zeros(3*ub.N+pb.N)
        u,p=system.reconstruct(z,rhs);actual=full_action(mesh,ub,pb,u,p,.1,128)
        momentum=np.concatenate([A@u[a]+B[a].T@p for a in range(3)])
        pressure=sum(B[a]@u[a] for a in range(3))
        np.testing.assert_allclose(actual,np.r_[momentum,pressure],atol=1e-9,rtol=1e-10)
        free=system.trace
        np.testing.assert_allclose(actual[:3*ub.N].reshape(3,-1)[:,free].ravel(),(system.matrix@z)[:3*system.nt],atol=1e-9,rtol=1e-10)


if __name__=='__main__':unittest.main()
