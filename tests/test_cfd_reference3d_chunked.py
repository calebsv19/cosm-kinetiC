"""Same forms, mass and physical observations across bounded-cell assembly."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_stokes_pair import assemble_pair,mixed_matrix
from scipy.sparse import hstack
from cfd_reference3d_preconditioner import PressureMass,matrix_sha
from cfd_reference3d_chunked import assemble_chunked,chunked_mass,volume_metrics,diagnose_chunked,streamed_matrix_sha,reaction_weights,mixed_matrix_lean,MixedOperator,saddle_digest
from cfd_reference3d_consistency import diagnose
from cfd_fem_reference3d_solenoidal import mesh_for_case


class ChunkedForms(unittest.TestCase):
    def test_matrix_mass_and_energy(self):
        m=alfeld_split(MeshTet.init_tensor(*[np.linspace(0,L,3) for L in (1.,1.5,2.)]))
        ub,pb,A,B=assemble_pair(m,.1);rng=np.random.default_rng(67)
        u=rng.normal(size=(3,ub.N));p=rng.normal(size=pb.N)
        g=np.stack([ub.interpolate(v).grad for v in u]);div=np.einsum('ii...->...',g);e=.5*(g+g.swapaxes(0,1))
        exact_D=float(np.sum(.2*np.einsum('ij...,ij...->...',e,e)*ub.dx))
        for chunk in (1,17,128):
            cu,cp,CA,CB=assemble_chunked(m,.1,chunk)
            for a,b in zip([A,*B],[CA,*CB]):
                difference=a-b
                if difference.nnz:self.assertLess(np.max(np.abs(difference.data)),1e-12)
            np.testing.assert_allclose(chunked_mass(cp,.1).solve(p),PressureMass(pb,.1).solve(p),rtol=1e-12,atol=1e-11)
            D,l2,maximum=volume_metrics(m,cu,u,.1,chunk)
            self.assertAlmostEqual(D/exact_D,1,places=12)
            self.assertAlmostEqual(l2/np.sqrt(np.sum(div**2*ub.dx)),1,places=12)
            self.assertAlmostEqual(maximum/np.max(np.abs(div)),1,places=12)

    def test_stream_digest_and_compressed_reaction(self):
        m,*_=mesh_for_case(count=2);ub,pb,A,B=assemble_chunked(m,.1,512)
        expected=matrix_sha(A);before=A.copy();self.assertEqual(streamed_matrix_sha(A),expected)
        self.assertEqual((A-before).nnz,0)
        rng=np.random.default_rng(69);u=rng.normal(size=(3,ub.N));p=rng.normal(size=pb.N)
        body=ub.get_dofs('body').all();weights=reaction_weights(A,B,body)
        for axis in range(3):
            exact=float((A@u[axis]+B[axis].T@p)[body].sum())
            self.assertAlmostEqual(exact,float(weights[0]@u[axis]+weights[1][axis]@p),places=11)

    def test_csr_saddle_equals_original(self):
        m=alfeld_split(MeshTet.init_tensor(*[np.linspace(0,L,3) for L in (1.,1.5,2.)]))
        ub,pb,A,B=assemble_chunked(m,.1,128)
        free=np.setdiff1d(np.arange(ub.N),ub.get_dofs().all())
        original=mixed_matrix(A,B,free)
        lean=mixed_matrix_lean(A[free][:,free],hstack([b[:,free] for b in B],format='csr'))
        self.assertEqual(matrix_sha(original),matrix_sha(lean))
        self.assertEqual((original-lean).nnz,0)

    def test_implicit_operator_and_digest(self):
        m=alfeld_split(MeshTet.init_tensor(*[np.linspace(0,L,3) for L in (1.,1.5,2.)]))
        ub,pb,A,B=assemble_chunked(m,.1,128)
        free=np.setdiff1d(np.arange(ub.N),ub.get_dofs().all());Af=A[free][:,free]
        BF=hstack([b[:,free] for b in B],format='csr')
        exact=mixed_matrix(A,B,free);implicit=MixedOperator.build(Af,BF)
        self.assertEqual(saddle_digest(Af,BF),matrix_sha(exact))
        x=np.random.default_rng(70).normal(size=exact.shape[0]);y=np.random.default_rng(71).normal(size=exact.shape[0])
        np.testing.assert_allclose(implicit@x,exact@x,atol=2e-13)
        self.assertAlmostEqual(float(x@(implicit@y)),float(y@(implicit@x)),places=10)

    def test_lifts_with_non_solenoidal_field(self):
        m,lo,hi,*_=mesh_for_case(count=2);ub,pb,*_=assemble_pair(m,.1)
        u=np.random.default_rng(68).normal(size=(3,ub.N))
        u[:,ub.get_dofs(['walls','body']).all()]=0
        p=.03+.002*pb.doflocs[0]
        a=diagnose(m,ub,pb,u,p,.1,lo,hi)
        cu,cp,*_=assemble_chunked(m,.1,512);b=diagnose_chunked(m,cu,cp,u,p,.1,lo,hi,512)
        np.testing.assert_allclose(a['normal_load_n'],b['normal_load_n'],atol=1e-13)
        for x,y in zip(a['volume_lifts'],b['volume_lifts']):
            for key in ('vector_laplacian_load_n','symmetric_stress_load_n','divergence_correction_n'):
                self.assertAlmostEqual(x[key],y[key],places=11)


if __name__=='__main__':unittest.main()
