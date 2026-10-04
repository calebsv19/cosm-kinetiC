"""Signed attribution must close the original physical stress identities."""
import sys,unittest,tempfile
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'tests'))
from test_cfd_reference3d_quartic_equilibrium import cube_case,polynomials
from cfd_reference3d_quartic_equilibrium import diagnose_equilibrium as original
from cfd_reference3d_signed_equilibrium import diagnose_equilibrium as signed
from cfd_reference3d_force_attribution import summarize_signed

class SignedForce(unittest.TestCase):
    def compare(self,mesh,ub,pb,lo,hi,u,p,order=8):
        old=original(mesh,ub,pb,u,p,.1,lo,hi,128,order)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'signed.npz'
            new=signed(mesh,ub,pb,u,p,.1,lo,hi,128,order,path)
            for key in old:self.assertEqual(new[key],old[key])
            summary=new['signed_force_attribution']
            with np.load(path,allow_pickle=False) as saved:
                self.assertEqual(saved['weighted_volume_divergence_n'].shape,(2,2,3,mesh.nelements))
                self.assertEqual(saved['interior_jump_n'].shape[-1],len(np.flatnonzero(mesh.f2t[1]!=-1)))
                for i,lift in enumerate(new['lifts']):
                    for j,part in enumerate(('pressure','viscous')):
                        np.testing.assert_allclose(saved['weighted_volume_divergence_n'][i,j].sum(axis=-1),lift[part]['weighted_volume_divergence_n'],rtol=1e-11,atol=1e-10)
                        np.testing.assert_allclose(saved['interior_jump_n'][i,j].sum(axis=-1),lift[part]['interior_jump_n'],rtol=1e-11,atol=1e-10)
                    total=np.sum([lift[part]['raw_minus_weak_n'] for part in ('pressure','viscous')],axis=0)
                    np.testing.assert_allclose(saved['cell_net_n'][i].sum(axis=(0,2)),total,rtol=1e-10,atol=1e-8)
                    bands=summary['lifts'][i]['centroid_bands']
                    np.testing.assert_allclose(np.sum([b['signed_total_n'] for b in bands],axis=0),total,rtol=1e-10,atol=1e-8)
                    self.assertEqual(sum(b['cell_count'] for b in bands),mesh.nelements)
                expected=np.max(np.abs(saved['cell_net_n'].sum(axis=1)[:,0]),axis=0)
                np.testing.assert_array_equal(expected,saved['cell_score_n'])
            self.assertFalse(summary['physical_accuracy_certified'])
            with self.assertRaises(FileExistsError):signed(mesh,ub,pb,u,p,.1,lo,hi,128,order,path)
        return summary

    def test_arbitrary_discontinuous_non_solenoidal_field(self):
        mesh,ub,pb,lo,hi=cube_case();rng=np.random.default_rng(81)
        self.compare(mesh,ub,pb,lo,hi,rng.normal(size=(3,ub.N))*.02,rng.normal(size=pb.N)*.03)

    def test_smooth_quartic_velocity_and_cubic_pressure(self):
        mesh,ub,pb,lo,hi=cube_case();u=polynomials(ub.doflocs);p=.02+.003*pb.doflocs[0]+.004*pb.doflocs[1]**3
        self.compare(mesh,ub,pb,lo,hi,u,p)

    def test_pressure_only_and_facet_quadrature(self):
        mesh,ub,pb,lo,hi=cube_case();p=np.random.default_rng(82).normal(size=pb.N)*.03;u=np.zeros((3,ub.N))
        a=self.compare(mesh,ub,pb,lo,hi,u,p,8);b=self.compare(mesh,ub,pb,lo,hi,u,p,10)
        for left,right in zip(a['lifts'],b['lifts']):np.testing.assert_allclose(left['signed_total_gap_n'],right['signed_total_gap_n'],rtol=1e-10,atol=1e-10)

    def test_signed_half_face_allocation_cancellation_and_invalid_adjacency(self):
        volume=np.zeros((2,2,3,3));face=np.zeros((2,2,3,2));adj=np.array([[0,1],[1,2]])
        volume[:,0,0]=[1.,2.,3.];volume[:,1,0]=[-.5,-1.,-1.5];face[:,0,0]=[4.,-2.];face[:,1,0]=[-2.,1.]
        cells=np.array([[0.,.05,.2],[0.,0.,0.],[0.,0.,0.]]);facets=(cells[:,:-1]+cells[:,1:])/2;lo=np.zeros(3);hi=np.ones(3)
        row,net,score=summarize_signed(volume,face,adj,cells,facets,lo,hi,[.25,.4])
        np.testing.assert_allclose(net[:,0,0],[[1.,-1.,-4.]]*2)
        np.testing.assert_allclose(net[:,1,0],[[-.5,.5,2.]]*2)
        np.testing.assert_allclose(score,[.5,.5,2.])
        self.assertGreater(row['lifts'][0]['absolute_cell_parts_n'][0],row['lifts'][0]['absolute_cell_total_n'][0])
        with self.assertRaises(ValueError):summarize_signed(volume,face,np.array([[0,-1],[1,2]]),cells,facets,lo,hi,[.25,.4])
        volume[0,0,0,0]=np.nan
        with self.assertRaises(ValueError):summarize_signed(volume,face,adj,cells,facets,lo,hi,[.25,.4])

if __name__=='__main__':unittest.main()
