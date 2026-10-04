"""Stress/divergence identity tests independent of cube solve accuracy."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet, Basis, ElementTetP2, ElementTetP1, BilinearForm, asm
from skfem.models.poisson import laplace
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_consistency import diagnose


class StressConsistency(unittest.TestCase):
    def test_nonsolenoidal_zero_trace_and_volume_identity(self):
        mesh=MeshTet.init_tensor(np.linspace(0,4,9),np.linspace(0,2,5),np.linspace(0,2,5))
        lo=np.array([1.5,.5,.5]);hi=lo+1;c=mesh.p[:,mesh.t].mean(axis=1)
        mesh=mesh.remove_elements(np.nonzero(np.all((c>lo[:,None])&(c<hi[:,None]),axis=0))[0])
        def body(x):
            return np.all((x>=lo[:,None]-1e-12)&(x<=hi[:,None]+1e-12),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
        mesh=mesh.with_boundaries({'body':body,'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),
                                  'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],4)})
        ub,pb=Basis(mesh,ElementTetP2(),intorder=4),Basis(mesh,ElementTetP1(),intorder=4)
        u=np.random.default_rng(37).normal(size=(3,ub.N))*.01
        u[:,ub.get_dofs(['body','walls']).all()]=0
        p=.03+np.array([.003,.002,-.001])@pb.doflocs;mu=.1
        row=diagnose(mesh,ub,pb,u,p,mu,lo,hi)
        self.assertLess(max(abs(x) for x in row['normal_divergence_identity_error_n']),1e-12)
        self.assertGreater(np.linalg.norm(row['normal_load_n']),1e-5) # Nonzero error cannot pass vacuously.
        for lift in row['volume_lifts']:
            self.assertLess(abs(lift['integration_by_parts_identity_error_n']),1e-12)
            shell=lift['shell_m'];d=np.linalg.norm(np.maximum(np.maximum(lo[:,None]-ub.doflocs,ub.doflocs-hi[:,None]),0),axis=0)
            t=np.minimum(d/shell,1);eta=1-3*t*t+2*t*t*t
            @BilinearForm
            def pressure_gradient(q,v,w):return -q*v.grad[0]
            vector=-float(eta@(mu*asm(laplace,ub)@u[0]+asm(pressure_gradient,pb,ub)@p))
            # Independently assemble each component of div(u)*d_x eta.
            div_load=0
            for a in range(3):
                @BilinearForm
                def term(u,v,w):return u.grad[a]*v.grad[0]
                div_load-=mu*float(eta@(asm(term,ub)@u[a]))
            self.assertAlmostEqual(vector,lift['vector_laplacian_load_n'],places=12)
            self.assertAlmostEqual(div_load,lift['divergence_correction_n'],places=12)
        shifted=diagnose(mesh,ub,pb,u,p+7,mu,lo,hi)
        for a,b in zip(row['volume_lifts'],shifted['volume_lifts']):
            self.assertAlmostEqual(a['vector_laplacian_load_n'],b['vector_laplacian_load_n'],places=12)
            self.assertAlmostEqual(a['symmetric_stress_load_n'],b['symmetric_stress_load_n'],places=12)

if __name__=='__main__':unittest.main()
