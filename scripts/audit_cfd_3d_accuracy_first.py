"""Seal accuracy-first fields, independent force comparisons and stress evidence."""
import ast,hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_accuracy_evidence import verify,physical_comparison,sha
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-accuracy-first'
def observer(path):
 r=json.loads(path.read_text());assert r['schema']=='physics_sim_c3d_accuracy_signed_receipt_v1' and r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['rss_cap_bytes']==3072*2**20 and r['wall_cap_s']==600 and r['wall_s']<600 and r['peak_observed_rss_bytes']<3072*2**20
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(path.parent/'source'/q)
 assert path.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
 assert sha(path.parent.parent.parent/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and not row['physical_accuracy_certified'] and sha(Path(row['input_receipt']))==row['input_receipt_sha256'] and sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
 assert max(abs(x) for lift in row['lifts'] for part in ('pressure','viscous') for x in lift[part]['identity_error_n'])<1e-9
 with np.load(Path(row['signed_attribution_path']),allow_pickle=False) as z:
  signed=z['interior_jump_n'].sum(axis=-1)-z['weighted_volume_divergence_n'].sum(axis=-1)
  np.testing.assert_allclose(z['cell_net_n'].sum(axis=-1),signed,rtol=1e-10,atol=1e-12)
  for i,lift in enumerate(row['lifts']):
   for j,part in enumerate(('pressure','viscous')):np.testing.assert_allclose(signed[i,j],lift[part]['jump_minus_volume_n'],rtol=1e-10,atol=1e-12)
 return r,row

def main():
 out=D/'checkpoint-audit.json';assert not out.exists()
 pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='9c9f1bc4c9907a53d68f8dc50fa6c708ca1c21594f99fb908caf2105656304b8'
 changed=[q for q,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/q)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 sources={};tests=0
 for lane,transform in (('support','transforms.json'),('physical-support','observer-transforms.json')):
  s=json.loads((D/lane/'receipt.json').read_text());tests+=s['tests_passed'];assert sha(D/lane/'tests.log')==s['test_log_sha256'] and sha(D/transform)==s['transforms_sha256']
  for q,h in s['source_sha256'].items():assert sha(R/q)==h==sha(D/lane/'frozen'/q);sources[q]=h
  for t in json.loads((D/transform).read_text()):
   assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
   for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
   assert v+t.get('append','')==(R/t['output']).read_text()
 assert tests==9
 t=json.loads((D/'physical-check-transform.json').read_text());assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256']
 original=t['original_functions'];parent=(R/t['parent']).read_text();parts=[ast.get_source_segment(parent,n) for n in ast.parse(parent).body if isinstance(n,ast.FunctionDef) and n.name in ('numerical_failure_reasons','publish_snapshot')];assert original=='\n\n'.join(parts)
 v=original.replace("row['final_residual']['true_residual']<1e-8","row['final_residual']['true_residual']<row['target']").replace('row[\'peak_rss_bytes\']<1800*1024**2 and row[\'wall_s\']<180',"row['peak_rss_bytes']<RSS_CAP and row['wall_s']<WALL_CAP")
 assert v==('def numerical_failure_reasons'+(R/t['output']).read_text().split('def numerical_failure_reasons',1)[1]).rstrip()
 assert json.loads((D/'support/receipt.json').read_text())['host_memory_bytes']==16*2**30
 assessment=json.loads((D/'physical-assessment.json').read_text());fine={};receipts={}
 for L in (4,8):
  q=Path(assessment['numerical_fields'][f'L{L}']['receipt']);r,row=verify(q);fine[L]=row;receipts[f'L{L}']=sha(q);assert receipts[f'L{L}']==assessment['numerical_fields'][f'L{L}']['receipt_sha256']
  f=row['preconditioner'];assert f['kind']=='mixed_workspace_coupled_cholesky' and f['shared_input_preserved_after_factor'] and f['shared_input_preserved_after_solve'] and f['physical_operator_dtype']=='float64' and f['exact_encoding']['bitwise_roundtrip_verified'] and f['exact_encoding']['original_values_preserved_bitwise'] and f['exact_encoding']['original_value_sha256']==f['exact_encoding']['decoded_value_sha256']
  assert row['final_retained_residual']['estimated_full_residual']<1e-11 and f['flexible_iteration']['restart']==6 and f['flexible_iteration']['final_true_metric']<1e-11
  a=assessment['same_domain_refinement'][f'L{L}'];oldp=Path(a['coarse_receipt']);_,old=verify_receipt(oldp);assert sha(oldp)==a['coarse_receipt_sha256']
  expected=physical_comparison(old,row,'same_domain_refinement');assert {k:a[k] for k in expected}==expected and expected['force_change_passed'] and expected['scalar_refinement_passed'] and not expected['raw_equilibrium_passed']
 assert physical_comparison(fine[4],fine[8],'domain_sensitivity')==assessment['domain_sensitivity'] and assessment['domain_sensitivity']['force_change_passed']
 observations={}
 for p in sorted((D/'observer-runs').glob('*/*-receipt.json')):
  r,row=observer(p);observations[p.stem]=dict(receipt=str(p),receipt_sha256=sha(p),wall_s=r['wall_s'],peak_mib=r['peak_observed_rss_bytes']/2**20,signed_force_attribution=row['signed_force_attribution'],maximum_identity_error_n=max(abs(x) for lift in row['lifts'] for part in ('pressure','viscous') for x in lift[part]['identity_error_n']))
 assert len(observations)>=1
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 for q in ('scripts/audit_cfd_3d_accuracy_first.py','docs/cfd_3d_accuracy_first_checkpoint.md'):sources[q]=sha(R/q)
 result=dict(status='FINER FORCE TESTING RESUMED; RAW TRACTION EQUILIBRIUM STILL OPEN',persistent_goal_complete=False,tests_passed=tests,source_sha256=sources,predecessor_sha256=pre['sha256'],physical_assessment_sha256=sha(D/'physical-assessment.json'),accepted_fine_receipt_sha256=receipts,physical_assessment=assessment,stress_observations=observations,native_hashes_preserved=native,changed_preexisting_files=changed)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(checkpoint=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
