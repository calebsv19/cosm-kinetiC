import json,sys,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from test_cfd_reference3d_bounded_condensed import fixture
import cfd_reference3d_accuracy_graded_budget as b
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem as Original
from cfd_reference3d_accuracy_graded_triangle import TriangleCondensedSystem as Graded
from cfd_reference3d_accuracy_graded_condensed import full_action
from cfd_reference3d_condensed import full_action as old_action
from cfd_reference3d_accuracy_graded_storage import decision
from cfd_reference3d_size_selected import decision as old_decision
from cfd_reference3d_accuracy_graded_mesh import build,translated_inner_keys
import os
ARCHIVE=Path(os.environ["PHYSICS_SIM_GRADED_ARCHIVE"])
class GradedArchive(unittest.TestCase):
 def test_saved_matched_meshes_and_first_layer(self):
  meshes={}
  with np.load(ARCHIVE/'paired-graded-geometry.npz',allow_pickle=False) as archive:
   for L in (4.,8.):
    bundle,_=build(L);meshes[L]=bundle;m,lo,hi,axes,n=bundle;prefix=f'L{int(L)}_';np.testing.assert_array_equal(m.p,archive[prefix+'vertices_m']);np.testing.assert_array_equal(m.t,archive[prefix+'tetrahedra']);self.assertLessEqual(m.nelements,120000)
    for i in (1,2):self.assertAlmostEqual(lo[i]-axes[i][axes[i]<lo[i]-1e-12][-1],.03125)
  self.assertEqual(translated_inner_keys(meshes[4.]),translated_inner_keys(meshes[8.]))
 def test_exact_literal_resource_adapters(self):
  for t in json.loads((ARCHIVE/'transforms.json').read_text()):
   v=(R/t['parent']).read_text()
   for a,c in t['literal_replacements']:self.assertIn(a,v);v=v.replace(a,c)
   self.assertEqual(v,(R/t['output']).read_text())
if __name__=='__main__':unittest.main()
