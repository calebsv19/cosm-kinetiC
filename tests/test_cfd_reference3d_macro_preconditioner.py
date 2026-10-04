"""Symmetry, positivity, viscosity scaling and known mixed-solution recovery."""
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
from scipy.sparse.linalg import LinearOperator,minres
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_chunked import assemble_chunked,chunked_mass
from cfd_reference3d_stokes_pair import mixed_matrix
from cfd_reference3d_preconditioner import velocity_preconditioner
from cfd_reference3d_pressure_modes import macro_pressure_preconditioner,patch_pressure_preconditioner


def case(mu, factory=macro_pressure_preconditioner):
    parent=MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.]))
    m=alfeld_split(parent).with_boundaries({'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],1.5)|np.isclose(x[2],0)|np.isclose(x[2],2)})
    u,p,A,B=assemble_chunked(m,mu,8)
    free=np.setdiff1d(np.arange(u.N),u.get_dofs('walls').all())
    action,meta=factory(m,u,p,A,B,chunked_mass(p,mu),free,parent.nelements,mu)
    return u,p,A,B,free,action,meta


class MacroPreconditioner(unittest.TestCase):
    factory=staticmethod(macro_pressure_preconditioner)
    def test_spd_and_scaling(self):
        u,p,A,B,free,action,meta=case(.1,self.factory)
        dense=np.column_stack([action(e) for e in np.eye(p.N)])
        np.testing.assert_allclose(dense,dense.T,rtol=1e-10,atol=1e-9)
        self.assertGreater(np.linalg.eigvalsh(dense)[0],0)
        other=case(.2,self.factory)[5];x=np.random.default_rng(74).normal(size=p.N)
        np.testing.assert_allclose(other(x),2*action(x),rtol=1e-10,atol=1e-8)
        self.assertEqual(meta['macro_count'],6)

    def test_known_solution_recovery(self):
        u,p,A,B,free,pressure,meta=case(.1,self.factory)
        K=mixed_matrix(A,B,free);nv=3*len(free)
        velocity,_=velocity_preconditioner(A[free][:,free],'factor')
        def apply(x):return np.r_[np.concatenate([velocity(v) for v in x[:nv].reshape(3,-1)]),pressure(x[nv:])]
        exact=np.random.default_rng(75).normal(size=K.shape[0]);rhs=K@exact
        result,info=minres(K,rhs,M=LinearOperator(K.shape,apply),rtol=1e-14,maxiter=3000)
        self.assertEqual(info,0)
        self.assertLess(np.linalg.norm(K@result-rhs)/np.linalg.norm(rhs),1e-9)
        self.assertLess(np.linalg.norm(result-exact)/np.linalg.norm(exact),1e-7)


class PatchPreconditioner(MacroPreconditioner):
    factory=staticmethod(patch_pressure_preconditioner)


if __name__=='__main__':unittest.main()
