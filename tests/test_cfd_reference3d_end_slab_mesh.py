"""End-slab cuts must preserve accepted inner geometry and physical boundaries."""
import sys,unittest
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_domain_mesh import domain_mesh

def tetrahedra_by_geometry(mesh,mask):
    vertices=mesh.p[:,mesh.t[:,mask]].transpose(2,1,0)
    # Canonical coordinates compare geometry, independent of global numbering.
    keys=[]
    for cell in vertices:keys.append(tuple(sorted(tuple(np.round(v,13)) for v in cell)))
    return sorted(keys)

class EndSlab(unittest.TestCase):
    def test_actual_normal_inner_tetrahedra_and_body_surface_preserved(self):
        a,al,ah,ax,am=domain_mesh(8.,6,True,mode='held_l4')
        b,bl,bh,bx,bm=domain_mesh(8.,6,True,outer_layers=2,mode='held_l4')
        self.assertEqual(a.nelements,23616);self.assertEqual(b.nelements,28416)
        np.testing.assert_array_equal(al,bl);np.testing.assert_array_equal(ah,bh)
        np.testing.assert_array_equal(ax[1:],bx[1:]);self.assertTrue(set(ax[0]).issubset(set(bx[0])))
        left,right=ax[0][1],ax[0][-2]
        def inner(mesh):
            x=mesh.p[0,mesh.t];return (x.min(axis=0)>=left-1e-12)&(x.max(axis=0)<=right+1e-12)
        self.assertEqual(int(inner(a).sum()),18816);self.assertEqual(int(inner(b).sum()),18816)
        self.assertEqual(tetrahedra_by_geometry(a,inner(a)),tetrahedra_by_geometry(b,inner(b)))
        def surface(mesh):return sorted(tuple(sorted(tuple(np.round(v,13)) for v in mesh.p[:,f].T)) for f in mesh.facets[:,mesh.boundaries['body']].T)
        self.assertEqual(surface(a),surface(b))

    def test_positive_jacobian_volume_shape_and_end_boundary_with_original_domain(self):
        a,lo,hi,ax,_=domain_mesh(8.,6,True,mode='held_l4')
        b,_,_,bx,_=domain_mesh(8.,6,True,outer_layers=2,mode='held_l4')
        self.assertGreater(np.abs(b.mapping().detA).min(),0.)
        self.assertAlmostEqual(np.abs(b.mapping().detA).sum()/6,31.,places=10)
        ca=np.linalg.cond(a.mapping().A.transpose(2,0,1));cb=np.linalg.cond(b.mapping().A.transpose(2,0,1))
        self.assertLess(cb.max(),.6*ca.max())
        for name,x in (('inlet',0.),('outlet',8.)):
            self.assertTrue(np.all(np.isclose(b.p[0,b.facets[:,b.boundaries[name]]],x)))
        centers=b.p[:,b.t].mean(axis=1)
        self.assertFalse(np.any(np.all((centers>lo[:,None])&(centers<hi[:,None]),axis=0)))

if __name__=='__main__':unittest.main()
