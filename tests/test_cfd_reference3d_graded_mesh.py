"""Geometry, conformity, symmetry and pressure-rank controls for graded cubes."""
import sys
import unittest
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from skfem import MeshTet
from scipy.sparse import hstack
from cfd_reference3d_graded_mesh import graded_mesh
from cfd_fem_reference3d_solenoidal import mesh_for_case
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_quartic_pair import assemble_quartic,CubicPressureMass
from cfd_reference3d_quartic_modes import local_modes,global_modes


class Graded(unittest.TestCase):
    def test_geometry_symmetry_and_true_boundaries(self):
        for length in (4.,8.):
            for count in (2,4):
                for split in (False,True):
                    old,*_=mesh_for_case(length,count=count,split=split)
                    for exponent in (1,2):
                        mesh,lo,hi,axes,macros=graded_mesh(length,count,split,exponent)
                        np.testing.assert_array_equal(mesh.t,old.t)
                        self.assertEqual(mesh.nelements,4*macros)
                        self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,4*length-1,places=10)
                        lengths=np.array([length,2.,2.]);vertices=set(map(tuple,np.round(mesh.p.T,10)))
                        for axis in range(3):
                            reflected=mesh.p.copy();reflected[axis]=lengths[axis]-reflected[axis]
                            self.assertEqual(vertices,set(map(tuple,np.round(reflected.T,10))))
                        total_area=0
                        for name,faces in mesh.boundaries.items():
                            points=mesh.p[:,mesh.facets[:,faces]]
                            area=np.linalg.norm(np.cross((points[:,1]-points[:,0]).T,(points[:,2]-points[:,0]).T),axis=1).sum()/2
                            expected={'body':6.,'inlet':4.,'outlet':4.,'walls':8*length}[name]
                            self.assertAlmostEqual(area,expected,places=10);total_area+=area
                        self.assertAlmostEqual(total_area,8*length+14,places=10)
                        center=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
                        exterior=np.any(np.isclose(center,0)|np.isclose(center,lengths[:,None]),axis=0)
                        body=np.all((center>=lo[:,None]-1e-10)&(center<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(center,lo[:,None])|np.isclose(center,hi[:,None]),axis=0)
                        self.assertTrue(np.all(exterior|body))
    def test_legacy_and_actual_normal_subdivision(self):
        for split in (False,True):
            a,*_=graded_mesh(exponent=3,split=split);b,*_=mesh_for_case(split=split)
            np.testing.assert_array_equal(a.p,b.p);np.testing.assert_array_equal(a.t,b.t)
        base,lo,hi,axes,_=graded_mesh();fine,_,_,changed,_=graded_mesh(split=True)
        self.assertGreater(fine.nelements,base.nelements)
        self.assertTrue(set(axes[0]).issubset(set(changed[0])))
        for a,b in zip(axes[1:],changed[1:]):np.testing.assert_array_equal(a,b)
        self.assertAlmostEqual(min(lo[0]-changed[0][changed[0]<lo[0]]),.375)
        self.assertLess(np.linalg.cond(base.mapping().A.transpose(2,0,1)).max(),np.linalg.cond(mesh_for_case()[0].mapping().A.transpose(2,0,1)).max())
    def test_outer_subdivision_preserves_near_body_and_true_boundaries(self):
        for split in (False,True):
            old,lo,hi,axes,_=graded_mesh(exponent=3,split=split)
            mesh,_,_,changed,macros=graded_mesh(exponent=3,split=split,outer_layers=3)
            self.assertGreater(mesh.nelements,old.nelements)
            self.assertEqual(mesh.nelements,4*macros)
            self.assertTrue(set(axes[0]).issubset(set(changed[0])))
            for a,b in zip(axes[1:],changed[1:]):np.testing.assert_array_equal(a,b)
            near=(axes[0]>=axes[0][1])&(axes[0]<=axes[0][-2])
            np.testing.assert_array_equal(axes[0][near],changed[0][(changed[0]>=axes[0][1])&(changed[0]<=axes[0][-2])])
            center=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
            exterior=np.any(np.isclose(center,0)|np.isclose(center,np.array([4.,2.,2.])[:,None]),axis=0)
            body=np.all((center>=lo[:,None]-1e-10)&(center<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(center,lo[:,None])|np.isclose(center,hi[:,None]),axis=0)
            self.assertTrue(np.all(exterior|body))
            self.assertAlmostEqual(np.abs(mesh.mapping().detA).sum()/6,15.,places=10)
            self.assertLess(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max(),np.linalg.cond(old.mapping().A.transpose(2,0,1)).max())

    def test_observed_linear_success_with_divergence_failure(self):
        from cfd_reference3d_graded_probe import numerical_failure_reasons
        # Actual diagnostics of the retained outer3 normal run: linear convergence
        # alone cannot publish a numerically accepted field.
        row=dict(info=0,final_residual={'true_residual':8.452212853068e-11},
            flux_error=3.8352350809067737e-11,volume_divergence_max_s_inv=1.1700477967903876e-8,
            physical_energy_imbalance=4.358603880651259e-11,physical_dissipation_w=.00018,
            tetrahedra=10176,iterations=390,peak_rss_bytes=1418*1024**2,wall_s=56.47)
        self.assertEqual(numerical_failure_reasons(row),['volume_divergence'])
        row['volume_divergence_max_s_inv']=4.251118385073917e-9
        self.assertEqual(numerical_failure_reasons(row),[])

    def test_resource_gate_after_serialization_cannot_publish(self):
        from tempfile import TemporaryDirectory
        from unittest.mock import patch
        from types import SimpleNamespace
        import time
        from cfd_reference3d_graded_probe import publish_snapshot
        row=dict(info=0,final_residual={'true_residual':1e-11},flux_error=1e-12,
            volume_divergence_max_s_inv=1e-10,physical_energy_imbalance=1e-10,physical_dissipation_w=.1,
            tetrahedra=24,iterations=10,peak_rss_bytes=300*1024**2,wall_s=1.)
        with TemporaryDirectory() as directory:
            snapshot=Path(directory)/'field.npz'
            with patch('cfd_reference3d_graded_probe.resource.getrusage',return_value=SimpleNamespace(ru_maxrss=1800*1024**2+1)):
                self.assertFalse(publish_snapshot(snapshot,{'test_field':np.array([1.])},row,time.monotonic()))
            self.assertFalse(snapshot.exists())
            self.assertFalse(snapshot.with_name(snapshot.name+'.pending.npz').exists())
            self.assertFalse(row['numerically_accepted'])
            self.assertEqual(row['numerical_failure_reasons'],['resources'])
            row['peak_rss_bytes']=300*1024**2
            self.assertTrue(publish_snapshot(snapshot,{'test_field':np.array([1.])},row,time.monotonic()))
            with np.load(snapshot) as field:self.assertTrue(bool(field['numerically_accepted']))

    def test_local_and_global_modes_on_affine_controls(self):
        parent=MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.]))
        mesh=alfeld_split(parent);ub,pb,A,blocks=assemble_quartic(mesh,.1,64);mass=CubicPressureMass(pb,.1)
        local=local_modes(mesh,ub,pb,A,blocks,mass,parent.nelements,.1)
        self.assertTrue(local['all_mean_zero_modes_detected'])
        self.assertLess(local['maximum_constant_gradient_relative_error'],1e-12)
        global_check=global_modes(hstack(blocks,format='csr'),A,mass,parent.nelements,.1)
        self.assertEqual(global_check['near_null_modes_below_1e_12'],0)
        self.assertGreater(global_check['constant_pressure_gradient_norm'],0)


if __name__=='__main__':unittest.main()
