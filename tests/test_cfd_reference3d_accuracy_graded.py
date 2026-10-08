import json,sys,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from test_cfd_reference3d_bounded_condensed import fixture
import cfd_reference3d_accuracy_graded_budget as b
from cfd_reference3d_triangle_condensed import TriangleCondensedSystem as Original
from cfd_reference3d_accuracy_graded_triangle import TriangleCondensedSystem as Graded
from cfd_reference3d_accuracy_graded_condensed import full_action
from cfd_reference3d_condensed import full_action as old_action
from cfd_reference3d_accuracy_graded_storage import decision
from cfd_reference3d_size_selected import decision as old_decision
from cfd_reference3d_accuracy_graded_mesh import build,translated_inner_keys
class GradedAccuracy(unittest.TestCase):
 def row(self):return dict(info=0,target=1e-10,final_residual={'true_residual':9e-12},flux_error=1e-12,volume_divergence_max_s_inv=1e-12,physical_energy_imbalance=.01,physical_dissipation_w=1.,tetrahedra=100608,iterations=100,peak_rss_bytes=6000*2**20,wall_s=900.)
 def test_strict_physical_gates_and_resource_boundaries(self):
  self.assertEqual(b.numerical_failure_reasons(self.row()),[])
  for field,value,reason in [('tetrahedra',120004,'resources'),('peak_rss_bytes',b.RSS_CAP,'resources'),('wall_s',1800.,'resources'),('flux_error',1e-7,'flux'),('volume_divergence_max_s_inv',1e-7,'volume_divergence'),('physical_energy_imbalance',.04,'energy')]:
   r=self.row();r[field]=value;self.assertIn(reason,b.numerical_failure_reasons(r))
  r=self.row();r['final_residual']['true_residual']=1e-9;self.assertIn('linear_residual',b.numerical_failure_reasons(r))
  with self.assertRaises(b.PhaseResourceStopped):b.enforce_phase('test',b.RSS_CAP,100.)
 def test_atomic_postserialization_failure_discards_pending(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'field.npz';r=self.row()
   with patch.object(b.resource,'getrusage',return_value=SimpleNamespace(ru_maxrss=6000*2**20)),patch.object(b.time,'monotonic',return_value=1801.):self.assertFalse(b.publish_snapshot(p,{'fixture':np.arange(3)},r,0.))
   self.assertFalse(p.exists());self.assertFalse(p.with_name(p.name+'.pending.npz').exists())
 def test_original_algebra_nonzero_eliminated_load_and_full_action_unchanged(self):
  m=fixture(True);old=Original(m,.1,fixed_boundaries=('walls',),assembly_batch=7);new=Graded(m,.1,fixed_boundaries=('walls',),assembly_batch=7)
  for a,c in zip((old.upper_matrix.data,old.upper_matrix.indices,old.upper_matrix.indptr),(new.upper_matrix.data,new.upper_matrix.indices,new.upper_matrix.indptr)):np.testing.assert_array_equal(a,c)
  rng=np.random.default_rng(7753);rhs=rng.normal(size=3*new.ub.N+new.pb.N)*.01;z=rng.normal(size=new.shape[0]);np.testing.assert_array_equal(old.reduce_rhs(rhs),new.reduce_rhs(rhs));u,p=new.reconstruct(z,rhs);uo,po=old.reconstruct(z,rhs);np.testing.assert_array_equal(u,uo);np.testing.assert_array_equal(p,po);np.testing.assert_array_equal(full_action(m,new.ub,new.pb,u,p,.1,128),old_action(m,old.ub,old.pb,u,p,.1,128))
 def test_fresh_matched_meshes_and_first_layer(self):
  meshes={}
  for L in (4.,8.):
   bundle,_=build(L);meshes[L]=bundle;m,lo,hi,axes,n=bundle;self.assertLessEqual(m.nelements,120000)
   for i in (1,2):self.assertAlmostEqual(lo[i]-axes[i][axes[i]<lo[i]-1e-12][-1],.03125)
  self.assertEqual(translated_inner_keys(meshes[4.]),translated_inner_keys(meshes[8.]))
 def test_coefficient_limit_is_explicit_and_bounded(self):
  with self.assertRaises(ValueError):old_decision(50000001)
  self.assertEqual(decision(120000000)['coefficient_count'],120000000)
  with self.assertRaises(ValueError):decision(120000001)
if __name__=='__main__':unittest.main()
