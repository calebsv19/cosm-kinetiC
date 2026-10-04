import sys,unittest
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_accuracy_side_layer_survey import build
from cfd_reference3d_second_normal_mesh import facet_keys
class SideLayer(unittest.TestCase):
 def test_both_controls_preserve_actual_body_and_change_only_declared_planes(self):
  for length in (4.,8.):
   for mode in ('redistribute','bisect'):
    (m,lo,hi,axes,n),(old,_,_,original,oldn)=build(length,mode)
    self.assertEqual(m.nelements,4*n);self.assertEqual(facet_keys(m,m.boundaries['body']),facet_keys(old,old.boundaries['body']));np.testing.assert_array_equal(axes[0],original[0]);self.assertAlmostEqual(np.abs(m.mapping().detA).sum()/6,4*length-1,places=10)
    self.assertEqual(m.nelements,old.nelements if mode=='redistribute' else (62976 if length==4. else 72384))
    for i in (1,2):
     self.assertAlmostEqual(lo[i]-axes[i][axes[i]<lo[i]-1e-12][-1],.03125)
     self.assertAlmostEqual(axes[i][axes[i]>hi[i]+1e-12][0]-hi[i],.03125)
     if mode=='bisect':self.assertTrue(set(original[i]).issubset(set(axes[i])))
     else:np.testing.assert_array_equal(axes[i][~np.isclose(axes[i],lo[i]-.03125)&~np.isclose(axes[i],hi[i]+.03125)],original[i][~np.isclose(original[i],lo[i]-.0625)&~np.isclose(original[i],hi[i]+.0625)])
 def test_undeclared_controls_reject(self):
  for length,mode in ((5.,'bisect'),(4.,'unknown')):
   with self.assertRaises(ValueError):build(length,mode)
if __name__=='__main__':unittest.main()
