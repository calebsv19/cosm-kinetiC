"""Seal exact cubic projection controls and current native-stencil diagnostics."""
import ast,hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_cubic_pressure_projection import interval_trace
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-cubic-pressure-projection'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==3 and sha(D/'support/tests.log')==s['test_log_sha256']
 for q,h in s['source_sha256'].items():assert sha(R/q)==h==sha(D/'support/frozen'/q)
 t=json.loads((D/'clip-transform.json').read_text());assert sha(R/t['parent'])==t['parent_sha256'];parent=(R/t['parent']).read_text();node=next(n for n in ast.parse(parent).body if isinstance(n,ast.FunctionDef) and n.name=='clip_tetra');v=ast.get_source_segment(parent,node);assert v==t['original_function']
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert '"""Original convex tetra/box clipping partition; cubic quadrature is separate."""\nimport numpy as np\n\n'+v+'\n'==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 t=json.loads((D/'runner-transform.json').read_text());assert sha(R/t['parent'])==t['parent_sha256'];v=(R/t['parent']).read_text()
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert v==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 c=(R/'src/app/cfd_obstacle3d.c').read_text();assert '(11 * pressure(s, p, fluid_cell) - 7 * pressure(s, p, c2) + 2 * pressure(s, p, c3)) /' in c
 observations=[]
 for p in sorted((D/'runs').glob('*/*-receipt.json')):
  r=json.loads(p.read_text());assert r['schema']=='physics_sim_c3d_cubic_projection_receipt_v1' and r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['wall_s']<180 and r['peak_observed_rss_bytes']<1024*2**20
  for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
  for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
  assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest() and sha(p.parent.parent.parent/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
  row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and not row['physical_accuracy_certified'] and not row['native_solved_pressure_error_measured'] and sha(Path(row['input_receipt']))==row['input_receipt_sha256'] and sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
  averages=np.array([slab['pressure_average_pa'] for slab in row['slabs']]);trace=interval_trace(averages[:3])-interval_trace(averages[3:]);assert abs(trace-row['native_trace_on_reference_n'])<1e-14 and abs(abs(trace/row['raw_reference_pressure_force_n']-1)-row['trace_reconstruction_relative_difference'])<1e-14
  with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:assert bool(z['diagnostic_only']);np.testing.assert_array_equal(z['pressure_average_pa'],averages)
  observations.append(dict(n=row['n'],receipt=str(p),receipt_sha256=sha(p),input_receipt_sha256=row['input_receipt_sha256'],raw_pressure_force_n=row['raw_reference_pressure_force_n'],native_trace_on_reference_n=trace,reconstruction_relative_difference=row['trace_reconstruction_relative_difference'],wall_s=r['wall_s'],peak_mib=r['peak_observed_rss_bytes']/2**20))
 assert {o['n'] for o in observations}=={16,32} and len({o['input_receipt_sha256'] for o in observations})==1
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 source_sha={**s['source_sha256'],'scripts/audit_cfd_3d_cubic_projection.py':sha(Path(__file__)),'docs/cfd_3d_cubic_projection_checkpoint.md':sha(R/'docs/cfd_3d_cubic_projection_checkpoint.md')}
 out.write_text(json.dumps(dict(status='CUBIC PROJECTION CALIBRATED; NATIVE TRACE BIAS MEASURED',tests_passed=3,source_sha256=source_sha,support_receipt_sha256=sha(D/'support/receipt.json'),observations=observations,native_hashes_preserved=native,physical_accuracy_certified=False,persistent_goal_complete=False),indent=2)+'\n');print(json.dumps(dict(checkpoint=str(out),sha256=sha(out),observations=observations)))
if __name__=='__main__':main()
