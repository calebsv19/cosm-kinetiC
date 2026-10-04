import unittest,sys
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_translated_cells import compare_cells,inner_cells
class TranslatedCells(unittest.TestCase):
    def test_translated_vertex_roundoff_and_vertex_order_do_not_change_cells(self):
        left=np.array([[[.0334936490545,0,0],[.1,0,0],[0,.1,0],[0,0,.1]]]);right=((left+np.array([3.5,0,0]))-np.array([3.5,0,0]))[:,[3,1,0,2]]
        result=compare_cells(left,right);self.assertTrue(result['cells_match']);self.assertLess(result['maximum_coordinate_difference_m'],1e-12)
    def test_key_collision_or_hidden_coordinate_change_cannot_certify(self):
        left=np.array([[[0.,0,0],[.1,0,0],[0,.1,0],[0,0,.1]]]);right=left.copy();right[0,0,0]=4e-11
        self.assertFalse(compare_cells(left,right)['cells_match'])
        with self.assertRaises(ValueError):compare_cells(np.concatenate([left,left]),np.concatenate([left,left]))
        right=left.copy();right[0,3]=[.1,.1,.1]
        self.assertFalse(compare_cells(left,right)['cells_match'])
    def test_all_archived_local_cells_match_below_one_picometer(self):
        for root in ('c3d-graded-local','c3d-graded-local-relative'):
            with np.load(R/'build'/root/'geometry.npz',allow_pickle=False) as saved:
                for kind in ('r025','r040'):
                    result=compare_cells(inner_cells(saved,f'L4_{kind}_'),inner_cells(saved,f'L8_{kind}_'))
                    self.assertTrue(result['cells_match']);self.assertLess(result['maximum_coordinate_difference_m'],1e-15)
if __name__=='__main__':unittest.main()
