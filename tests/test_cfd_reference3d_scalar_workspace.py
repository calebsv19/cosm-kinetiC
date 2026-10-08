"""Scalar graph retains rounded coefficients, physical action and public factor lifecycle."""
import sys,unittest
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix,csc_matrix,triu
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_scalar_workspace import scalar_input,ScalarWorkspaceCholesky
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_mixed_workspace import numeric_stage_admission
LIB=library_path('build/c3d-second-normal/scalar-support-factor.dylib')

def fixture():
    rng=np.random.default_rng(41216);M=rng.normal(size=(12,12));A=M@M.T+12*np.eye(12)
    # Two exact zeros exercise structural omission without dropping a coefficient.
    A[0,7]=A[7,0]=0.;A[2,11]=A[11,2]=0.
    return A,VectorTriangle(triu(csr_matrix(A),format='csr'),LIB)

class ScalarGraph(unittest.TestCase):
    def test_identical_float_triangle_and_independent_action(self):
        A,v=fixture();digest=storage_sha(v.starts,v.rows,v.values);s,r,f,m=scalar_input(v)
        L=csc_matrix((f,r,s),shape=(12,12)).toarray();rounded=L+L.T-np.diag(np.diag(L));order=np.arange(12).reshape(3,4).T.ravel()
        np.testing.assert_array_equal(rounded,A[np.ix_(order,order)].astype(np.float32))
        self.assertGreater(m['exact_zero_coefficients_omitted'],0)
        self.assertEqual(m['owned_input_allocation_bytes'],s.nbytes+2*4*m['capacity_scalar_entries'])
        x=np.linspace(-1,2,12);np.testing.assert_allclose(v@x,A@x,rtol=1e-14,atol=1e-14)
        self.assertEqual(storage_sha(v.starts,v.rows,v.values),digest)
        for i in range(12):self.assertTrue(np.all(np.diff(r[s[i]:s[i+1]])>0));self.assertTrue(np.all(r[s[i]:s[i+1]]>=i))

    def test_coupled_float_solve_coordinates_ownership_and_cleanup(self):
        A,v=fixture();factor=ScalarWorkspaceCholesky(v,LIB,pressure_control=False)
        rhs=np.linspace(-1,2,12);actual=factor.solve(rhs)
        np.testing.assert_allclose(actual,np.linalg.solve(A.astype(np.float32).astype(float),rhs),rtol=2e-6,atol=2e-7)
        self.assertTrue(factor.input_unchanged());self.assertTrue(factor.metadata['user_factor_storage_verified'])
        self.assertEqual(factor.metadata['block_size'],1)
        with self.assertRaises(ValueError):factor.solve(np.full(12,np.nan))
        factor.close();factor.close();self.assertIsNone(factor._handle)

    def test_symbolic_stop_destroys_handle_before_any_numeric_factor(self):
        A,v=fixture();factor=ScalarWorkspaceCholesky.__new__(ScalarWorkspaceCholesky);captured={}
        class Stop(Exception):pass
        def stage(phase):
            if phase=='workspace_pressure_complete':
                captured.update(numeric_stage_admission(factor,1234));raise Stop()
        with self.assertRaises(Stop):ScalarWorkspaceCholesky.__init__(factor,v,LIB,action=lambda:v@np.ones(12),stage_callback=stage)
        self.assertIsNone(factor._handle);self.assertTrue(factor.input_unchanged())
        self.assertEqual(captured['basis_reservation_bytes'],1234)
        self.assertTrue(factor.pressure_record['action_preserved'])

    def test_nonzero_underflow_overflow_and_input_tamper_rejected(self):
        for value in (1e40,1e-50):
            A=np.eye(3);A[0,1]=A[1,0]=value;v=VectorTriangle(triu(csr_matrix(A),format='csr'),LIB)
            with self.assertRaises(ValueError):scalar_input(v)
        _,v=fixture();v.values[0]+=1
        with self.assertRaises(ValueError):scalar_input(v)

if __name__=='__main__':unittest.main()
