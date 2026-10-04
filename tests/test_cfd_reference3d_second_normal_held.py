"""Matched L8 geometry retains the translated L4 cube and complete inner cells."""
import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_second_normal_mesh import facet_keys

class HeldNormal(unittest.TestCase):
    def test_actual_translated_inner_cells_and_cube_triangles(self):
        (a,lo,hi,axes,_),ma=second_normal_tensor_mesh(4.)
        (b,blo,bhi,baxes,_),mb=second_normal_tensor_mesh(8.)
        shifted=a.p.copy();shifted[0]+=2
        def keys(mesh,points,left,right):
            v=points[:,mesh.t];keep=np.all((v[0]>=left-1e-11)&(v[0]<=right+1e-11),axis=0)
            return {tuple(sorted(map(tuple,np.round(points[:,t].T,11)))) for t in mesh.t[:,keep].T}
        ka=keys(a,shifted,3.3125,4.6875);kb=keys(b,b.p,3.3125,4.6875)
        self.assertEqual(ka,kb);self.assertEqual(len(ka),23616)
        def body(mesh,p):return {tuple(sorted(map(tuple,np.round(p[:,mesh.facets[:,f]].T,11)))) for f in mesh.boundaries['body']}
        self.assertEqual(body(a,shifted),body(b,b.p))
        np.testing.assert_allclose(blo,lo+np.array([2.,0.,0.]),rtol=0,atol=1e-14)
        np.testing.assert_allclose(bhi,hi+np.array([2.,0.,0.]),rtol=0,atol=1e-14)
        self.assertEqual(b.nelements,33216);self.assertEqual(mb['normal_distance_m'],ma['normal_distance_m'])

if __name__=='__main__':unittest.main()
