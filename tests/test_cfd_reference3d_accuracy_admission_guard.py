import ast,inspect,json,sys,unittest,contextlib,io
from pathlib import Path
from types import SimpleNamespace
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
import cfd_reference3d_accuracy_graded_probe as old
import cfd_reference3d_accuracy_graded_guarded_probe as new
from cfd_reference3d_accuracy_graded_budget import PhaseResourceStopped
class AdmissionGuard(unittest.TestCase):
 def stage(self,module,admitted):
  run=ast.parse(inspect.getsource(module.run)).body[0];sample=next(n for n in run.body if isinstance(n,ast.FunctionDef) and n.name=='sample');tree=ast.Module(body=[sample],type_ignores=[])
  admission=dict(numeric_stage_admitted=admitted,estimated_numeric_stage_bytes=9*2**30 if not admitted else 5*2**30,rss_cap_bytes=8192*2**20,wall_cap_s=1800.,basis_reservation_bytes=100,coarse_pressure_reservation_bytes=20)
  scope=dict(time=SimpleNamespace(monotonic=lambda:10.),resource=SimpleNamespace(RUSAGE_SELF=0,getrusage=lambda _:SimpleNamespace(ru_maxrss=100)),started=0.,resource_samples={},json=json,enforce_phase=lambda *a:None,coarse_pressure='quadratic',coarse_reserve=lambda *a:20,nv=10,system=SimpleNamespace(volumes=[1.,1.]),fresh_admission=lambda *a:admission.copy(),exact_factor=object(),basis_reservation=lambda *a:100,C=SimpleNamespace(shape=(12,12)),restart=6,PhaseResourceStopped=PhaseResourceStopped)
  exec(compile(tree,'<bounded admission stage>','exec'),scope);return scope['sample']
 def test_actual_stage_metadata_collision_reproduced_and_resource_failure_preserved(self):
  with contextlib.redirect_stdout(io.StringIO()):
   with self.assertRaises(TypeError):self.stage(old,False)('workspace_pressure_complete')
   with self.assertRaises(PhaseResourceStopped) as caught:self.stage(new,False)('workspace_pressure_complete')
  r=caught.exception.record;self.assertEqual(r['phase'],'numeric_stage_admission');self.assertFalse(r['numeric_stage_admitted']);self.assertEqual(r['estimated_numeric_stage_bytes'],9*2**30);self.assertEqual(r['rss_cap_bytes'],8192*2**20);self.assertEqual(r['wall_cap_s'],1800)
 def test_successful_admission_progress_is_identical(self):
  logs=[]
  for module in (old,new):
   out=io.StringIO()
   with contextlib.redirect_stdout(out):self.stage(module,True)('workspace_pressure_complete')
   logs.append(out.getvalue())
  self.assertEqual(logs[0],logs[1]);self.assertTrue(json.loads(logs[1].splitlines()[1])['numeric_stage_admitted'])
if __name__=='__main__':unittest.main()
