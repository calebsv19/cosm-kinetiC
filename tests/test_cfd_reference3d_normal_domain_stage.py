"""Normal-stage admission is diagnostic only and never allocates a numeric factor."""
import sys,json
import unittest
from unittest.mock import patch
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import cfd_reference3d_normal_domain_stage_probe as stage
from cfd_reference3d_allocator_pressure import release_free_pages
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=ROOT/'build/c3d-vector-storage/support/factor.dylib'


class Stage(unittest.TestCase):
    def fixture(self):
        mesh=system_fixture().mesh.with_boundaries({'body':lambda x:np.isclose(x[2],0),'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],1.3)})
        return mesh,np.array([.3,.2,.08]),np.array([.6,.4,.12]),[np.array([0.,1.3]),np.array([0.,.7]),np.array([0.,.08,.2])],mesh.nelements//4

    def test_fitting_stage_still_has_no_numeric_inverse_field_or_full_acceptance(self):
        with patch.object(stage,'domain_mesh',return_value=self.fixture()) as mesh_call:
            row=stage.run(length=8.,count=6,split=True,factor_library=LIB,domain_mesh_mode='held_l4',chunk_size=512)
        mesh_call.assert_called_once_with(8.,6,True,3,1,'held_l4')
        self.assertEqual(row['domain_mesh_mode'],'held_l4')
        self.assertEqual(row['axis_nodes_m'],[a.tolist() for a in self.fixture()[3]])
        self.assertTrue(row['diagnostic_accepted']);self.assertFalse(row['numerically_accepted'] or row['numerical_field_published'] or row['numeric_factor_attempted'])
        self.assertTrue(row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved'])
        budget=row['admission'];self.assertEqual(budget['reserve_bytes'],32*1024**2)
        self.assertEqual(budget['estimated_numeric_stage_bytes'],sum(budget[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes')))
        self.assertTrue(budget['numeric_stage_admitted']);json.loads(json.dumps(row))

    def test_over_budget_diagnostic_rejects_numeric_admission_and_cleans_symbolic(self):
        def inflated(*args,**kwargs):
            row=release_free_pages(*args,**kwargs);row['current_rss_after_bytes']=1800*1024**2;return row
        with patch.object(stage,'domain_mesh',return_value=self.fixture()),patch.object(stage.workspace_module,'release_free_pages',side_effect=inflated):
            row=stage.run(count=6,split=True,factor_library=LIB)
        self.assertTrue(row['diagnostic_accepted'] and row['symbolic_handle_cleanup_verified'])
        self.assertFalse(row['admission']['numeric_stage_admitted'] or row['numeric_factor_attempted'] or row['numerical_field_published'])


if __name__=='__main__':unittest.main()
