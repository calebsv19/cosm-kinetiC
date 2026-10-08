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

class Transition(unittest.TestCase):
    def test_fast_intrinsic_shape_matches_full_mapping_labels_and_scale(self):
        old=MeshTet.init_tensor([0.,3.],[0.,.125],[0.,.125])
        for scale in (.01,1.,100.):
            for permutation in itertools.permutations(range(4)):
                mesh=MeshTet(old.p*scale,old.t[list(permutation)])
                np.testing.assert_allclose(alfeld_shape(mesh.p,mesh.t),shape_inverse(alfeld_split(mesh)),rtol=1e-12,atol=1e-12)

    def test_original_edge_groups_shared_topology_symmetry_partition_and_bounds(self):
        old,initial=small_case();result,meta,removed,added=optimize_edges(old,initial)
        np.testing.assert_array_equal(result.p[:,:old.nvertices],old.p)
        np.testing.assert_array_equal(result.t,initial.t)
        np.testing.assert_array_equal(result.f2t,initial.f2t)
        self.assertLess(meta['maximum_parent_volume_partition_error_m3'],1e-12)
        self.assertLessEqual(meta['passes'],4)
        self.assertLessEqual(meta['optimized_objective'][0],meta['initial_objective'][0]+1e-12)
        self.assertTrue(set(meta['fractions']).issubset(FRACTIONS))
        self.assertGreater(np.abs(result.mapping().detA).min(),0)
        self.assertAlmostEqual(np.abs(result.mapping().detA).sum(),np.abs(old.mapping().detA).sum(),places=12)
        for group,fraction in zip(meta['groups'],meta['fractions']):
            for entry in group['entries']:
                a,b=entry['edge'];t=1-fraction if entry['reverse'] else fraction
                np.testing.assert_allclose(result.p[:,entry['vertex']],(1-t)*old.p[:,a]+t*old.p[:,b],rtol=0,atol=1e-14)
            if group['fixed_midpoint']:self.assertEqual(fraction,.5)
        self.assertLess(cKDTree(result.p.T).query(result.p.T[:,[0,2,1]])[0].max(),1e-12)
        with self.assertRaises(ValueError):optimize_edges(old,initial,5)
        with self.assertRaises(ValueError):apply_fraction(result.p.copy(),old.p,meta['groups'][0],.1)

    def test_full_original_fe_action_pressure_and_nonzero_load_reconstruction(self):
        old=MeshTet.init_tensor([0.,1.25],[0.,.5],[0.,.5]);initial=old.refined(np.arange(old.nelements))
        refined,_,_,_=optimize_edges(old,initial);mesh=alfeld_split(refined)
        system=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        ub,pb,A,B=assemble_quartic(mesh,.1,128);K=mixed_matrix(A,B,np.arange(ub.N))
        rng=np.random.default_rng(2012);rhs=rng.normal(size=K.shape[0]);z=rng.normal(size=system.shape[0])
        u,p=system.reconstruct(z,rhs);actual=full_action(mesh,ub,pb,u,p,.1,128)
        np.testing.assert_allclose(actual,K@np.r_[u.ravel(),p],rtol=1e-10,atol=2e-9)
        residual=actual-rhs
        retained=np.r_[residual[:3*ub.N].reshape(3,-1)[:,system.trace].ravel(),[residual[3*ub.N:][ids].sum() for _,_,ids,_ in system.records]]
        np.testing.assert_allclose(retained,system.matrix@z-system.reduce_rhs(rhs),rtol=1e-10,atol=2e-9)
        np.testing.assert_allclose(system.retained_field(u,p),z,rtol=1e-10,atol=1e-10)
        for _,indices,pressure,_ in system.records:
            self.assertLess(np.max(np.abs(residual[:3*ub.N].reshape(3,-1)[:,indices[system.ref.bubble]])),2e-9)
            self.assertLess(np.max(np.abs(system.ref.Q.T@residual[3*ub.N:][pressure])),2e-9)

if __name__=='__main__':unittest.main()
