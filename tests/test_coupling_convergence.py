import math
from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.convergence import restrict,comparison,matched_comparisons
class ConvergenceTests(unittest.TestCase):
    def test_extensive_restriction_conserves_and_maps(self):
        values=[float(i) for i in range(64)]
        actual=restrict(values,[4,4,4],[2,2,2])
        self.assertEqual(sum(actual),sum(values));self.assertEqual(actual[0],sum(values[i] for i in (0,1,4,5,16,17,20,21)))
    def test_identical_physical_density_has_zero_error(self):
        coarse={'energy_j':[2.]*8,'smoke_kg':[1.]*8}
        fine={'energy_j':[.25]*64,'smoke_kg':[.125]*64}
        result=comparison(coarse,fine,[2]*3,[4]*3)
        for row in result.values():self.assertEqual(row['relative_l1'],0);self.assertEqual(row['relative_l2'],0)
    def test_common_partition_does_not_hide_fine_diagnostic(self):
        grids=[[2]*3,[4]*3,[8]*3]
        fields=[]
        for grid in grids:
            g=grid[0];values=[]
            for k in range(g):
                for j in range(g):
                    for i in range(g):values.append((8/g)**3*(1+(.2 if i%4<2 else -.2) if g==8 else 1))
            fields.append({'energy_j':values,'smoke_kg':values})
        common,native=matched_comparisons(fields,grids)
        self.assertAlmostEqual(common[1]['energy_j']['relative_l1'],0)
        self.assertGreater(native[1]['energy_j']['relative_l1'],.1)
    def test_non_nested_and_zero_field_refuse(self):
        with self.assertRaises(ValueError):restrict([1.]*27,[3]*3,[2]*3)
        with self.assertRaises(ValueError):comparison({'energy_j':[0.]*8},{'energy_j':[0.]*64},[2]*3,[4]*3)
if __name__=='__main__':unittest.main()
