"""Independent coupled SPD controls for optional Float graph approximation."""
import sys,unittest
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix,triu
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference_test_support import library_path
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_pruned_graph import strength_data,compensated_input,THRESHOLDS
from cfd_reference3d_pruned_workspace import PrunedWorkspaceCholesky
from cfd_reference3d_mixed_workspace import numeric_stage_admission
from cfd_reference3d_shared_factor import storage_sha
LIB=library_path('build/c3d-pruned-graph/support-factor.dylib')

def fixture():
    n=5;rng=np.random.default_rng(997);A=np.zeros((3*n,3*n));weights=np.zeros(n)
    for i in range(n):
        for j in range(i+1,n):
            E=rng.normal(size=(3,3))*[0.,1e-5,.002,.08][(i+j)%4]
            A[3*i:3*i+3,3*j:3*j+3]=E;A[3*j:3*j+3,3*i:3*i+3]=E.T
            weights[i]+=np.linalg.norm(E);weights[j]+=np.linalg.norm(E)
    for i in range(n):
        B=rng.normal(size=(3,3));A[3*i:3*i+3,3*i:3*i+3]=B@B.T+(1+weights[i])*np.eye(3)
    order=np.arange(3*n).reshape(n,3).T.ravel();physical=A[np.ix_(order,order)]
    return A,physical,VectorTriangle(triu(csr_matrix(physical),format='csr'),LIB)

def dense_input(starts,rows,values):
    n=len(starts)-1;M=np.zeros((3*n,3*n));v=values.reshape(-1,3,3)
    for i in range(n):
        for k in range(starts[i],starts[i+1]):
            j=rows[k];M[3*i:3*i+3,3*j:3*j+3]=v[k]
            if i!=j:M[3*j:3*j+3,3*i:3*i+3]=v[k].T
    return M

def independent(A,tau):
    n=len(A)//3;M=A.copy();scales=np.array([np.linalg.norm(A[3*i:3*i+3,3*i:3*i+3]) for i in range(n)])
    for i in range(n):
        for j in range(i+1,n):
            E=A[3*i:3*i+3,3*j:3*j+3];w=np.linalg.norm(E)
            if w/np.sqrt(scales[i]*scales[j])<=tau:
                M[3*i:3*i+3,3*j:3*j+3]=0;M[3*j:3*j+3,3*i:3*i+3]=0
                M[3*i:3*i+3,3*i:3*i+3]+=w*np.eye(3);M[3*j:3*j+3,3*j:3*j+3]+=w*np.eye(3)
    return M

class Graph(unittest.TestCase):
    def test_all_thresholds_independent_coefficients_spd_and_dominance(self):
        A,P,v=fixture();digest=storage_sha(v.starts,v.rows,v.values);x=np.linspace(-1,2,len(P))
        for tau in THRESHOLDS:
            s,r,f,m=compensated_input(v,tau);M=independent(A,tau)
            np.testing.assert_array_equal(dense_input(s,r,f),M.astype(np.float32).astype(float))
            self.assertGreater(np.linalg.eigvalsh(M).min(),0);self.assertGreaterEqual(np.linalg.eigvalsh(M-A).min(),-1e-12)
            self.assertEqual(m['threshold'],tau);self.assertTrue(m['physical_input_preserved_bitwise'])
            np.testing.assert_allclose(v@x,P@x,rtol=1e-14,atol=1e-14)
        self.assertEqual(storage_sha(v.starts,v.rows,v.values),digest)

    def test_zero_threshold_exact_original_float_action(self):
        A,P,v=fixture();s,r,f,m=compensated_input(v,0.)
        np.testing.assert_array_equal(dense_input(s,r,f),A.astype(np.float32).astype(float));self.assertEqual(m['diagonal_compensation_sum'],0.)

    def test_float_solve_coordinates_owned_storage_and_cleanup(self):
        A,P,v=fixture();factor=PrunedWorkspaceCholesky(v,LIB,pressure_control=False,threshold=1e-2)
        s,r,f,_=compensated_input(v,1e-2);M=dense_input(s,r,f);order=np.arange(len(A)).reshape(5,3).T.ravel();rhs=np.linspace(-1,2,len(A))
        np.testing.assert_allclose(factor.solve(rhs),np.linalg.solve(M[np.ix_(order,order)],rhs),rtol=3e-6,atol=2e-7)
        self.assertTrue(factor.metadata['user_factor_storage_verified']);self.assertTrue(factor.input_unchanged());factor.close();factor.close();self.assertIsNone(factor._handle)

    def test_symbolic_stop_preserves_physical_arrays_and_cleans_handle(self):
        A,P,v=fixture();factor=PrunedWorkspaceCholesky.__new__(PrunedWorkspaceCholesky);captured={}
        class Stop(Exception):pass
        def stage(phase):
            if phase=='workspace_pressure_complete':captured.update(numeric_stage_admission(factor,1000));raise Stop()
        with self.assertRaises(Stop):PrunedWorkspaceCholesky.__init__(factor,v,LIB,action=lambda:v@np.ones(len(P)),stage_callback=stage,threshold=1e-3)
        self.assertIsNone(factor._handle);self.assertTrue(factor.input_unchanged());self.assertEqual(captured['basis_reservation_bytes'],1000)
        self.assertTrue(factor.pressure_record['action_preserved'])

    def test_invalid_scales_threshold_float_values_and_indefinite_factor_reject(self):
        A,P,v=fixture()
        with self.assertRaises(ValueError):compensated_input(v,.1)
        for scale in (-1.,1e40,1e-50):
            P=np.eye(3)*scale;v=VectorTriangle(triu(csr_matrix(P),format='csr'),LIB)
            with self.assertRaises(ValueError):compensated_input(v,0.)
        P=np.eye(6);P[0,1]=P[1,0]=3.;v=VectorTriangle(triu(csr_matrix(P),format='csr'),LIB)
        with self.assertRaises(ValueError):PrunedWorkspaceCholesky(v,LIB,pressure_control=False,threshold=0.)

    def test_strength_statistics_counts_and_input_tamper(self):
        A,P,v=fixture();norms,strength,m=strength_data(v)
        for t in m['thresholds']:
            self.assertEqual(t['retained_upper_blocks']+t['dropped_blocks'],len(v.rows))
        self.assertGreater(m['diagonal_min_eigenvalue'],0);self.assertTrue(np.all(np.isfinite(strength)))
        v.values[0]+=1
        with self.assertRaises(ValueError):strength_data(v)

if __name__=='__main__':unittest.main()
