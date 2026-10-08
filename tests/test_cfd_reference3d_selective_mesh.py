"""Independent geometry, input-integrity and original-action checks for outer refinement."""
import sys
import json
import tempfile
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cfd_reference3d_selective_mesh import selective_mesh
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_condensed import CondensedSystem,full_action
from cfd_reference3d_quartic_pair import assemble_quartic
ROOT=Path(__file__).resolve().parents[1]


class Selective(unittest.TestCase):
    def test_explicit_original_action_on_refined_anisotropic_macro_mesh(self):
        # Conforming bisection, original quartic volume assembly and exact local
        # reconstruction are independent implementations of the same FE action.
        macro=MeshTet.init_tensor([0.,3.3125],[0.,.0625],[0.,.0625]).refined(np.array([0]))
        mesh=alfeld_split(macro);system=CondensedSystem(mesh,.1,cache_cap_bytes=0)
        ub,pb,A,B=assemble_quartic(mesh,.1,128)
        rng=np.random.default_rng(803);z=rng.normal(size=system.shape[0]);rhs=np.zeros(3*ub.N+pb.N)
        u,p=system.reconstruct(z,rhs);actual=full_action(mesh,ub,pb,u,p,.1,128)
        explicit=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))]
        np.testing.assert_allclose(actual,explicit,atol=1e-9,rtol=1e-10)
        retained=actual[:3*ub.N].reshape(3,-1)[:,system.trace].ravel()
        np.testing.assert_allclose(retained,(system.matrix@z)[:3*system.nt],atol=1e-9,rtol=1e-10)


if __name__=='__main__':unittest.main()
