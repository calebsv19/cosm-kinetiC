"""Once-only target full FE/cost readiness; broader policy failures remain sealed."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_exact_target_evidence import frozen,sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_encoded_operator import budget
from cfd_reference3d_size_selected import decision
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-exact-target'
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
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='db6f4c40eba5d283f2f7ac017ad42a00ed92b126e8db9509251153f0d3f6043f'
 previous=json.loads(Path(pre['path']).read_text());assert not previous['candidate_adopted'] and not previous['matched_eligible'] and not previous['small']['cost_gate_passed'] and not previous['comparisons']['prospective_large_pair_cost_gate_passed']
 for p,h in previous['source_sha256'].items():assert sha(R/p)==h
 for p,h in previous['receipt_sha256'].items():assert sha(Path(p))==h
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 e=json.loads((D/'eligibility.json').read_text());assert e['target_only_numerical_investigation_eligible'] and not e['general_size_policy_adopted'] and e['general_cost_failures_preserved'] and e['attempts_declared']==1
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 paths=list((D/'runs').glob('*/*-receipt.json'));assert len(paths)==1;p=paths[0];r,row=frozen(p)
 oldp=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'));ar,a=verify_receipt(oldp)
 result=dict(target_only_force_testing_ready=False,exact_recipe_qualified=False,general_size_policy_adopted=False,default_native_desktop_changed=False)
 if r['returncode']==0:
  accepted(p);assert row['tetrahedra']==33216 and row['identity']==a['identity'] and row['preconditioner']['rounded_values_sha256']==a['preconditioner']['rounded_values_sha256']
  stagep=next((R/'build/c3d-encoded-operator/stage-runs').glob('*/*-receipt.json'));sr=json.loads(stagep.read_text());stage=json.loads(Path(sr['command'][sr['command'].index('--output')+1]).read_text());assert row['identity']==stage['identity']
  c=force_comparison(row,a);eq=max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
  it=1-row['iterations']/a['iterations'];st=1-(row['timings']['setup_s']+row['timings']['solve_s'])/(a['timings']['setup_s']+a['timings']['solve_s']);whole=r['wall_s']/ar['wall_s'];owned=row['peak_rss_bytes']/a['peak_rss_bytes'];cost=(it>=.1 or st>=.1) and whole<=1.1 and owned<=1.1
  original_raw_mismatch=abs((row['pressure_force_n'][0]+row['raw_symmetric_viscous_force_n'][0])/row['reaction_force_n'][0]-1)
  result.update(numerical_field_available_for_diagnostics=eq,performance_qualified=cost,target_only_force_testing_ready=eq and cost,exact_recipe_qualified=eq and cost,physical_equivalence=c,physical_equivalence_accepted=eq,cost_gate_passed=cost,iterations_relative_reduction=it,setup_solve_relative_reduction=st,whole_time_ratio=whole,owned_peak_ratio=owned,raw_surface_reaction_mismatch=original_raw_mismatch,original_raw_force_gate_passed=original_raw_mismatch<=.01,measurements=dict(whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],flux_error=row['flux_error'],max_divergence=row['volume_divergence_max_s_inv'],energy_imbalance=row['physical_energy_imbalance'],timings=row['timings'],forces={k:row[k] for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n')},retirement=row['workspace_retirement']))
 else:
  verify_receipt(p);result.update(failure=r['diagnostic_failure'],result=row,whole_wall_s=r['wall_s'],sampled_peak_mib=r['peak_observed_rss_bytes']/2**20)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(result,indent=2)+'\n')
 files=list((R/'scripts').glob('*exact_target*.py'))+list((R/'docs').glob('cfd_3d_exact_target*.md'))
 audit=dict(status='PROGRESS: target-only complete numerical readiness measured; broader policy and physical qualification open',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p),str(oldp):sha(oldp)},native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,comparison_sha256=sha(cp),eligibility_sha256=sha(D/'eligibility.json'),result=result,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),result=result)))
if __name__=='__main__':main()
