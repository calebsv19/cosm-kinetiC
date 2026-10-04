"""Once-only exact action/factor and unchanged complete control cost audit."""
import json
from pathlib import Path
from cfd_reference3d_encoded_evidence import frozen,sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_coarse import reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-encoded-operator'

def budget(a,n,np_):
 assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','basis_reservation_bytes','reserve_bytes')) and a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*2**20)
 assert a['residency_measurement'].startswith('fresh at stage admission')

def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='121763933b36815d1aa0e425a9417bb28a0a20bd49fc343ec46f9a1fe3b9ab41'
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==6
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text() and sha(D/'support/factor.dylib')==support['factor_library_sha256'] and sha(D/'support/factor-build.json')==support['factor_build_sha256']
 assert sha(D/'source-transform-control.json')==support['transformation_sha256'] and sha(D/'factor-transform-control.json')==support['factor_transform_sha256']
 factor=json.loads((D/'factor-transform-control.json').read_text())
 for t in [factor,*json.loads((D/'source-transform-control.json').read_text())]:
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 assert (R/'scripts/cfd_reference3d_encoded_storage.c').read_text().startswith((R/'scripts/cfd_reference3d_mixed_storage.c').read_text()) and factor['c_parent_sha256']==sha(R/'scripts/cfd_reference3d_mixed_storage.c')
 stagep=next((D/'stage-runs').glob('*/*-receipt.json'));sr,stage=frozen(stagep);assert sr['returncode']==0 and stage['diagnostic_accepted'] and not stage['numeric_factor_attempted'] and not stage['numerically_accepted'] and not stage['numerical_field_published'] and stage['symbolic_handle_cleanup_verified'] and stage['original_mixed_input_preserved']
 budget(stage['admission'],stage['condensation']['reduced_free_dofs'],stage['condensation']['condensed_pressure_dofs']);assert stage['admission']['numeric_stage_admitted']
 eligibility=json.loads((D/'stage-eligibility.json').read_text());assert eligibility['controls_eligible'] and eligibility['known_joint_backing_saved_bytes']==89646480
 for p,h in eligibility['input_sha256'].items():assert sha(Path(p))==h
 results={};receipts={str(stagep):sha(stagep)};measurements={}
 for count,group in ((2,'original'),(6,'normal')):
  p=next((D/'runs').glob(f'*/L4-body{count}-{group}-encoded-quadratic-receipt.json'));r,row=frozen(p);verify_receipt(p);assert row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10
  oldp=next((R/'build/c3d-pressure-coarse/runs').glob(f'*/L4-body{count}-{group}-quadratic-receipt.json'));ar,a=verify_receipt(oldp);assert row['identity']==a['identity']
  pc=row['preconditioner'];m=row['condensation']['block_storage']['velocity'];e=pc['exact_encoding']
  assert pc['rounded_values_sha256']==a['preconditioner']['rounded_values_sha256']==e['predictor_float_sha256'] and pc['original_physical_vector_input_sha256']==a['identity']['vector_storage_sha256'] and pc['factor_input_allocation_bytes']==0 and pc['shared_input_preserved_after_solve'] and pc['user_factor_storage_verified'] and pc['pressure_control']['action_preserved'] and pc['pressure_control']['live_input_preserved']
  assert e['bitwise_roundtrip_verified'] and e['original_value_sha256']==e['decoded_value_sha256'] and m['original_double_owner_released'] and m['full_mixed_action_bitwise_preserved']
  assert row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup'] and row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise']
  f=pc['flexible_iteration'];assert f['restart']==6 and f['iterations']==row['iterations'] and f['basis_array_bytes']<f['basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],6) and f['final_true_metric']<=1e-11
  guard=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');budget(guard,row['condensed_free_dofs'],row['condensation']['condensed_pressure_dofs']);assert guard['numeric_stage_admitted']
  coarse=row['pressure_preconditioner'];assert coarse['columns']==10 and coarse['constant_pressure_direction_retained'] and coarse['coarse_reservation_bytes']==guard['coarse_pressure_reservation_bytes'] and coarse['coarse_relative_skew']<1e-5 and coarse['coarse_reproduction_relative_error']<1e-5 and coarse['coarse_smallest_eigenvalue']>0
  c=force_comparison(row,a);assert max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
  whole=r['wall_s']/ar['wall_s'];owned=row['peak_rss_bytes']/a['peak_rss_bytes'];passed=whole<=1.1 and owned<=1.1 and (e['saved_joint_bytes']>=32*2**20 or 1-owned>=.1)
  actual=json.loads((D/(group+'-control-comparison.json')).read_text());assert actual['physical_equivalence']==c and actual['whole_time_ratio']==whole and actual['owned_peak_ratio']==owned and actual['cost_gate_passed']==passed
  for q,h in actual['input_sha256'].items():assert sha(Path(q))==h
  results[group]=actual;receipts.update({str(p):sha(p),str(oldp):sha(oldp)});measurements[group]=dict(whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],timings=row['timings'])
 matched=json.loads((D/'matched-eligibility.json').read_text());assert matched['matched_eligible']==all(q['cost_gate_passed'] for q in results.values())==False and not matched['numerical_adapter_adopted'] and not (D/'matched-runs').exists()
 for p,h in matched['input_sha256'].items():assert sha(Path(p))==h
 files=list((R/'scripts').glob('*encoded*.py'))+[R/'scripts/cfd_reference3d_encoded_storage.c',R/'tests/test_cfd_reference3d_encoded_operator.py']+list((R/'docs').glob('cfd_3d_encoded_operator*.md'))
 result=dict(status='PROGRESS: exact action/shared factor verified; matched stage admits; small complete cost failure retained, normal17percent memory improvement; no matched numeric/adoption',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,numerical_adapter_adopted=False,matched_eligible=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256=receipts,native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),controls=results,measurements=measurements,stage_admission=stage['admission'],eligibility_sha256=sha(D/'matched-eligibility.json'),committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),matched_eligible=False)))
if __name__=='__main__':main()
