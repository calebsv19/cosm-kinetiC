"""Physical domain pairing separates end-slab extension from body refinement."""
import sys,unittest
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_domain_mesh import domain_mesh

class DomainPair(unittest.TestCase):
    def test_actual_normal_connectivity_cross_section_body_and_inner_planes(self):
        a,al,ah,ax,am=domain_mesh(4.,6,True,mode='original')
        b,bl,bh,bx,bm=domain_mesh(8.,6,True,mode='held_l4')
        self.assertEqual(a.nelements,23616);self.assertEqual(b.nelements,a.nelements);self.assertEqual(am,bm)
        np.testing.assert_array_equal(a.t,b.t);np.testing.assert_array_equal(a.p[1:],b.p[1:])
        np.testing.assert_array_equal(bx[0][1:-1],ax[0][1:-1]+2.)
        np.testing.assert_array_equal(bl,al+np.array([2.,0.,0.]));np.testing.assert_array_equal(bh,ah+np.array([2.,0.,0.]))
        inner=(a.p[0]>=ax[0][1])&(a.p[0]<=ax[0][-2])
        np.testing.assert_allclose(b.p[0,inner],a.p[0,inner]+2.,rtol=0,atol=1e-14)
        for name in ('body','walls','inlet','outlet'):np.testing.assert_array_equal(a.boundaries[name],b.boundaries[name])

    def test_positive_jacobians_volume_and_body_faces_with_no_hole_filling(self):
        for length in (4.,8.):
            mesh,lo,hi,axes,macros=domain_mesh(length,4,True,mode='held_l4')
            determinants=np.abs(mesh.mapping().detA)
            self.assertGreater(determinants.min(),0.);self.assertAlmostEqual(determinants.sum()/6,4*length-1,places=10)
            centers=mesh.p[:,mesh.facets[:,mesh.boundaries['body']]].mean(axis=1)
            self.assertTrue(np.all((centers>=lo[:,None]-1e-12)&(centers<=hi[:,None]+1e-12)))
            self.assertTrue(np.all(np.any(np.isclose(centers,lo[:,None])|np.isclose(centers,hi[:,None]),axis=0)))
            tetra_centers=mesh.p[:,mesh.t].mean(axis=1)
            self.assertFalse(np.any(np.all((tetra_centers>lo[:,None])&(tetra_centers<hi[:,None]),axis=0)))
            for name,x in (('inlet',0.),('outlet',length)):
                self.assertTrue(np.all(np.isclose(mesh.p[0,mesh.facets[:,mesh.boundaries[name]]],x)))

    def test_stretching_entire_domain_is_distinct_from_held_control(self):
        original=domain_mesh(8.,2,False,mode='original');held=domain_mesh(8.,2,False,mode='held_l4')
        np.testing.assert_array_equal(original[0].t,held[0].t)
        self.assertFalse(np.array_equal(original[0].p,held[0].p))
        with self.assertRaises(ValueError):domain_mesh(8.,2,False,exponent=2,mode='held_l4')

if __name__=='__main__':unittest.main()
