import unittest,sys,gc,weakref,json
from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace
import numpy as np
from scipy.sparse import csr_matrix,triu,diags
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from test_cfd_reference3d_coarse_velocity import system_fixture
from cfd_reference3d_distributed_p3_condensed import DistributedP3CondensedSystem
from cfd_reference3d_distributed_p3_cg8 import RefinedFloatCoarse,BalancedSparse,DistributedP3CG8Factor as DistributedP3Factor,fresh_admission,work_reserve,triple,inner_cg8
from cfd_reference3d_bounded_fill1 import BlockIC0
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_triangle import SymmetricTriangle
from cfd_reference3d_condensed import full_action
from cfd_reference3d_quartic_pair import assemble_quartic
LOCAL=R/'build/c3d-bounded-fill1/support/factor.dylib';COARSE=R/'build/c3d-distributed-p3/support/coarse.dylib'
def fixture():
 old=system_fixture();new=DistributedP3CondensedSystem(old.mesh,.1,fixed_boundaries=('walls',),assembly_batch=7,pressure_control=False)
 return old,new
def dense_fixture():
 rng=np.random.default_rng(5429);M=rng.normal(size=(24,24));A=M@M.T+10*np.eye(24);Z=csr_matrix(rng.normal(size=(24,4)));upper=triu(csr_matrix(Z.T@A@Z),format='csr');return rng,A,Z,upper
