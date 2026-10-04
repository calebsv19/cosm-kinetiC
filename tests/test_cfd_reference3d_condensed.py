"""Exact local algebra and independently reconstructed full quadrature action."""
import sys
import unittest
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import spsolve
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_quartic_pair import assemble_quartic
from cfd_reference3d_stokes_pair import mixed_matrix
from cfd_reference3d_condensed import CondensedSystem,full_action


class Condensation(unittest.TestCase):
    def test_independent_full_action_and_nonzero_load_elimination(self):
        mesh=alfeld_split(MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.])))
        system=CondensedSystem(mesh,.1);ub,pb,A,B=assemble_quartic(mesh,.1,64);K=mixed_matrix(A,B,np.arange(ub.N))
        rng=np.random.default_rng(199);u=rng.normal(size=(3,ub.N));p=rng.normal(size=pb.N)
        np.testing.assert_allclose(full_action(mesh,ub,pb,u,p,.1),K@np.r_[u.ravel(),p],rtol=1e-11,atol=1e-11)
        rhs=rng.normal(size=K.shape[0]);z=rng.normal(size=system.shape[0]);u,p=system.reconstruct(z,rhs)
        residual=K@np.r_[u.ravel(),p]-rhs;expected=system.matrix@z-system.reduce_rhs(rhs)
        retained=np.r_[residual[:3*ub.N].reshape(3,-1)[:,system.trace].ravel(),np.array([residual[3*ub.N:][ids].sum() for _,_,ids,_ in system.records])]
        np.testing.assert_allclose(retained,expected,rtol=1e-10,atol=1e-10)
        np.testing.assert_allclose(system.retained_field(u,p),z,atol=1e-11)
        for _,indices,pressure,_ in system.records:
            self.assertLess(np.max(np.abs(residual[:3*ub.N].reshape(3,-1)[:,indices[system.ref.bubble]])),1e-10)
            self.assertLess(np.max(np.abs(system.ref.Q.T@residual[3*ub.N:][pressure])),1e-10)
    def test_cacheless_reconstruction_preserves_full_field(self):
        mesh=alfeld_split(MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.])))
        cached=CondensedSystem(mesh,.1);uncached=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        self.assertEqual(uncached.metadata['reconstruction_cache_bytes'],0)
        import json
        self.assertEqual(json.loads(json.dumps(cached.metadata)),cached.metadata)
        np.testing.assert_allclose(cached.matrix.toarray(),uncached.matrix.toarray(),atol=1e-13)
        rhs=np.random.default_rng(202).normal(size=3*cached.ub.N+cached.pb.N)
        z=np.random.default_rng(203).normal(size=cached.shape[0])
        for a,b in zip(cached.reconstruct(z,rhs),uncached.reconstruct(z,rhs)):
            np.testing.assert_allclose(a,b,atol=1e-12)

    def test_global_solve_and_full_residual_with_nonzero_boundary(self):
        mesh=alfeld_split(MeshTet.init_tensor(np.array([0.,1.]),np.array([0.,1.5]),np.array([0.,2.])))
        system=CondensedSystem(mesh,.1);rng=np.random.default_rng(200)
        u=np.array([system.ub.doflocs[1],-system.ub.doflocs[0],np.zeros(system.ub.N)])*.02
        p=.03+.01*system.pb.doflocs[0]
        rhs=full_action(mesh,system.ub,system.pb,u,p,.1)
        exact=system.retained_field(u,p)
        boundary=system.ub.get_dofs().all();fixed=np.r_[np.concatenate([system.trace_inverse[boundary]+a*system.nt for a in range(3)]),3*system.nt]
        free=np.setdiff1d(np.arange(system.shape[0]),fixed);reduced=system.reduce_rhs(rhs)
        z=exact.copy();z[free]=spsolve(system.matrix[free][:,free],reduced[free]-system.matrix[free][:,fixed]@exact[fixed])
        actualu,actualp=system.reconstruct(z,rhs)
        np.testing.assert_allclose(actualu,u,atol=1e-11);np.testing.assert_allclose(actualp,p,atol=1e-10)
        self.assertLess(np.max(np.abs(full_action(mesh,system.ub,system.pb,actualu,actualp,.1)-rhs)),1e-11)


if __name__=='__main__':unittest.main()
