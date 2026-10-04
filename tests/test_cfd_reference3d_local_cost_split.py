import ctypes as ct,hashlib,json,sys,unittest
from pathlib import Path
from unittest.mock import patch
import numpy as np
from scipy.sparse import csr_matrix
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'));LIB=R/'build/c3d-local-cost-split/support/factor.dylib'
from cfd_reference3d_local_cost_split_action import profiled_inner8
from cfd_reference3d_packed_inner8_action import native_inner8
from cfd_reference3d_bounded_fill1 import BlockIC0
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_encoded_operator import EncodedTriangle
class Split(unittest.TestCase):
 def test_original_and_encoded_native_output_stats_and_zero_rhs(self):
  rng=np.random.default_rng(24571);M=rng.normal(size=(45,45));A=M@M.T+3*np.eye(45);t=VectorTriangle(csr_matrix(np.triu(A)),LIB)
  for owner in (t,EncodedTriangle(t,LIB)):
   f=BlockIC0(owner,LIB,False)
   for rhs in (rng.normal(size=f.n),np.zeros(f.n)):
    actual,stats=profiled_inner8(f,rhs);np.testing.assert_array_equal(actual,native_inner8(f,rhs));self.assertEqual(stats['packed_solve_calls'],stats['iterations']);self.assertEqual(stats['physical_action_calls'],stats['iterations']);self.assertGreaterEqual(stats['native_total_s'],stats['packed_solve_s']+stats['physical_action_s'])
   self.assertTrue(f.input_unchanged());f.close()
 def test_actual_anisotropic_fe_balanced_equivalence(self):
  from test_cfd_reference3d_distributed_p3_cg8 import fixture
  from cfd_reference3d_packed_inner8_pressure import NativeInner8PressureFactor
  _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LIB);f=NativeInner8PressureFactor(t,LIB,pressure_control=False,coarse_library=R/'build/c3d-p3-cg8-scalar/support/coarse.dylib',Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata)
  for rhs in np.random.default_rng(24572).normal(size=(2,nv)):
   expected=f.solve(rhs)
   with patch.object(f.balanced,'inverse',lambda x:profiled_inner8(f,x)[0]):actual=f.solve(rhs)
   np.testing.assert_array_equal(actual,expected)
  self.assertTrue(f.input_unchanged());f.close()
 def test_statistics_alias_short_work_and_closed_rejections(self):
  t=VectorTriangle(csr_matrix(np.eye(24)),LIB);f=BlockIC0(t,LIB,False);rhs=np.ones(f.n);profiled_inner8(f,rhs);fn=f.library.cfd_reference_inner_cg8_profile;lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);out=np.full(f.n,123.);work=np.empty(7*f.n);steps=ct.c_int();stats=np.empty(5)
  def call(s=stats,n=None):return fn(f._handle,t.nodes,t.starts.ctypes.data_as(lp),t.rows.ctypes.data_as(ip),t.values.ctypes.data_as(dp),None,None,f.perm.ctypes.data_as(ip),rhs.ctypes.data_as(dp),out.ctypes.data_as(dp),work.ctypes.data_as(dp),len(work) if n is None else n,0,ct.byref(steps),s.ctypes.data_as(dp))
  self.assertEqual(call(out),-501);np.testing.assert_array_equal(out,np.full(f.n,123.));self.assertEqual(call(work),-501);self.assertEqual(call(rhs),-501);self.assertEqual(call(t.values),-501);self.assertEqual(call(n=1),-501)
  with self.assertRaises(ValueError):profiled_inner8(f,np.full(f.n,np.nan))
  f.close()
  with self.assertRaises(ValueError):profiled_inner8(f,rhs)
 def test_exact_source_transform_and_additional_reservation(self):
  d=R/'build/c3d-local-cost-split';t=json.loads((d/'native-transform.json').read_text());self.assertEqual((R/t['output']).read_text(),(R/t['parent']).read_text()+t['appended_function']);self.assertEqual(hashlib.sha256((R/t['output']).read_bytes()).hexdigest(),t['output_sha256']);t=json.loads((d/'runner-transform.json').read_text());v=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:self.assertIn(a,v);v=v.replace(a,b)
  self.assertEqual(v,(R/t['output']).read_text())
if __name__=='__main__':unittest.main()
