"""Once-only prospective size policy, full fields and retained small failure."""
import json
from pathlib import Path
from cfd_reference3d_selected_evidence import frozen,sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_encoded_operator import budget
from cfd_reference3d_size_selected import decision
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-size-selected'

def accepted(p):
 r,row=frozen(p);verify_receipt(p);assert row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10 and not row['physical_accuracy_certified']
 pc=row['preconditioner'];f=pc['flexible_iteration'];assert f['final_true_metric']<=1e-11 and pc['shared_input_preserved_after_solve'] and pc['user_factor_storage_verified'] and pc['pressure_control']['action_preserved'] and pc['pressure_control']['live_input_preserved']
 m=row['condensation'];assert m['full_load_residency']['restored_bitwise_after_factor_cleanup'] and m['factor_metadata_residency']['catalogs_restored_bitwise'] and m['block_storage']['velocity']['full_mixed_action_bitwise_preserved']
 s=m['block_storage']['storage_selection'];assert s==decision(s['coefficient_count'],s['mode'])
 if s['encoded_selected']:
  e=pc['exact_encoding'];assert e['bitwise_roundtrip_verified'] and e['original_value_sha256']==e['decoded_value_sha256'] and e['saved_joint_bytes']==s['predicted_joint_saved_bytes']>=32*2**20 and pc['factor_input_allocation_bytes']==0 and m['block_storage']['velocity']['original_double_owner_released']
 else:assert pc['factor_input_allocation_bytes']==s['predicted_joint_saved_bytes'] and not m['block_storage']['velocity']['original_double_owner_released']
 guard=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');budget(guard,row['condensed_free_dofs'],m['condensed_pressure_dofs']);assert guard['numeric_stage_admitted']
 retire=row['workspace_retirement'];assert retire['performed'] and retire['factor_and_coarse_owners_released'] and retire['all_completed_buffers_and_backing_owners_released'] and retire['remaining_authority_preserved_bitwise'] and not retire['explicit_collection_performed'] and not retire['allocator_relief_performed'] and retire['owned_high_water_after_bytes']>=retire['capture_owned_high_water_bytes']
 coarse=row['pressure_preconditioner'];assert coarse['columns']==10 and coarse['constant_pressure_direction_retained'] and coarse['coarse_reservation_bytes']==guard['coarse_pressure_reservation_bytes'] and coarse['coarse_relative_skew']<1e-5 and coarse['coarse_reproduction_relative_error']<1e-5 and coarse['coarse_smallest_eigenvalue']>0
 return r,row

def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='ac45451d580fd40054e0bf8d4b2eebca136588a8263080a9484849d87649d67b'
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==2 and sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text() and sha(D/'source-transform-control.json')==support['transformation_sha256']
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 smallp=next((D/'runs').glob('*/L4-body2-original-selected-quadratic-receipt.json'));sr,small=accepted(smallp)
 smallold=next((R/'build/c3d-pressure-coarse/runs').glob('*/L4-body2-original-quadratic-receipt.json'));ar,a=verify_receipt(smallold)
 smallfailed=next((R/'build/c3d-encoded-operator/runs').glob('*/L4-body2-original-encoded-quadratic-receipt.json'));br,b=verify_receipt(smallfailed)
 sc=json.loads((D/'small-comparison.json').read_text());assert small['identity']==a['identity'] and sc['physical_equivalence']==force_comparison(small,a) and sc['whole_time_ratio']==sr['wall_s']/ar['wall_s'] and sc['owned_peak_ratio']==small['peak_rss_bytes']/a['peak_rss_bytes'] and sc['owned_improvement_vs_rejected']==1-small['peak_rss_bytes']/b['peak_rss_bytes']
 assert sc['cost_gate_passed']==(sc['whole_time_ratio']<=1.1 and sc['owned_peak_ratio']<=1.1 and sc['owned_improvement_vs_rejected']>=.05)==False
 for p,h in sc['input_sha256'].items():assert sha(Path(p))==h
 normal={};receipts=dict(sc['input_sha256']);measurements={}
 for mode in ('legacy','selected'):
  p=next((D/'runs').glob(f'*/L4-body6-normal-{mode}-quadratic-receipt.json'));r,row=accepted(p);normal[mode]=(r,row);receipts[str(p)]=sha(p)
  op=next((R/'build/c3d-pressure-coarse/runs').glob('*/L4-body6-normal-quadratic-receipt.json'));orr,old=verify_receipt(op);receipts[str(op)]=sha(op)
  assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
  c=force_comparison(row,old);assert max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
  measurements[mode]=dict(whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],timings=row['timings'],historical_whole_ratio=r['wall_s']/orr['wall_s'],historical_owned_ratio=row['peak_rss_bytes']/old['peak_rss_bytes'])
 lr,l=normal['legacy'];cr,candidate=normal['selected'];assert l['identity']==candidate['identity'] and l['preconditioner']['rounded_values_sha256']==candidate['preconditioner']['rounded_values_sha256']
 force=force_comparison(candidate,l);assert max([*force['component_relative_changes'].values(),*force['scalar_relative_changes'].values()])<1e-7
 whole=cr['wall_s']/lr['wall_s'];owned=candidate['peak_rss_bytes']/l['peak_rss_bytes'];saving=candidate['preconditioner']['exact_encoding']['saved_joint_bytes'];gate=whole<=1.1 and 1-owned>=.1 and saving>=32*2**20
 comparison=dict(prospective_large_pair_cost_gate_passed=gate,whole_time_ratio=whole,owned_peak_ratio=owned,owned_peak_improvement=1-owned,joint_saved_bytes=saving,physical_equivalence=force,small_historical_cost_gate_passed=False,matched_eligible=False,candidate_adopted=False,original_physical_gate_replaced=False,measurements=measurements,input_sha256=receipts)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(comparison,indent=2)+'\n');assert not (D/'matched-runs').exists()
 files=list((R/'scripts').glob('*selected*.py'))+[R/'tests/test_cfd_reference3d_size_selected.py']+list((R/'docs').glob('cfd_3d_size_selected*.md'))
 result=dict(status='PROGRESS: size selection and complete prospective larger pair verified; small historical cost failure retained, matched numeric withheld',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,candidate_adopted=False,matched_eligible=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256=receipts,native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),comparison_sha256=sha(cp),comparisons=comparison,small=sc,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),large_pair_gate=gate,whole_time_ratio=whole,owned_peak_improvement=1-owned)))
if __name__=='__main__':main()
