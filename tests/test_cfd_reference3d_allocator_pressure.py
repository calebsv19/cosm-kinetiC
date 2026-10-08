"""Live FE/action/factor-owner proofs for explicit allocator pressure control."""
import os
import sys
import unittest
from unittest.mock import patch,Mock
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference_test_support import library_path
from test_cfd_reference3d_bounded_condensed import fixture
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem
from cfd_reference3d_shared_factor import BlockTriangle,SharedTriangleFactor,storage_sha
from cfd_reference3d_allocator_pressure import release_free_pages
from cfd_reference3d_quartic_pair import assemble_quartic
LIB=library_path('build/c3d-cholesky/support/factor.dylib')


def arrays(C):
    return tuple(a for m in (C.velocity.upper,C.coupling,C.pressure.upper) for a in (m.indptr,m.indices,m.data))


class Pressure(unittest.TestCase):
    def test_anisotropic_refined_original_equations_and_nonzero_rhs_preserved(self):
        mesh=fixture(True);old=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        system=TriangleCondensedSystem(mesh,.1,fixed_boundaries=('walls',),assembly_batch=7)
        C=BlockTriangle(system.upper_matrix,len(system.retained_free)-system.nmacro)
        rng=np.random.default_rng(841);rhs=rng.normal(size=3*old.ub.N+old.pb.N)*.01
        z=np.zeros(old.shape[0]);z[system.retained_free]=rng.normal(size=C.shape[0])
        before_rhs=system.reduce_rhs(rhs);u,p=system.reconstruct(z,rhs)
        pressure=C.pressure.upper.diagonal().copy();self.assertTrue(np.any(pressure!=0))
        live=arrays(C)+(rhs,z,system.volumes);digest=storage_sha(*live)
        row=release_free_pages(live,lambda:C@z[system.retained_free])
        self.assertTrue(row['live_input_preserved'] and row['action_preserved'])
        self.assertEqual(row['action_max_absolute_change'],0.)
        self.assertEqual(storage_sha(*live),digest)
        self.assertGreaterEqual(row['owned_high_water_after_bytes'],row['owned_high_water_before_bytes'])
        self.assertGreaterEqual(row['reported_released_bytes'],0)
        np.testing.assert_array_equal(system.reduce_rhs(rhs),before_rhs)
        after_u,after_p=system.reconstruct(z,rhs)
        np.testing.assert_array_equal(after_u,u);np.testing.assert_array_equal(after_p,p)
        np.testing.assert_array_equal(C.pressure.upper.diagonal(),pressure)
        np.testing.assert_array_equal(pressure,old.matrix.diagonal()[3*old.nt:])
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        actual=full_action(mesh,ub,pb,after_u,after_p,.1,128)
        expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-9)
        np.testing.assert_allclose(C@z[system.retained_free],old.matrix[system.retained_free][:,system.retained_free]@z[system.retained_free],rtol=1e-10,atol=1e-9)

    def test_existing_owned_factor_and_dense_inverse_survive_pressure(self):
        rng=np.random.default_rng(842);a=rng.normal(size=(31,31));dense=a.T@a+np.eye(31)*.1
        upper=csr_matrix(np.triu(dense));factor=SharedTriangleFactor(upper,LIB)
        rhs=rng.normal(size=31);before=factor.solve(rhs)
        live=(factor.starts,factor.rows,factor.values,rhs)
        row=release_free_pages(live,lambda:factor.solve(rhs))
        self.assertTrue(row['action_preserved'] and factor.input_unchanged())
        np.testing.assert_array_equal(factor.solve(rhs),before)
        np.testing.assert_allclose(before,np.linalg.solve(dense,rhs),rtol=1e-10,atol=1e-10)
        factor.close();factor.close()
        with self.assertRaises(ValueError):factor.solve(rhs)

    def test_invalid_live_input_rejected_before_pressure_api(self):
        for live in ((),(np.arange(9.)[::2],),(np.array([np.nan]),),([1.,2.],)):
            with patch('cfd_reference3d_allocator_pressure.pressure_api') as api:
                with self.assertRaises(ValueError):release_free_pages(live)
                api.assert_not_called()
        function=Mock(return_value=0)
        with patch('cfd_reference3d_allocator_pressure.pressure_api',return_value=(None,function)):
            with self.assertRaises(ValueError):release_free_pages((np.ones(3),),lambda:np.array([np.nan]))
        function.assert_not_called()

    def test_unsupported_platform_or_missing_api_is_explicit(self):
        live=(np.arange(4.),);before=live[0].copy()
        with patch('cfd_reference3d_allocator_pressure.sys.platform','linux'):
            with self.assertRaisesRegex(RuntimeError,'requires macOS'):release_free_pages(live)
        with patch('cfd_reference3d_allocator_pressure.ct.CDLL',return_value=object()):
            with self.assertRaisesRegex(RuntimeError,'API unavailable'):release_free_pages(live)
        np.testing.assert_array_equal(live[0],before)

    def test_zero_report_is_valid_observation_without_resource_success_claim(self):
        function=Mock(return_value=0)
        with patch('cfd_reference3d_allocator_pressure.pressure_api',return_value=(None,function)):
            row=release_free_pages((np.arange(4.),))
        function.assert_called_once_with(None,0)
        self.assertEqual(row['reported_released_bytes'],0)
        self.assertFalse(row['action_preserved']);self.assertIsNone(row['action_max_absolute_change'])
        self.assertNotIn('resource_accepted',row)


if __name__=='__main__':unittest.main()
