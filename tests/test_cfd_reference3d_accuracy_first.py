import hashlib,json,sys,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
import cfd_reference3d_accuracy_budget as b
import cfd_reference3d_accuracy_floor as w
from cfd_reference3d_graded_probe import numerical_failure_reasons as old_checks
class Accuracy(unittest.TestCase):
 def row(self):return dict(info=0,target=1e-10,final_residual={'true_residual':9e-12},flux_error=1e-12,volume_divergence_max_s_inv=1e-12,physical_energy_imbalance=.01,physical_dissipation_w=1.,tetrahedra=43008,iterations=100,peak_rss_bytes=2200*2**20,wall_s=250.)
 def test_explicit_resource_change_keeps_and_strengthens_physical_gates(self):
  r=self.row();self.assertEqual(old_checks(r),['resources']);self.assertEqual(b.numerical_failure_reasons(r),[])
  for field,value,reason in [('flux_error',1e-7,'flux'),('volume_divergence_max_s_inv',1e-7,'volume_divergence'),('physical_energy_imbalance',.04,'energy'),('peak_rss_bytes',b.RSS_CAP,'resources'),('wall_s',b.WALL_CAP,'resources')]:
   q=self.row();q[field]=value;self.assertIn(reason,b.numerical_failure_reasons(q))
  r['final_residual']['true_residual']=1e-9;self.assertIn('linear_residual',b.numerical_failure_reasons(r))
  b.enforce_phase('test',2200*2**20,250.)
  with self.assertRaises(b.PhaseResourceStopped):b.enforce_phase('test',b.RSS_CAP,250.)
  with self.assertRaises(b.PhaseResourceStopped):b.enforce_phase('test',1,600.)
  with self.assertRaises(ValueError):b.enforce_phase('test',1,float('nan'))
 def test_fresh_admission_changes_only_declared_resource_decision(self):
  base=dict(estimated_numeric_stage_bytes=2188930576,numeric_stage_admitted=False,current_rss_before_numeric_bytes=602521600,basis_reservation_bytes=65317016,reserve_bytes=32*2**20)
  with patch.object(b,'original_admission',return_value=base.copy()) as call:result=b.fresh_admission(object(),1,2,rss_reader=lambda:123)
  self.assertTrue(result['numeric_stage_admitted']);self.assertEqual(result['estimated_numeric_stage_bytes'],base['estimated_numeric_stage_bytes']);self.assertEqual(result['basis_reservation_bytes'],base['basis_reservation_bytes']);self.assertEqual(result['rss_cap_bytes'],3072*2**20);self.assertIn('rss_reader',call.call_args.kwargs)
 def test_atomic_publication_rechecks_and_removes_failed_pending(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'field.npz';r=self.row()
   with patch.object(b.resource,'getrusage',return_value=SimpleNamespace(ru_maxrss=2200*2**20)),patch.object(b.time,'monotonic',return_value=601.):self.assertFalse(b.publish_snapshot(p,{'fixture':np.arange(3)},r,0.))
   self.assertFalse(p.exists());self.assertFalse(p.with_name(p.name+'.pending.npz').exists())
   r=self.row()
   with patch.object(b.resource,'getrusage',return_value=SimpleNamespace(ru_maxrss=2200*2**20)),patch.object(b.time,'monotonic',return_value=251.):self.assertTrue(b.publish_snapshot(p,{'fixture':np.arange(3)},r,0.))
   self.assertTrue(p.exists())
 def test_saved_both_domain_geometry_and_exact_solver_recipe(self):
  geometry=next((R/'build/c3d-floor-balanced8').rglob('paired-floor-balanced8-geometry.npz'));self.assertEqual(w.sha(geometry),w.GEOMETRY_SHA);original=w.probe.domain_mesh
  def fake(**k):
   bundle=w.probe.domain_mesh();self.assertEqual(bundle[0].nelements,43008 if k['length']==4. else 49920);self.assertEqual((k['target'],k['retained_target'],k['maxiter'],k['restart'],k['coarse_pressure'],k['chunk_size']),(1e-10,1e-11,3000,6,'quadratic',512));return {'numerically_accepted':False}
  with patch.object(w.probe,'run',side_effect=fake):
   for length in (4.,8.):result=w.run(geometry,None,None,length);self.assertFalse(result['accuracy_contract']['performance_threshold_applied'])
  self.assertIs(w.probe.domain_mesh,original)
  with self.assertRaises(ValueError):w.run(geometry,None,None,5.)
 def test_exact_engine_and_supervisor_transforms(self):
  for t in json.loads((R/'build/c3d-accuracy-first/transforms.json').read_text()):
   v=(R/t['parent']).read_text()
   for a,c in t['literal_replacements']:self.assertIn(a,v);v=v.replace(a,c)
   v+=t.get('append','');self.assertEqual(v,(R/t['output']).read_text());self.assertEqual(hashlib.sha256((R/t['parent']).read_bytes()).hexdigest(),t['parent_sha256'])
if __name__=='__main__':unittest.main()