class Distributed(unittest.TestCase):
 def test_original_physical_bits_local_vs_global_galerkin_and_boundaries(self):
  old,s=fixture()
  for a,b in ((old.upper_matrix.indptr,s.upper_matrix.indptr),(old.upper_matrix.indices,s.upper_matrix.indices),(old.upper_matrix.data,s.upper_matrix.data)):np.testing.assert_array_equal(a,b)
  nv=len(s.retained_free)-s.nmacro;U=old.upper_matrix[:nv,:nv];A=U+U.T-diags(U.diagonal());Z=s.p3.Z;expected=(Z.T@A@Z).toarray();C=s.p3.upper;actual=(C+C.T-diags(C.diagonal())).toarray();np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-10)
  self.assertTrue(s.p3.interpolation['essential_boundary_vanishing_verified']);self.assertTrue(s.p3.interpolation['essential_projection_injective']);self.assertLessEqual(s.p3.metadata['maximum_local_columns'],60);self.assertLessEqual(s.p3.metadata['maximum_local_temporary_bytes'],2*2**20);self.assertLessEqual(s.p3.metadata['maximum_projected_sparse_construction_bytes'],256*2**20)
 def test_original_nonzero_load_pressure_reconstruction_full_action(self):
  old,s=fixture();g=np.random.default_rng(5430);rhs=g.normal(size=3*s.ub.N+s.pb.N)*.01;z=g.normal(size=s.shape[0]);np.testing.assert_array_equal(old.reduce_rhs(rhs),s.reduce_rhs(rhs));u,p=s.reconstruct(z,rhs);ou,op=old.reconstruct(z,rhs);np.testing.assert_array_equal(u,ou);np.testing.assert_array_equal(p,op)
  ub,pb,A,B=assemble_quartic(s.mesh,.1,128);expected=np.r_[np.concatenate([A@u[a]+B[a].T@p for a in range(3)]),sum(B[a]@u[a] for a in range(3))];np.testing.assert_allclose(full_action(s.mesh,ub,pb,u,p,.1,128),expected,rtol=1e-10,atol=1e-9)
 def test_dense_balanced_formula_spd_coarse_action(self):
  rng,A,Z,C=dense_fixture();velocity=SymmetricTriangle(triu(csr_matrix(A),format='csr'));coarse=RefinedFloatCoarse(C,COARSE);G=np.diag(1/np.diag(A));b=BalancedSparse(velocity,Z,coarse,lambda x:G@x);P=Z.toarray();E=P@np.linalg.solve(P.T@A@P,P.T);expected=E+(np.eye(24)-E@A)@G@(np.eye(24)-A@E);actual=np.column_stack([b.solve(x) for x in np.eye(24)]);np.testing.assert_allclose(actual,expected,rtol=1e-10,atol=1e-10);np.testing.assert_allclose(actual,actual.T,atol=1e-10);self.assertGreater(np.linalg.eigvalsh(actual).min(),0);np.testing.assert_allclose(actual@A@P,P,rtol=1e-10,atol=1e-10);coarse.close()
 def test_actual_both_factors_dense_and_anisotropic_original_action(self):
  rng,A,Z,C=dense_fixture();t=VectorTriangle(triu(csr_matrix(A),format='csr'),LOCAL);f=DistributedP3Factor(t,LOCAL,pressure_control=False,coarse_library=COARSE,Z=Z,coarse_upper=C);P=Z.toarray();E=P@np.linalg.solve(P.T@A@P,P.T);actual=np.column_stack([f.solve(x) for x in np.eye(24)]);np.testing.assert_allclose(actual,np.linalg.inv(A),atol=1e-10,rtol=1e-10);self.assertTrue(f.input_unchanged());f.close()
  old,s=fixture();nv=len(s.retained_free)-s.nmacro;triangle=VectorTriangle(s.upper_matrix[:nv,:nv],LOCAL);f=DistributedP3Factor(triangle,LOCAL,pressure_control=False,coarse_library=COARSE,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata);x,y=rng.normal(size=(2,nv));fx,fy=f.solve(x),f.solve(y);np.testing.assert_allclose(f.solve(2*x),2*fx,rtol=2e-8,atol=1e-8);self.assertGreater(x@fx,0);self.assertGreater(y@fy,0);self.assertTrue(f.input_unchanged());self.assertEqual(f.metadata['coarse_factor']['value_dtype'],'float32');f.close()
 def test_combined_admission_and_construction_rejection(self):
  fake=SimpleNamespace(n=60,rows=np.empty(100),coarse_factor=SimpleNamespace(storage=2234,numeric_workspace=1578),Z=SimpleNamespace(shape=(60,20)),pressure_record={'current_rss_after_bytes':1024});a=fresh_admission(fake,10,20,lambda:4096);self.assertTrue(a['numeric_stage_admitted']);self.assertEqual(a['estimated_numeric_stage_bytes'],4096+72*100+1024+2234+32*60+32*2**20+30+work_reserve(60,20))
  with self.assertRaises(ValueError):work_reserve(0,1)
  from cfd_reference3d_distributed_p3_assembly import P3Collector
  from cfd_reference3d_domain_budget import PhaseResourceStopped
  c=P3Collector.__new__(P3Collector);c.levels=[];c.rr=np.empty(1);c.cc=np.empty(1);c.values=np.empty(1);c.nc=10;c.max_live=0;c.pressure_control=False
  with self.assertRaises(PhaseResourceStopped):c.guard(new_nnz=256*2**20)
 def test_partial_cleanup_repeated_owners_and_invalid_coarse_input(self):
  rng,A,Z,C=dense_fixture();t=VectorTriangle(triu(csr_matrix(A),format='csr'),LOCAL);closed=[];original=RefinedFloatCoarse.close
  def close(obj):had=bool(obj._handle);original(obj);closed.append((had,obj._handle is None))
  def stop(phase):
   if phase=='workspace_pressure_complete':raise ValueError('numeric stage withheld')
  with patch.object(RefinedFloatCoarse,'close',close):
   with patch('cfd_reference3d_bounded_fill1.release_free_pages',return_value={'current_rss_after_bytes':1024}):
    with self.assertRaises(ValueError):DistributedP3Factor(t,LOCAL,coarse_library=COARSE,Z=Z,coarse_upper=C,stage_callback=stop)
  self.assertIn((True,True),closed)
  for i in range(20):
   f=DistributedP3Factor(t,LOCAL,pressure_control=False,coarse_library=COARSE,Z=Z,coarse_upper=C);refs=[weakref.ref(a) for a in (f.starts,f.rows,f.perm,f.inverse_permutation,f.coarse_factor.starts)];f.close();f.close();del f;gc.collect();self.assertTrue(all(r() is None for r in refs))
  bad=C.copy();bad.data[0]=np.nan
  with self.assertRaises(ValueError):RefinedFloatCoarse(bad,COARSE)
  coarse=RefinedFloatCoarse(C,COARSE);coarse.close()
  with self.assertRaises(ValueError):coarse.solve(np.ones(C.shape[0]))
 def test_triple_model_spd_and_Float_original_coarse_accuracy(self):
  rng=np.random.default_rng(99);M=rng.normal(size=(12,12));A=M@M.T+np.eye(12);G=np.diag(1/np.diag(A))*6
  expected=3*G-3*G@A@G+G@A@G@A@G;actual=np.column_stack([triple(lambda u:A@u,lambda x:G@x,x) for x in np.eye(12)]);np.testing.assert_allclose(actual,expected,rtol=1e-11,atol=1e-11);self.assertGreater(np.linalg.eigvalsh(actual).min(),0);self.assertGreater(np.linalg.eigvalsh(np.sqrt(G)@A@np.sqrt(G)).max(),2)
  C=triu(csr_matrix(A),format='csr');coarse=RefinedFloatCoarse(C,COARSE);x,y=rng.normal(size=(2,12));fx,fy=coarse.solve(x),coarse.solve(y);np.testing.assert_allclose(A@fx,x,rtol=1e-10,atol=1e-10);np.testing.assert_allclose(coarse.solve(x+2*y),fx+2*fy,rtol=1e-10,atol=1e-10);np.testing.assert_allclose(x@fy,y@fx,rtol=1e-10,atol=1e-10);self.assertTrue(coarse.input_unchanged());self.assertEqual(coarse.metadata['fixed_residual_steps'],3);coarse.close()
 def test_inner_CG8_independent_transformed_scipy_and_guards(self):
  from scipy.sparse.linalg import cg
  rng=np.random.default_rng(812);Q,_=np.linalg.qr(rng.normal(size=(80,80)));A=(Q*np.geomspace(1,1000,80))@Q.T;g=1/np.diag(A);half=np.sqrt(g);rhs=rng.normal(size=80);counts=[]
  expected,info=cg(half[:,None]*A*half[None,:],half*rhs,rtol=0,atol=0,maxiter=8,callback=lambda x:counts.append(1));actual=inner_cg8(lambda x:A@x,lambda x:g*x,rhs);np.testing.assert_allclose(actual,half*expected,atol=1e-11,rtol=1e-11);self.assertEqual(len(counts),8);self.assertEqual(info,8);self.assertGreater(rhs@actual,0);np.testing.assert_allclose(inner_cg8(lambda x:A@x,lambda x:g*x,3*rhs),3*actual,rtol=1e-10,atol=1e-10);np.testing.assert_array_equal(inner_cg8(lambda x:A@x,lambda x:g*x,np.zeros(80)),np.zeros(80))
  with self.assertRaises(ValueError):inner_cg8(lambda x:-x,lambda x:x,rhs)
  with self.assertRaises(ValueError):inner_cg8(lambda x:x,lambda x:-x,rhs)
  with self.assertRaises(ValueError):inner_cg8(lambda x:x,lambda x:x,np.full(80,np.nan))
 def test_exact_factor_and_probe_transform(self):
  t=json.loads((R/'build/c3d-distributed-p3-cg8/factor-transform-control.json').read_text());s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:self.assertIn(a,s);s=s.replace(a,b)
  self.assertEqual(s,(R/t['output']).read_text());self.assertEqual((R/'scripts/cfd_reference3d_distributed_p3_cg8_probe.py').read_text(),(R/'scripts/cfd_reference3d_distributed_p3_probe.py').read_text().replace('from cfd_reference3d_distributed_p3 import DistributedP3Factor','from cfd_reference3d_distributed_p3_cg8 import DistributedP3CG8Factor'))
 def test_source_transform_and_Float_library_identity(self):
  t=json.loads((R/'build/c3d-distributed-p3/assembly-transform-control.json').read_text());s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:self.assertIn(a,s);s=s.replace(a,b)
  self.assertEqual(s,(R/t['output']).read_text());self.assertEqual((R/'scripts/cfd_reference3d_distributed_p3_coarse.c').read_bytes(),(R/'scripts/cfd_reference3d_encoded_storage.c').read_bytes())
if __name__=='__main__':unittest.main()
