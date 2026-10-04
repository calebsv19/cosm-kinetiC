"""Independent algebraic checks for mass scaling, SPD action and mixed solve."""
import sys
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse import hstack
from scipy.sparse.linalg import minres,LinearOperator
from skfem import MeshTet,Basis,ElementDG,ElementTetP2,asm
from skfem.models.poisson import mass
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_stokes_pair import assemble_pair,mixed_matrix
from cfd_reference3d_preconditioner import PressureMass,velocity_preconditioner,residual_diagnostics,macro_pressure_modes


class PreconditionerChecks(unittest.TestCase):
    def pair(self):
        mesh=alfeld_split(MeshTet.init_tensor(*[np.linspace(0,L,2) for L in (1.,1.5,2.)]))
        mesh=mesh.with_boundaries({'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],1.5)|np.isclose(x[2],0)|np.isclose(x[2],2.)})
        ub,pb,A,blocks=assemble_pair(mesh,.1)
        free=np.setdiff1d(np.arange(ub.N),ub.get_dofs('walls').all())
        return ub,pb,A,blocks,free

    def test_mass_against_independent_assembly(self):
        _,pb,*_=self.pair();M=asm(mass,pb);helper=PressureMass(pb,.1)
        x=np.random.default_rng(41).normal(size=pb.N)
        np.testing.assert_allclose(helper.apply(x),M@x,atol=2e-15)
        np.testing.assert_allclose(M@helper.solve(x),x,atol=2e-13)
        self.assertAlmostEqual(helper.norm(x)**2,x@(M@x),places=12)

    def test_factor_and_amg_are_symmetric_positive(self):
        _,_,A,_,free=self.pair();Af=A[free][:,free]
        rng=np.random.default_rng(42);x=rng.normal(size=len(free));y=rng.normal(size=len(free))
        for kind in ('amg','factor'):
            pc,_=velocity_preconditioner(Af,kind)
            self.assertGreater(float(x@pc(x)),0)
            self.assertAlmostEqual(float(x@pc(y)),float(y@pc(x)),places=11)
            if kind=='factor':np.testing.assert_allclose(Af@pc(x),x,atol=2e-13)

    def test_identical_mixed_operator_recovers_known_vector(self):
        _,pb,A,blocks,free=self.pair();K=mixed_matrix(A,blocks,free);before=K.copy()
        helper=PressureMass(pb,.1);nv=3*len(free)
        exact=np.random.default_rng(43).normal(size=K.shape[0]);rhs=K@exact
        results=[]
        for kind in ('amg','factor'):
            pc,_=velocity_preconditioner(A[free][:,free],kind)
            def action(x):return np.r_[np.concatenate([pc(v) for v in x[:nv].reshape(3,-1)]),helper.precondition(x[nv:])]
            result,info=minres(K,rhs,M=LinearOperator(K.shape,action),rtol=1e-14,maxiter=3000)
            self.assertEqual(info,0)
            self.assertLess(np.linalg.norm(K@result-rhs)/np.linalg.norm(rhs),1e-10)
            np.testing.assert_allclose(result,exact,atol=2e-8)
            diag=residual_diagnostics(K,result,rhs,nv,helper)
            self.assertAlmostEqual(diag['true_residual']**2,diag['momentum_relative_to_rhs']**2+diag['continuity_relative_to_rhs']**2,places=25)
            results.append(result)
        self.assertEqual((K-before).nnz,0)
        np.testing.assert_allclose(results[0],results[1],atol=2e-8)

    def test_natural_ends_do_not_have_constant_pressure_null(self):
        _,pb,A,blocks,free=self.pair();B=hstack([b[:,free] for b in blocks],format='csr')
        modes=macro_pressure_modes(B,A[free][:,free],pb,6,.1)
        self.assertEqual(modes['near_null_modes_below_1e_12'],0)
        self.assertGreater(modes['constant_pressure_gradient_norm'],1e-3)
        self.assertGreater(min(modes['smallest_eigenvalues']),1e-6)


if __name__=='__main__':unittest.main()
