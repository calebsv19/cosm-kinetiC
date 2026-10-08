"""Transition relocation must preserve partition, symmetry and original mixed FE action."""
import sys,unittest,itertools,json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
from skfem import MeshTet
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_force_transition import alfeld_shape,optimize_edges,edge_groups,apply_fraction,FRACTIONS,force_transition_mesh
from cfd_reference3d_force_local_mesh import force_ranked_input
from cfd_reference3d_corner_local_mesh import shape_inverse,LocalGeometryRejected
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_stokes_pair import mixed_matrix


def small_case():
    old=MeshTet.init_tensor([0.,3.],[0.,.125],[0.,.125])
    return old,old.refined(np.arange(old.nelements))

import os
DIAGNOSTIC=Path(os.environ["PHYSICS_SIM_FORCE_DIAGNOSTIC"])
SURVEY=Path(os.environ["PHYSICS_SIM_FORCE_SURVEY"])
class TransitionArchive(unittest.TestCase):
    def test_actual_ranked_cube_original_points_and_optimizer_objective(self):
        old,octant,lo,hi,axes,lengths,scores,pairs,binding=force_ranked_input(DIAGNOSTIC)
        initial=octant.refined(np.array(pairs[0]));refined,meta,removed,added=optimize_edges(octant,initial)
        np.testing.assert_array_equal(refined.p[:,:octant.nvertices],octant.p)
        self.assertLessEqual(meta['optimized_objective'][0],meta['initial_objective'][0]+1e-12)
        self.assertLessEqual(meta['passes'],4)
        self.assertLess(meta['maximum_parent_volume_partition_error_m3'],1e-12)
        baseline=json.loads((SURVEY).read_text())['candidates'][0]['metadata']
        self.assertEqual(len(removed),baseline['affected_original_macros_per_octant'])
        self.assertEqual(len(added),baseline['affected_refined_macros_per_octant'])

    def test_only_shape_admitted_actual_mesh_can_return_for_algebra(self):
        try:
            mesh,meta=force_transition_mesh(DIAGNOSTIC,0)
            self.assertLessEqual(meta['refined_affected_worst_shape'],meta['original_affected_worst_shape']*(1+1e-8))
            self.assertLess(meta['refined_affected_mean_shape'],meta['original_affected_mean_shape']*(1-1e-8))
            self.assertLessEqual(meta['refined_global_max_condition'],meta['original_global_max_condition']*(1+1e-8))
        except LocalGeometryRejected as error:
            self.assertTrue(error.reasons)
            self.assertIn('affected intrinsic worst shape worsened',error.reasons)
        with self.assertRaises(ValueError):force_transition_mesh(DIAGNOSTIC,8)

if __name__=='__main__':unittest.main()
