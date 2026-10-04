import unittest,sys,gc,weakref,json
from pathlib import Path
from types import SimpleNamespace
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from test_cfd_reference3d_distributed_p3_cg8 import fixture,LOCAL
from cfd_reference3d_pressure_proxy_refine3 import PressureProxyRefine3Factor,work_reserve,diagnostic_reserve,fresh_admission
from cfd_reference3d_p3_cg8_scalar import triple
from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor
from cfd_reference3d_vector_storage import VectorTriangle
LIB=R/'build/c3d-p3-cg8-scalar/support/coarse.dylib'
class PressureRefine(unittest.TestCase):
 def test_independent_dense_symmetric_positive_polynomial(self):
  rng=np.random.default_rng(813);M=rng.normal(size=(12,12));A=M@M.T+np.eye(12);G=np.diag(1/np.diag(A))*6;expected=3*G-3*G@A@G+G@A@G@A@G;actual=np.column_stack([triple(lambda x:A@x,lambda x:G@x,x) for x in np.eye(12)]);np.testing.assert_allclose(actual,expected,rtol=1e-11,atol=1e-11);self.assertGreater(np.linalg.eigvalsh(actual).min(),0);np.testing.assert_allclose(actual,actual.T,atol=1e-11)
 def test_original_anisotropic_physical_model_and_unchanged_CG8(self):
  _,s=fixture();nv=len(s.retained_free)-s.nmacro;t=VectorTriangle(s.upper_matrix[:nv,:nv],LOCAL);kw=dict(pressure_control=False,coarse_library=LIB,Z=s.p3.Z,coarse_upper=s.p3.upper,coarse_metadata=s.p3.metadata);a=PressureProxyRefine3Factor(t,LOCAL,**kw);b=DistributedP3CG8ScalarPressureFactor(t,LOCAL,**kw);x,y=np.random.default_rng(73).normal(size=(2,nv));np.testing.assert_allclose(a.solve(x),b.solve(x),rtol=1e-8,atol=1e-8);ax,ay=a.pressure_solve(x),a.pressure_solve(y);np.testing.assert_allclose(x@ay,y@ax,rtol=1e-8,atol=1e-8);self.assertGreater(x@ax,0);self.assertTrue(a.input_unchanged());self.assertEqual(a.metadata['pressure_proxy_fixed_balanced_steps'],3);refs=[weakref.ref(a.linear_pressure),weakref.ref(a.balanced),weakref.ref(a.coarse_factor.starts)];a.close();b.close();del a;gc.collect();self.assertTrue(all(r() is None for r in refs))
 def test_extended_complete_work_budget_and_source_transform(self):
  fake=SimpleNamespace(n=60,rows=np.empty(100),coarse_factor=SimpleNamespace(storage=2234,numeric_workspace=1578),Z=SimpleNamespace(shape=(60,20)),pressure_record={'current_rss_after_bytes':1024});a=fresh_admission(fake,10,20,rss_reader=lambda:4096);self.assertEqual(a['estimated_numeric_stage_bytes'],4096+72*100+1024+2234+32*60+32*2**20+30+work_reserve(60,20));self.assertEqual(work_reserve(60,20),8*(48*60+24*20)+2*2**20)
  from cfd_reference3d_pressure_column_proxy import diagnostic_reserve as old
  self.assertEqual(diagnostic_reserve(60,20),old(60,20)+8*60*10);t=json.loads((R/'build/c3d-pressure-proxy-refine3/control-transform-control.json').read_text());s=(R/t['parent']).read_text()
  for x,y in t['literal_replacements']:self.assertIn(x,s);s=s.replace(x,y)
  self.assertEqual(s,(R/t['output']).read_text());self.assertEqual((R/'scripts/cfd_reference3d_pressure_proxy_refine3_probe.py').read_text(),(R/'scripts/cfd_reference3d_p3_cg8_scalar_probe.py').read_text().replace('from cfd_reference3d_p3_cg8_scalar_pressure import DistributedP3CG8ScalarPressureFactor','from cfd_reference3d_pressure_proxy_refine3 import PressureProxyRefine3Factor'))
if __name__=='__main__':unittest.main()
