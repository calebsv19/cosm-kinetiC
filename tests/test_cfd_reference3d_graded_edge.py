"""Check archived edge geometries and literal observer transformations."""
import unittest,sys,json,hashlib
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_graded_edge_survey import build,CONTROLS
from cfd_reference3d_accuracy_graded_mesh import translated_inner_keys
class GradedEdge(unittest.TestCase):
    def test_actual_archived_cell_geometry_reproduces_declared_changes(self):
        d=R/'build/c3d-graded-stress';survey=json.loads((d/'edge-geometry-survey.json').read_text())
        with np.load(d/'edge-geometry.npz',allow_pickle=False) as saved:
            inner={}
            for L in (4.,8.):
                for kind,(strip,normal) in CONTROLS.items():
                    bundle,parent=build(L,kind);m,lo,hi,axes,n=bundle;prefix=f'L{int(L)}_{kind}_'
                    np.testing.assert_array_equal(m.p,saved[prefix+'vertices_m']);np.testing.assert_array_equal(m.t,saved[prefix+'tetrahedra'])
                    self.assertEqual(m.nelements,parent[0].nelements);self.assertEqual(n,int(saved[prefix+'macro_tetrahedra']))
                    self.assertAlmostEqual(np.abs(m.mapping().detA).sum()/6,4*L-1,places=10)
                    for a in range(3):
                        np.testing.assert_array_equal(axes[a],saved[prefix+f'axis{a}'])
                        if strip is not None:self.assertAlmostEqual(axes[a][axes[a]>lo[a]+1e-12][0]-lo[a],strip)
                        if a in (1,2):self.assertAlmostEqual(lo[a]-axes[a][axes[a]<lo[a]-1e-12][-1],normal)
                    row=next(r for r in survey['records'] if r['length']==L and r['kind']==kind)
                    self.assertTrue(row['physical_geometry_passed']);self.assertEqual(row['prospective_quality_passed'],kind in ('edge050','edge040'))
                    inner[L,kind]=translated_inner_keys(bundle)
            for kind in CONTROLS:self.assertEqual(inner[4.,kind],inner[8.,kind])
    def test_observer_changes_only_declared_input_guards_and_routing(self):
        for t in json.loads((R/'build/c3d-graded-stress/transforms.json').read_text()):
            parent=(R/t['parent']).read_bytes();output=(R/t['output']).read_bytes()
            self.assertEqual(hashlib.sha256(parent).hexdigest(),t['parent_sha256']);self.assertEqual(hashlib.sha256(output).hexdigest(),t['output_sha256'])
            v=parent.decode()
            for a,b in t['literal_replacements']:self.assertIn(a,v);v=v.replace(a,b)
            self.assertEqual(v,output.decode())
if __name__=='__main__':unittest.main()
