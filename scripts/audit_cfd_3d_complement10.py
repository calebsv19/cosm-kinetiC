"""Once-only complement10 numerical and exact-reference recipe qualification."""
import json
from pathlib import Path
from cfd_reference3d_complement10_evidence import frozen,sha,R,D
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_flexible import basis_reservation
from audit_cfd_3d_spatial import verify_receipt,force_comparison

def budget(r):
 m=next(q for q in r['progress'] if q.get('phase')=='assembled');n=m['reduced_free_dofs'];np_=m['condensed_pressure_dofs'];a=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission')
 assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']<=1800*2**20 and a['residency_measurement'].startswith('fresh at stage admission')
 return a

def full(p,oldp):
 r,row=frozen(p);verify_receipt(p);orr,old=verify_receipt(oldp);a=budget(r)
 assert row['info']==0 and row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10 and row['preconditioner']['flexible_iteration']['final_true_metric']<=1e-11
 assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
 pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['complementary_mass_inverse_scale']==10 and pc['coarse_correction_scale']==1 and pc['constant_pressure_direction_retained'] and pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5 and pc['coarse_smallest_eigenvalue']>0
 assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released'] and row['workspace_retirement']['remaining_authority_preserved_bitwise'] and row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise'] and row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
 c=force_comparison(row,old);assert max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
 return r,row,orr,old,c,a

def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='2cf501edd853b6b763478bb38a5105d882c4a3df0fbb128dee95be697a4f5aa3'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4 and sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text()
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 sp=next((D/'runs').glob('*/*-receipt.json'));op=next((R/'build/c3d-size-selected/runs').glob('*/L4-body2-original-selected-quadratic-receipt.json'));sr,small,orr,old,c,a=full(sp,op)
 e=json.loads((D/'target-eligibility.json').read_text());assert e['target_numerical_investigation_eligible'] and e['small_strict_complete_field'] and e['small_iterations']==small['iterations'] and e['small_force_comparison']==c and e['attempts_declared']==1
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 tps=list((D/'target-runs').glob('*/*-receipt.json'));assert len(tps)==1;tp=tps[0];tr,target=verify_receipt(tp);budget(tr)
 tenp=next((R/'build/c3d-exact-target/runs').glob('*/*-receipt.json'));massp=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'))
 perf=False;tc=None;mc=None;gates={}
 if target is not None and target.get('numerically_accepted'):
  tr,target,tenr,ten,tc,ta=full(tp,tenp);massr,mass=verify_receipt(massp);assert target['identity']==mass['identity'] and target['preconditioner']['rounded_values_sha256']==mass['preconditioner']['rounded_values_sha256'];mc=force_comparison(target,mass);assert max([*mc['component_relative_changes'].values(),*mc['scalar_relative_changes'].values()])<1e-7
  cost=lambda row:row['timings']['setup_s']+row['timings']['solve_s'];gates=dict(iteration_improvement_at_least_10pct=target['iterations']<=.9*mass['iterations'],setup_solve_improvement_at_least_10pct=cost(target)<=.9*cost(mass),whole_wall_within_10pct_both=tr['wall_s']<=1.1*min(tenr['wall_s'],massr['wall_s']),owned_peak_within_10pct_both=target['peak_rss_bytes']<=1.1*min(ten['peak_rss_bytes'],mass['peak_rss_bytes']),setup_solve_no_10pct_regression_vs_ten=cost(target)<=1.1*cost(ten))
  perf=(gates['iteration_improvement_at_least_10pct'] or gates['setup_solve_improvement_at_least_10pct']) and all(gates[k] for k in ('whole_wall_within_10pct_both','owned_peak_within_10pct_both','setup_solve_no_10pct_regression_vs_ten'))
  detail=dict(iterations=target['iterations'],mass_iterations=mass['iterations'],ten_iterations=ten['iterations'],whole_wall_s=tr['wall_s'],owned_peak_mib=target['peak_rss_bytes']/2**20,full_residual=target['final_residual'],timings=target['timings'],iteration_improvement_fraction=1-target['iterations']/mass['iterations'],setup_solve_improvement_fraction=1-cost(target)/cost(mass),force_comparison_vs_ten=tc,force_comparison_vs_mass=mc,physical_energy_imbalance=target['physical_energy_imbalance'],volume_divergence_max_s_inv=target['volume_divergence_max_s_inv'],flux_error=target['flux_error'])
 else:
  assert tr['returncode']!=0;detail=dict(returncode=tr['returncode'],wall_s=tr['wall_s'],sampled_peak_mib=tr['peak_observed_rss_bytes']/2**20,numerical_field_published=False)
 comparison=dict(optional_exact_reference_recipe_qualified=perf,target_only_force_testing_ready=perf,general_or_native_default_adopted=False,physical_accuracy_certified=False,persistent_goal_complete=False,qualification_gates=gates,small=dict(iterations=small['iterations'],previous_iterations=old['iterations'],whole_wall_s=sr['wall_s'],full_residual=small['final_residual'],force_comparison=c),target=detail,original_force_energy_gates_preserved=True)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(comparison,indent=2)+'\n')
 files=list((R/'scripts').glob('*complement10*.py'))+[R/'tests/test_cfd_reference3d_complement10.py']+list((R/'docs').glob('cfd_3d_complement10*.md'))
 audit=dict(status='PROGRESS: exact-reference force testing ready' if perf else 'PROGRESS: complement scaling target not qualified',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p) for p in (sp,op,tp,tenp,massp)},native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),comparison_sha256=sha(cp),comparisons=comparison,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),exact_force_testing_ready=perf)))
if __name__=='__main__':main()
