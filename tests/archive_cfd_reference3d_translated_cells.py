"""Explicit saved-experiment checks; excluded from current source fixtures."""
import unittest
import test_cfd_reference3d_translated_cells as current
from cfd_reference_test_support import archive_root
globals().update({k:v for k,v in vars(current).items() if not k.startswith('__') and not (isinstance(v,type) and issubclass(v,unittest.TestCase))})

class SavedExperiment(unittest.TestCase):
    def test_all_archived_local_cells_match_below_one_picometer(self):
        for root in ('c3d-graded-local','c3d-graded-local-relative'):
            with np.load(archive_root()/root/'geometry.npz',allow_pickle=False) as saved:
                for kind in ('r025','r040'):
                    result=compare_cells(inner_cells(saved,f'L4_{kind}_'),inner_cells(saved,f'L8_{kind}_'))
                    self.assertTrue(result['cells_match']);self.assertLess(result['maximum_coordinate_difference_m'],1e-15)

if __name__=='__main__':unittest.main()
