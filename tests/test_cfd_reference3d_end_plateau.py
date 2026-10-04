"""Three end layers preserve the physical inner/body control and improve end shape."""
import sys,unittest
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_domain_mesh import domain_mesh

def geometry(mesh,indices):
    return sorted(tuple(sorted(tuple(np.round(v,13)) for v in cell))
                  for cell in mesh.p[:,mesh.t[:,indices]].transpose(2,1,0))

def body_geometry(mesh):
    return sorted(tuple(sorted(tuple(np.round(v,13)) for v in mesh.p[:,f].T))
                  for f in mesh.facets[:,mesh.boundaries['body']].T)

class EndPlateau(unittest.TestCase):
    def test_held_inner_body_and_boundaries(self):
        old,lo,hi,axes,_=domain_mesh(8.,6,True,outer_layers=2,mode='held_l4')
        new,nlo,nhi,naxes,_=domain_mesh(8.,6,True,outer_layers=3,mode='held_l4')
        base,_,_,base_axes,_=domain_mesh(8.,6,True,mode='held_l4')
        self.assertEqual(old.nelements,28416);self.assertEqual(new.nelements,33216)
        np.testing.assert_array_equal(lo,nlo);np.testing.assert_array_equal(hi,nhi)
        np.testing.assert_array_equal(axes[1:],naxes[1:])
        left,right=base_axes[0][1],base_axes[0][-2]
        def inner(mesh):
            x=mesh.p[0,mesh.t];return np.flatnonzero((x.min(axis=0)>=left-1e-12)&(x.max(axis=0)<=right+1e-12))
        self.assertEqual(len(inner(new)),18816)
        self.assertEqual(geometry(base,inner(base)),geometry(new,inner(new)))
        self.assertEqual(geometry(old,inner(old)),geometry(new,inner(new)))
        self.assertEqual(body_geometry(old),body_geometry(new))
        for label,x in [('inlet',0.),('outlet',8.)]:
            self.assertTrue(np.all(np.isclose(new.p[0,new.facets[:,new.boundaries[label]]],x)))
        walls=new.p[:,new.facets[:,new.boundaries['walls']]].mean(axis=1)
        self.assertTrue(np.all(np.isclose(walls[1],0)|np.isclose(walls[1],2)|np.isclose(walls[2],0)|np.isclose(walls[2],2)))

    def test_volume_positive_mapping_and_end_shape(self):
        old,_,_,_,_=domain_mesh(8.,6,True,outer_layers=2,mode='held_l4')
        new,lo,hi,_,_=domain_mesh(8.,6,True,outer_layers=3,mode='held_l4')
        self.assertGreater(np.abs(new.mapping().detA).min(),0)
        self.assertAlmostEqual(np.abs(new.mapping().detA).sum()/6,31.,places=10)
        self.assertLess(np.linalg.cond(new.mapping().A.transpose(2,0,1)).max(),.70*np.linalg.cond(old.mapping().A.transpose(2,0,1)).max())
        centers=new.p[:,new.t].mean(axis=1)
        self.assertFalse(np.any(np.all((centers>lo[:,None])&(centers<hi[:,None]),axis=0)))

if __name__=='__main__':unittest.main()
