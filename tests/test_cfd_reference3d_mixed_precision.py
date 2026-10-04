"""Approximate factor ownership, unchanged full FE and flexible true-residual controls."""
import sys,unittest,weakref,gc
from unittest.mock import patch
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_mixed_workspace import MixedWorkspaceCholesky,numeric_stage_admission
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_shared_factor import BlockTriangle,storage_sha
from cfd_reference3d_flexible import flexible_gmres,basis_reservation
from cfd_reference3d_condensed import full_action
from cfd_reference3d_quartic_pair import assemble_quartic
from test_cfd_reference3d_coarse_velocity import system_fixture
LIB=ROOT/'build/c3d-mixed-precision/support/factor.dylib'

def triangle(A):return VectorTriangle(csr_matrix(np.triu(A)),LIB)

class Precision(unittest.TestCase):
    def test_dense_approximate_inverse_original_action_owner_and_repeats(self):
        rng=np.random.default_rng(183);B=rng.normal(size=(18,18));A=B.T@B+5*np.eye(18);t=triangle(A);digest=t.input_sha256
        inverse=MixedWorkspaceCholesky(t,LIB);x=rng.normal(size=18);before=t@x
        value=inverse.solve(x);np.testing.assert_allclose(value,np.linalg.solve(A,x),atol=2e-7,rtol=2e-6)
        np.testing.assert_array_equal(inverse.solve(x),value);np.testing.assert_array_equal(t@x,before)
        self.assertEqual(t.input_sha256,digest);self.assertTrue(inverse.input_unchanged())
        self.assertEqual(inverse.metadata['preconditioner_value_dtype'],'float32');self.assertTrue(inverse.metadata['user_factor_storage_verified'])
        self.assertEqual(inverse.metadata['factor_input_allocation_bytes'],len(t.values)*4)
        owner=weakref.ref(t);del t;gc.collect();self.assertIsNotNone(owner());inverse.close()
        with self.assertRaises(ValueError):inverse.solve(x)

    def test_anisotropic_original_full_fe_pressure_rhs_reconstruction_unchanged(self):
        system=system_fixture();nv=len(system.retained_free)-system.nmacro;C=BlockTriangle(system.upper_matrix,nv)
        rng=np.random.default_rng(184);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01
        z=np.zeros(system.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0]);reduced=system.reduce_rhs(rhs);u,p=system.reconstruct(z,rhs)
        velocity=VectorTriangle(C.velocity.upper,LIB);C.velocity=velocity;v=rng.normal(size=C.shape[0]);before=C@v;inverse=MixedWorkspaceCholesky(velocity,LIB)
        solved=inverse.solve(v[:nv]);self.assertTrue(np.all(np.isfinite(solved)))
        corrected,info,diagnostic=flexible_gmres(velocity,v[:nv],inverse.solve,target=1e-10)
        self.assertEqual(info,0);self.assertLess(np.linalg.norm(velocity@corrected-v[:nv])/np.linalg.norm(v[:nv]),1e-10)
        self.assertTrue(inverse.input_unchanged());inverse.close();np.testing.assert_array_equal(C@v,before)
        np.testing.assert_array_equal(system.reduce_rhs(rhs),reduced);uu,pp=system.reconstruct(z,rhs);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p)
        ub,pb,A,B=assemble_quartic(system.mesh,.1,128);expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)

    def test_refined_original_pressure_load_full_fe_and_tight_correction(self):
        from test_cfd_reference3d_bounded_condensed import fixture
        from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
        system=TriangleCondensedSystem(fixture(True),.1,fixed_boundaries=('walls',),assembly_batch=7);nv=len(system.retained_free)-system.nmacro
        C=BlockTriangle(system.upper_matrix,nv);velocity=VectorTriangle(C.velocity.upper,LIB);C.velocity=velocity
        rng=np.random.default_rng(186);rhs=rng.normal(size=3*system.ub.N+system.pb.N)*.01;reduced=system.reduce_rhs(rhs);x=rng.normal(size=C.shape[0]);before=C@x
        retained=np.zeros(system.shape[0]);retained[system.retained_free]=x;u,p=system.reconstruct(retained,rhs);inverse=MixedWorkspaceCholesky(velocity,LIB)
        corrected,info,_=flexible_gmres(velocity,x[:nv],inverse.solve,target=1e-10)
        self.assertEqual(info,0);self.assertLess(np.linalg.norm(velocity@corrected-x[:nv])/np.linalg.norm(x[:nv]),1e-10)
        inverse.close();np.testing.assert_array_equal(C@x,before);np.testing.assert_array_equal(system.reduce_rhs(rhs),reduced)
        uu,pp=system.reconstruct(retained,rhs);np.testing.assert_array_equal(uu,u);np.testing.assert_array_equal(pp,p)
        ub,pb,A,B=assemble_quartic(system.mesh,.1,128);expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(full_action(system.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)

    def test_nonpositive_rounding_invalid_closed_and_partial_lifetime(self):
        for A in (np.diag([1.,1.,-1.]),np.diag([1.,1.,1e-100]),np.diag([1.,1.,1e100])):
            with self.assertRaises(ValueError):MixedWorkspaceCholesky(triangle(A),LIB)
        inverse=MixedWorkspaceCholesky(triangle(np.eye(3)),LIB)
        for x in (np.ones(2),np.full(3,np.nan),np.full(3,1e100),np.full(3,1e-100)):
            with self.assertRaises(ValueError):inverse.solve(x)
        inverse.close();inverse.close()
        held=MixedWorkspaceCholesky.__new__(MixedWorkspaceCholesky)
        def fail(phase):
            if phase=='workspace_symbolic_ready':raise RuntimeError('owned stage rejection')
        with self.assertRaises(RuntimeError):MixedWorkspaceCholesky.__init__(held,triangle(np.eye(3)),LIB,stage_callback=fail)
        self.assertIsNone(held._handle)

    def test_budget_reserves_flexible_arrays_and_one_hundred_cleanup(self):
        t=triangle(np.eye(3))
        for _ in range(100):
            f=MixedWorkspaceCholesky(t,LIB,pressure_control=False);np.testing.assert_array_equal(f.solve(np.ones(3)),np.ones(3));f.close()
        f=MixedWorkspaceCholesky(t,LIB);amount=basis_reservation(3,60);row=numeric_stage_admission(f,amount)
        self.assertEqual(row['basis_reservation_bytes'],amount)
        self.assertEqual(row['estimated_numeric_stage_bytes'],sum(row[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes')));f.close()

class Flexible(unittest.TestCase):
    def test_indefinite_mixed_all_modes_and_varying_approximate_action(self):
        rng=np.random.default_rng(185);G=rng.normal(size=(12,4));A=np.block([[np.eye(12)*2,G],[G.T,-np.eye(4)*.2]]);b=rng.normal(size=16);calls=[0]
        def varied(v):
            calls[0]+=1;return np.asarray(v/(1.+.01*calls[0]),dtype=np.float32).astype(float)
        events=[];x,info,row=flexible_gmres(A,b,varied,restart=20,callback=lambda x,i,m:events.append((i,m)))
        self.assertEqual(info,0);self.assertLess(np.linalg.norm(A@x-b)/np.linalg.norm(b),1e-10)
        np.testing.assert_allclose(x,np.linalg.solve(A,b),atol=1e-9,rtol=1e-9)
        self.assertGreater(calls[0],1);self.assertLessEqual(row['basis_array_bytes'],row['basis_reservation_bytes']);self.assertTrue(events)

    def test_restart_cap_and_projection_cannot_certify_unresolved_true_residual(self):
        A=np.diag(np.arange(1.,25.));b=np.ones(24);x,info,row=flexible_gmres(A,b,lambda v:v,maxiter=3,restart=2)
        self.assertEqual(info,1);self.assertEqual(row['iterations'],3);self.assertGreater(row['final_true_metric'],1e-10)
        x,info,row=flexible_gmres(np.zeros((3,3)),np.ones(3),lambda v:v,restart=3)
        self.assertEqual(info,1);self.assertEqual(row['final_true_metric'],1.);np.testing.assert_array_equal(x,np.zeros(3))

    def test_nonfinite_wrong_shape_and_invalid_request_rejected(self):
        for pc in (lambda v:np.full(3,np.nan),lambda v:np.ones(2)):
            with self.assertRaises(ValueError):flexible_gmres(np.eye(3),np.ones(3),pc)
        for kw in ({'maxiter':3001},{'restart':61},{'target':0.}):
            with self.assertRaises(ValueError):flexible_gmres(np.eye(3),np.ones(3),lambda v:v,**kw)
        with self.assertRaises(ValueError):flexible_gmres(np.full((3,3),np.nan),np.ones(3),lambda v:v)

    def test_zero_rhs_and_original_true_metric_controls_termination(self):
        x,info,row=flexible_gmres(np.eye(3),np.zeros(3),lambda v:v);self.assertEqual(info,0);self.assertEqual(row['iterations'],0)
        x,info,row=flexible_gmres(np.diag([1.,2.,3.]),np.ones(3),lambda v:v,metric=lambda r:float(np.linalg.norm(r)))
        self.assertEqual(info,0);self.assertLess(np.linalg.norm(np.diag([1.,2.,3.])@x-np.ones(3)),1e-10)

class Stage(unittest.TestCase):
    def stage_case(self,inflated=False):
        import cfd_reference3d_mixed_precision_stage_probe as stage
        from test_cfd_reference3d_factor_catalog_stage import Stage as Fixture
        from cfd_reference3d_allocator_pressure import release_free_pages
        self.assertIs(stage.VectorWorkspaceCholesky,MixedWorkspaceCholesky)
        def measure(*args,**kwargs):
            row=release_free_pages(*args,**kwargs)
            if inflated:row['current_rss_after_bytes']=1800*1024**2
            return row
        with patch.object(stage,'domain_mesh',return_value=Fixture().fixture()),patch.object(stage.workspace_module,'release_free_pages',side_effect=measure):
            return stage.run(length=8.,count=6,split=True,outer_layers=2,domain_mesh_mode='held_l4',factor_library=LIB)

    def test_fitting_stage_is_not_a_numeric_inverse_or_field(self):
        row=self.stage_case();self.assertTrue(row['diagnostic_accepted'] and row['symbolic_handle_cleanup_verified'])
        self.assertTrue(row['admission']['numeric_stage_admitted']);self.assertGreater(row['admission']['basis_reservation_bytes'],0)
        self.assertFalse(row['numeric_factor_attempted'] or row['numerically_accepted'] or row['numerical_field_published'])

    def test_over_budget_stage_cleans_symbolic_and_withholds_numeric(self):
        row=self.stage_case(True);self.assertTrue(row['symbolic_handle_cleanup_verified'])
        self.assertFalse(row['admission']['numeric_stage_admitted'] or row['numeric_factor_attempted'] or row['numerical_field_published'])

if __name__=='__main__':unittest.main()
