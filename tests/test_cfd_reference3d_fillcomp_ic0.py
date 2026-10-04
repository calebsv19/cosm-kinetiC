import unittest,sys
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_fillcomp_ic0 import BlockIC0,BalancedVelocity,basis,velocity_reserve,fresh_admission
from cfd_reference3d_shared_factor import BlockTriangle
from test_cfd_reference3d_coarse_velocity import system_fixture
from cfd_reference3d_condensed import full_action
from cfd_reference3d_quartic_pair import assemble_quartic
LIB=R/'build/c3d-fillcomp-ic0/support/factor.dylib'
def triangle(A):return VectorTriangle(csr_matrix(np.triu(A)),LIB)
def dense_lower(f):
    n=f.owner.nodes;L=np.zeros((3*n,3*n));v=f.lower_copy().reshape(-1,3,3)
    for j in range(n):
        for k in range(f.starts[j],f.starts[j+1]):L[3*f.rows[k]:3*f.rows[k]+3,3*j:3*j+3]=v[k]
    p=np.arange(3*n).reshape(n,3).T.ravel()
    return L,p
class IC(unittest.TestCase):
 def test_dense_exact_and_coordinates(self):
    g=np.random.default_rng(771);M=g.normal(size=(15,15));A=M@M.T+5*np.eye(15);t=triangle(A);f=BlockIC0(t,LIB,False)
    L,p=dense_lower(f);model=(L@L.T)[np.ix_(p,p)]
    # Predictor precision is explicit: compare against original rounded block matrix.
    expected=np.zeros_like(A)
    for j in range(t.nodes):
     for k in range(t.starts[j],t.starts[j+1]):
      i=t.rows[k];b=t.values.reshape(-1,3,3)[k].astype(np.float32).astype(float)
      ix=np.arange(3)*t.nodes+j;iy=np.arange(3)*t.nodes+i;expected[np.ix_(ix,iy)]=b;expected[np.ix_(iy,ix)]=b.T
    np.testing.assert_allclose(model,expected,atol=1e-12);x=g.normal(size=15);np.testing.assert_allclose(f.solve(x),np.linalg.solve(expected,x),atol=1e-12)
    self.assertTrue(f.input_unchanged());self.assertEqual(f.metadata['shifted_pivots'],0);f.close()
    with self.assertRaises(ValueError):f.solve(x)
 def test_sparse_model_positive_compensation_and_original_preservation(self):
    # Sparse cycle with strong couplings: this indefinite predictor explicitly tests PC-only compensation.
    A=np.kron(np.array([[1.,.9,0.,.9],[.9,1.,.9,0.],[0.,.9,1.,.9],[.9,0.,.9,1.]]),np.eye(3));p=np.arange(12).reshape(4,3).T.ravel();A=A[np.ix_(p,p)]
    t=triangle(A);f=BlockIC0(t,LIB,False);L,p=dense_lower(f);model=(L@L.T)[np.ix_(p,p)];x=np.arange(12.)
    self.assertGreater(f.metadata['fill_compensated_pairs'],0);self.assertGreater(np.linalg.eigvalsh(model).min(),0)
    np.testing.assert_allclose(f.solve(x),np.linalg.solve(model,x),rtol=1e-10);self.assertTrue(t.input_unchanged());f.close()
 def test_sparse_spd_model_error_positive_and_bounded_inverse(self):
    nodes=7;H=np.eye(nodes)*2
    for i in range(nodes):H[i,(i+1)%nodes]=H[(i+1)%nodes,i]=.9
    A=np.kron(H,np.array([[1.,.1,.05],[.1,1.2,.08],[.05,.08,.8]]));p=np.arange(3*nodes).reshape(nodes,3).T.ravel();A=A[np.ix_(p,p)];t=triangle(A);f=BlockIC0(t,LIB,False);l,p=dense_lower(f);model=(l@l.T)[np.ix_(p,p)];self.assertGreater(f.metadata['fill_compensated_pairs'],0);self.assertEqual(f.metadata['shifted_pivots'],0)
    rounded=np.zeros_like(A)
    for j in range(t.nodes):
     for k in range(t.starts[j],t.starts[j+1]):
      i=t.rows[k];b=t.values.reshape(-1,3,3)[k].astype(np.float32).astype(float);ix=np.arange(3)*nodes+j;iy=np.arange(3)*nodes+i;rounded[np.ix_(ix,iy)]=b;rounded[np.ix_(iy,ix)]=b.T
    self.assertGreaterEqual(np.linalg.eigvalsh(model-rounded).min(),-1e-10);self.assertLess(np.linalg.norm(np.linalg.inv(model),2),10*np.linalg.norm(np.linalg.inv(rounded),2));self.assertTrue(f.input_unchanged());f.close()
 def test_balanced_formula_reproduction_complement_and_symmetry(self):
    g=np.random.default_rng(772);r=g.normal(size=(36,36));A=r.T@r+np.eye(36);Z=g.normal(size=(36,5));G=np.diag(1/np.diag(A));b=BalancedVelocity(Z,A@Z,lambda x:G@x)
    E=Z@np.linalg.solve(Z.T@A@Z,Z.T);expected=E+(np.eye(36)-E@A)@G@(np.eye(36)-A@E)
    actual=np.column_stack([b.solve(x) for x in np.eye(36)]);np.testing.assert_allclose(actual,expected,atol=1e-12);np.testing.assert_allclose(actual,actual.T,atol=1e-12)
    self.assertGreater(np.linalg.eigvalsh(actual).min(),0);np.testing.assert_allclose(actual@A@Z,Z,atol=1e-11)
    with self.assertRaises(ValueError):BalancedVelocity(np.column_stack([Z,Z[:,0]]),np.column_stack([A@Z,A@Z[:,0]]),lambda x:G@x)
 def test_anisotropic_nonzero_load_full_equations(self):
    s=system_fixture();C=BlockTriangle(s.upper_matrix,len(s.retained_free)-s.nmacro);t=VectorTriangle(C.velocity.upper,LIB);g=np.random.default_rng(773);rhs=g.normal(size=3*s.ub.N+s.pb.N)*.01;z=np.zeros(s.shape[0]);z[s.retained_free]=g.normal(size=C.shape[0]);reduced=s.reduce_rhs(rhs);u,p=s.reconstruct(z,rhs);pc=BlockIC0(t,LIB,False)
    x,y=g.normal(size=(2,t.shape[0]));bx,by=pc.solve(x),pc.solve(y);self.assertGreater(x@bx,0);np.testing.assert_allclose(x@by,y@bx,rtol=1e-12,atol=1e-8);pc.close()
    np.testing.assert_array_equal(s.reduce_rhs(rhs),reduced);uu,pp=s.reconstruct(z,rhs);np.testing.assert_array_equal(u,uu);np.testing.assert_array_equal(p,pp)
    ub,pb,A,B=assemble_quartic(s.mesh,.1,128);expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))];np.testing.assert_allclose(full_action(s.mesh,ub,pb,u,p,.1,128),expected,atol=1e-9,rtol=1e-10)
 def test_admission_invalid_rank_and_physical_prefix(self):
    self.assertTrue((R/'scripts/cfd_reference3d_fillcomp_ic0.c').read_bytes().startswith((R/'scripts/cfd_reference3d_encoded_storage.c').read_bytes()))
    t=triangle(np.eye(36));f=BlockIC0(t,LIB,False);f.pressure_record={'current_rss_after_bytes':1024};a=fresh_admission(f,100,200,lambda:2048)
    self.assertEqual(a['estimated_numeric_stage_bytes'],2048+72*len(f.rows)+1024+8*36+32*2**20+300+velocity_reserve(36));self.assertTrue(a['numeric_stage_admitted'])
    with self.assertRaises(ValueError):f.solve(np.full(36,np.nan))
    with self.assertRaises(ValueError):basis(np.zeros((20,3)),4.,np.zeros(3),np.ones(3))
    f.close();f.close()
if __name__=='__main__':unittest.main()
