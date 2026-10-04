"""Once-only matched shorter-domain full numerical proof."""
import json,hashlib
from pathlib import Path
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_flexible import basis_reservation
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-force-resume'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='97654b8181c9171956189d8fafe779b5a5777c3b466143aa1d78b0c6f3cbb8f9'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 p=next((D/'runs').glob('*/*-receipt.json'));r,row=verify_receipt(p)
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 oldp=next((R/'build/c3d-second-normal/runs').glob('*/L4-body6-second-normal-tensor-receipt.json'));orr,old=verify_receipt(oldp);assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
 assert row['length']==4 and row['tetrahedra']==28416 and row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10 and row['preconditioner']['flexible_iteration']['final_true_metric']<=1e-11 and row['info']==0
 assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['remaining_authority_preserved_bitwise'] and row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise'] and row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
 pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['complementary_mass_inverse_scale']==10 and pc['coarse_correction_scale']==1
 a=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');n=row['condensed_free_dofs'];np_=row['condensation']['condensed_pressure_dofs'];assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']<=1800*2**20
 c=force_comparison(row,old);assert max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
 lp=next((R/'build/c3d-complement10/target-runs').glob('*/*-receipt.json'));lr,long=verify_receipt(lp);dc=force_comparison(long,row);assert not c['physical_force_gate_passed'] and not dc['physical_force_gate_passed']
 comp=dict(matched_domain_numerical_readiness=True,physical_accuracy_certified=False,general_or_native_adopted=False,whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],old_iterations=old['iterations'],old_retained_margin_not_identical=old.get('retained_target')!=row['retained_target'],iteration_performance_improvement_claimed=False,full_residual=row['final_residual'],timings=row['timings'],original_L4_field_comparison=c,original_L4_L8_domain_comparison=dc,numeric_stage_admission=a,original_force_energy_gates_preserved=True)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(comp,indent=2)+'\n')
 files=list((R/'scripts').glob('*force_resume*.py'))+list((R/'docs').glob('cfd_3d_force_resume*.md'))
 out.write_text(json.dumps(dict(status='PROGRESS: matched L4 strict numerical control complete; physical mesh screen next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p) for p in (p,oldp,lp)},native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,comparison_sha256=sha(cp),comparisons=comp,committed=False,packaged=False,installed=False),indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out))))
if __name__=='__main__':main()
