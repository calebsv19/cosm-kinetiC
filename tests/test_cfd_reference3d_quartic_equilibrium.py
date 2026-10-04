"""Independent Hessians and elementwise stress identity on non-equilibrium fields."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_chunked import cell_basis
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_quartic_equilibrium import velocity_hessian,diagnose_equilibrium


def polynomials(x):
    a,b,c=x
    return np.array([a**4+2*a*a*b+c*c,b**4+a*c*c,c**4+a*b*b*c])


def cube_case():
    lo=np.array([1.5,.5,.5]);hi=lo+1
    parent=MeshTet.init_tensor(np.array([0.,1.5,2.5,4.]),np.array([0.,.5,1.5,2.]),np.array([0.,.5,1.5,2.]))
    center=parent.p[:,parent.t].mean(axis=1)
    parent=parent.remove_elements(np.flatnonzero(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0)))
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=alfeld_split(parent).with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],4),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    ub,pb,*_=assemble_quartic(mesh,.1,128)
    return mesh,ub,pb,lo,hi


class Equilibrium(unittest.TestCase):
    def assert_identity(self,row,tolerance=1e-8):
        self.assertAlmostEqual(sum(row['equilibrium_indicator_squared_per_tet']),sum(row['h_squared_volume_defect_centroid_buckets'])+sum(row['h_weighted_jump_defect_centroid_buckets']),places=10)
        for lift in row['lifts']:
            for part in ('pressure','viscous'):
                self.assertLess(np.max(np.abs(lift[part]['identity_error_n'])),tolerance)
                for source,bucket in (('weighted_volume_divergence_n','volume_divergence_centroid_buckets_n'),('interior_jump_n','interior_jump_centroid_buckets_n')):
                    np.testing.assert_allclose(np.sum(lift[part][bucket],axis=0),lift[part][source],atol=tolerance)

    def test_physical_hessian_on_affine_mesh(self):
        parent=MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.]),np.array([0.,1.]))
        transform=np.array([[1.,.2,.3],[.1,1.5,.4],[-.2,.3,2.]])
        mesh=alfeld_split(MeshTet(transform@parent.p,parent.t))
        ub,pb,*_=assemble_quartic(mesh,.1,128);u=polynomials(ub.doflocs)
        cells=np.arange(mesh.nelements);basis=cell_basis(ub,cells);a,b,c=basis.global_coordinates()
        actual=velocity_hessian(ub,u,cells,basis.X);expected=np.zeros_like(actual)
        expected[0,0,0]=12*a*a+4*b;expected[0,0,1]=4*a;expected[0,1,0]=4*a;expected[0,2,2]=2
        expected[1,1,1]=12*b*b;expected[1,0,2]=2*c;expected[1,2,0]=2*c;expected[1,2,2]=2*a
        expected[2,2,2]=12*c*c;expected[2,0,1]=2*b*c;expected[2,1,0]=2*b*c;expected[2,1,1]=2*a*c
        expected[2,0,2]=b*b;expected[2,2,0]=b*b;expected[2,1,2]=2*a*b;expected[2,2,1]=2*a*b
        np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-9)

    def test_smooth_polynomial_stress_has_no_jump(self):
        mesh,ub,pb,lo,hi=cube_case();u=polynomials(ub.doflocs)
        p=.02+.003*pb.doflocs[0]+.004*pb.doflocs[1]**3
        row=diagnose_equilibrium(mesh,ub,pb,u,p,.1,lo,hi,128)
        self.assertLess(row['interior_stress_jump_l2'],1e-10)
        self.assertGreater(row['volume_strong_equilibrium_defect_l2'],1.)
        self.assert_identity(row)

    def test_arbitrary_non_solenoidal_and_discontinuous_field(self):
        mesh,ub,pb,lo,hi=cube_case();rng=np.random.default_rng(81)
        u=rng.normal(size=(3,ub.N))*.02;p=rng.normal(size=pb.N)*.03
        row=diagnose_equilibrium(mesh,ub,pb,u,p,.1,lo,hi,128)
        self.assertGreater(row['interior_stress_jump_l2'],.1)
        self.assertGreater(row['volume_strong_equilibrium_defect_l2'],1.)
        self.assert_identity(row)

    def test_pressure_only_and_quadrature(self):
        mesh,ub,pb,lo,hi=cube_case();u=np.zeros((3,ub.N));p=np.random.default_rng(82).normal(size=pb.N)*.03
        a=diagnose_equilibrium(mesh,ub,pb,u,p,.1,lo,hi,128,8)
        b=diagnose_equilibrium(mesh,ub,pb,u,p,.1,lo,hi,128,10)
        self.assert_identity(a);self.assert_identity(b)
        for left,right in zip(a['lifts'],b['lifts']):
            self.assertEqual(left['viscous']['raw_surface_load_n'],[0.]*3)
            np.testing.assert_allclose(left['pressure']['interior_jump_n'],right['pressure']['interior_jump_n'],atol=1e-12)


if __name__=='__main__':unittest.main()
