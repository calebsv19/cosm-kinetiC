"""Verify local macro pressure nullity diagnostics, including a broken coupling."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_chunked import assemble_chunked,chunked_mass
from cfd_reference3d_pressure_modes import local_macro_pressure_modes


class PressureModes(unittest.TestCase):
    def test_all_local_mean_zero_modes(self):
        parent=MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.]))
        mesh=alfeld_split(parent);u,p,A,B=assemble_chunked(mesh,.1,8)
        row=local_macro_pressure_modes(mesh,u,p,A,B,chunked_mass(p,.1),parent.nelements,.1)
        self.assertTrue(row['all_mean_zero_modes_detected'])
        self.assertEqual(row['rank_counts'],{39:6})
        self.assertLess(row['maximum_constant_gradient_relative_error'],1e-12)
        self.assertGreater(row['minimum_positive_generalized_eigenvalue'],1e-4)

    def test_missing_coupling_is_detected(self):
        parent=MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.]),np.array([0.,1.]))
        mesh=alfeld_split(parent);u,p,A,B=assemble_chunked(mesh,.1,8)
        broken=[b*0 for b in B]
        row=local_macro_pressure_modes(mesh,u,p,A,broken,chunked_mass(p,.1),parent.nelements,.1)
        self.assertFalse(row['all_mean_zero_modes_detected'])
        self.assertEqual(row['rank_counts'],{0:6})


if __name__=='__main__':unittest.main()
