"""Once-only cubic pressure control and exact-target cap rejection audit."""
import json
from pathlib import Path
from cfd_reference3d_pressure_cubic_evidence import frozen,sha
from cfd_reference3d_pressure_cubic import reserve,EXPONENTS
from cfd_reference3d_flexible import basis_reservation
from audit_cfd_3d_spatial import verify_receipt,force_comparison
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pressure-cubic'
def budget(r,row):
 phase=next(q for q in r['progress'] if q.get('phase')=='assembled');n=phase['reduced_free_dofs'];np_=phase['condensed_pressure_dofs'];a=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission')
 assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*2**20)
 assert a['residency_measurement'].startswith('fresh at stage admission')
 return a

def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='0d62ca6a8852ed290468adf24553cb9fcd65f25759ca76e6431c0e5618fb5966'
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4 and sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text()
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 sp=next((D/'runs').glob('*/*-receipt.json'));sr,small=frozen(sp);verify_receipt(sp);budget(sr,small)
 op=next((R/'build/c3d-size-selected/runs').glob('*/L4-body2-original-selected-quadratic-receipt.json'));ar,a=verify_receipt(op);assert small['identity']==a['identity'] and small['preconditioner']['rounded_values_sha256']==a['preconditioner']['rounded_values_sha256'] and small['target']==1e-10 and small['retained_target']==1e-11 and small['final_residual']['true_residual']<1e-10
 c=force_comparison(small,a);assert max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
 pc=small['pressure_preconditioner'];assert pc['columns']==20 and pc['complete_cubic_span'] and pc['exponents']==[list(e) for e in EXPONENTS] and pc['constant_pressure_direction_retained'] and pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5 and pc['coarse_smallest_eigenvalue']>0
 assert small['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and small['workspace_retirement']['remaining_authority_preserved_bitwise'] and small['condensation']['factor_metadata_residency']['catalogs_restored_bitwise'] and small['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
 e=json.loads((D/'target-eligibility.json').read_text());assert e['target_numerical_investigation_eligible'] and e['small_complete_numerical_gate_passed'] and e['small_iterations']==small['iterations'] and e['small_prior_iterations']==a['iterations'] and e['small_force_comparison']==c and e['small_performance_regression_retained'] and e['attempts_declared']==1
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 targetp=list((D/'target-runs').glob('*/*-receipt.json'));assert len(targetp)==1;tp=targetp[0];tr,target=verify_receipt(tp);assert tr['returncode']==-15 and tr['stop_reason']=='reference RSS/time cap' and target is None and tr['wall_s']>=180 and tr['diagnostic_failure']['kind']=='resource_cap'
 assert not tp.with_name(tp.name.removesuffix('-receipt.json')+'.npz').exists();guard=budget(tr,target);assert guard['numeric_stage_admitted']
 oldp=next((R/'build/c3d-exact-target/runs').glob('*/*-receipt.json'));orr,old=verify_receipt(oldp);actual=next(q['identity'] for q in tr['progress'] if q.get('phase')=='assembled');assert actual==old['identity']
 for p,h in tr['source_sha256'].items():assert sha(R/'scripts'/p)==h
 phases=[q.get('phase') for q in tr['progress']];assert 'workspace_numeric_ready' in phases and 'completed_workspace_retired' in phases and 'publication_complete' not in phases
 last=next((q for q in reversed(tr['progress']) if 'iteration' in q),None)
 comparison=dict(optional_cubic_pressure_adopted=False,exact_target_qualified=False,small=dict(iterations=small['iterations'],previous_iterations=a['iterations'],whole_wall_s=sr['wall_s'],owned_peak_mib=small['peak_rss_bytes']/2**20,full_residual=small['final_residual'],force_comparison=c),target=dict(returncode=tr['returncode'],whole_wall_s=tr['wall_s'],sampled_peak_mib=tr['peak_observed_rss_bytes']/2**20,last_iteration_observation=last,last_phase=tr['progress'][-1],numeric_stage_admission=guard,numerical_field_published=False,full_target_or_physical_checks_not_proven_from_phase_marker=True),original_physical_gate_replaced=False,physical_accuracy_certified=False)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(comparison,indent=2)+'\n')
 files=list((R/'scripts').glob('*pressure_cubic*.py'))+[R/'tests/test_cfd_reference3d_pressure_cubic.py']+list((R/'docs').glob('cfd_3d_pressure_cubic*.md'))
 audit=dict(status='PROGRESS: cubic pressure support/small strict field pass; exact cube time cap rejected, no adoption or field',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p) for p in (sp,op,tp,oldp)},native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),comparison_sha256=sha(cp),comparisons=comparison,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),adopted=False)))
if __name__=='__main__':main()
