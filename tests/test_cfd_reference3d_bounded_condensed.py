"""Independent operator, RHS and field reconstruction proofs for bounded assembly."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_bounded_condensed import BoundedCondensedSystem
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_corner_local_mesh import edge_star_refined


def fixture(refined=False):
    parent=MeshTet.init_tensor([0.,1.3],[0.,.7],[0.,.2])
    if refined:parent=edge_star_refined(parent,(0,))
    mesh=alfeld_split(parent)
    return mesh.with_boundaries({'walls':lambda x:np.isclose(x[2],0)|np.isclose(x[2],.2)})


class Bounded(unittest.TestCase):
    def test_full_operator_matches_old_assembly_across_batches(self):
        for refined in (False,True):
            mesh=fixture(refined);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
            for batch in (1,7,64):
                new=BoundedCondensedSystem(mesh,.1,assembly_batch=batch)
                delta=new.reduced_matrix-old.matrix
                self.assertLess(np.linalg.norm(delta.data)/np.linalg.norm(old.matrix.data),1e-12)
                rng=np.random.default_rng(671);z=rng.normal(size=old.shape[0])
                np.testing.assert_allclose(new.reduced_matrix@z,old.matrix@z,atol=1e-10,rtol=1e-11)
                self.assertEqual(new.metadata['assembly']['coo_batch_allocation_bytes'],batch*103**2*16)
                self.assertEqual(new.metadata['assembly']['legacy_all_entry_coo_bytes'],mesh.nelements//4*103**2*16)
    def test_free_projection_and_symmetry_match_original(self):
        mesh=fixture(True);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        new=BoundedCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        free=new.retained_free;expected=old.matrix[free][:,free]
        delta=new.reduced_matrix-expected
        self.assertLess(np.linalg.norm(delta.data)/np.linalg.norm(expected.data),1e-12)
        symmetry=new.reduced_matrix-new.reduced_matrix.T
        self.assertLess(np.linalg.norm(symmetry.data)/np.linalg.norm(expected.data),1e-12)
        fixed=old.trace_inverse[old.ub.get_dofs('walls').all()]
        self.assertTrue(all(i not in set(free) for a in range(3) for i in fixed+a*old.nt))
        self.assertTrue(np.array_equal(free[-old.nmacro:],np.arange(old.nmacro)+3*old.nt))
    def test_nonzero_eliminated_rhs_reconstruction_and_original_action(self):
        mesh=fixture(True);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        new=BoundedCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        rng=np.random.default_rng(672);rhs=rng.normal(size=3*old.ub.N+old.pb.N)*.01
        z=np.zeros(old.shape[0]);z[new.retained_free]=rng.normal(size=len(new.retained_free))
        np.testing.assert_allclose(new.reduce_rhs(rhs),old.reduce_rhs(rhs),rtol=0,atol=1e-12)
        u,p=new.reconstruct(z,rhs);old_u,old_p=old.reconstruct(z,rhs)
        np.testing.assert_allclose(u,old_u,rtol=0,atol=1e-12);np.testing.assert_allclose(p,old_p,rtol=0,atol=1e-12)
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        actual=full_action(mesh,ub,pb,u,p,.1,128)
        explicit=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(actual,explicit,rtol=1e-10,atol=1e-9)
        trace_residual=(actual-rhs)[:3*ub.N].reshape(3,-1)[:,new.trace].ravel()
        # Original pressure rows transformed to exact macro constants by summation.
        pressure_residual=(actual-rhs)[3*ub.N:]
        reduced_pressure=np.array([pressure_residual[ids].sum() for _,_,ids,_ in new.records])
        expected=new.reduced_matrix@z[new.retained_free]-new.reduce_rhs(rhs)[new.retained_free]
        np.testing.assert_allclose(np.r_[trace_residual,reduced_pressure][new.retained_free],expected,rtol=1e-10,atol=1e-9)
    def test_macro_pressure_diagonal_roundoff_terms_are_preserved(self):
        mesh=fixture(True);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        new=BoundedCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        old_pressure=old.matrix.diagonal()[3*old.nt:]
        new_pressure=new.reduced_matrix.diagonal()[-old.nmacro:]
        self.assertTrue(np.any(old_pressure!=0))
        np.testing.assert_array_equal(new_pressure,old_pressure)

    def test_invalid_batch_is_rejected_before_allocating_mesh_algebra(self):
        mesh=fixture()
        for batch in (0,-1,257,1.5):
            with self.assertRaises(ValueError):BoundedCondensedSystem(mesh,.1,assembly_batch=batch)


if __name__=='__main__':unittest.main()
