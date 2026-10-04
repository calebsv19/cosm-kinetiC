"""Once-only lifecycle and complete cost rejection audit; no failed gate replaced."""
import json
from pathlib import Path
from cfd_reference3d_retired_evidence import frozen,sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_encoded_operator import budget
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-workspace-retirement'
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='36a60b0f7f63615a1e9fe5caab4b07ad3100f95cf046407e011ee0ce09bc73ab'
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==3 and sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text()
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 results={};receipts={}
 for count,group in ((2,'original'),(6,'normal')):
  p=next((D/'runs').glob(f'*/L4-body{count}-{group}-retired-quadratic-receipt.json'));r,row=frozen(p);verify_receipt(p)
  op=next((R/'build/c3d-pressure-coarse/runs').glob(f'*/L4-body{count}-{group}-quadratic-receipt.json'));ar,a=verify_receipt(op)
  rejected=next((R/'build/c3d-encoded-operator/runs').glob(f'*/L4-body{count}-{group}-encoded-quadratic-receipt.json'));br,b=verify_receipt(rejected)
  assert row['identity']==a['identity'] and row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10
  pc=row['preconditioner'];f=pc['flexible_iteration'];assert f['final_true_metric']<=1e-11 and pc['rounded_values_sha256']==a['preconditioner']['rounded_values_sha256'] and pc['shared_input_preserved_after_solve'] and pc['user_factor_storage_verified'] and pc['pressure_control']['live_input_preserved'] and pc['pressure_control']['action_preserved']
  m=row['condensation'];assert m['factor_metadata_residency']['catalogs_restored_bitwise'] and m['full_load_residency']['restored_bitwise_after_factor_cleanup']
  guard=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');budget(guard,row['condensed_free_dofs'],m['condensed_pressure_dofs']);assert guard['numeric_stage_admitted']
  retire=row['workspace_retirement'];assert retire['performed'] and retire['all_completed_buffers_and_backing_owners_released'] and retire['remaining_authority_preserved_bitwise'] and not retire['explicit_collection_performed'] and not retire['allocator_relief_performed'] and retire['owned_high_water_after_bytes']>=retire['capture_owned_high_water_bytes']
  phases=[q.get('phase') for q in r['progress']];assert phases.index('completed_workspace_retired')<phases.index('factor_metadata_restored')<phases.index('full_residual_verified')<phases.index('publication_complete')
  c=force_comparison(row,a);assert max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
  whole=r['wall_s']/ar['wall_s'];owned=row['peak_rss_bytes']/a['peak_rss_bytes'];recovery=1-row['peak_rss_bytes']/b['peak_rss_bytes'];saving=pc['exact_encoding']['saved_joint_bytes']
  passed=whole<=1.1 and owned<=1.1 and (recovery>=.05 if count==2 else (1-owned>=.1 and saving>=32*2**20))
  x=dict(control=group,numerical_accepted=True,whole_time_ratio=whole,owned_peak_ratio=owned,owned_improvement_vs_rejected=recovery,physical_equivalence=c,cost_gate_passed=passed,whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],retirement=retire,input_sha256={str(p):sha(p),str(op):sha(op),str(rejected):sha(rejected)})
  cp=D/(group+'-control-comparison.json')
  if cp.exists():assert json.loads(cp.read_text())==x
  else:cp.write_text(json.dumps(x,indent=2)+'\n')
  results[group]=x;receipts.update(x['input_sha256'])
 eligible=all(x['cost_gate_passed'] for x in results.values());assert not eligible and not (D/'matched-runs').exists()
 files=list((R/'scripts').glob('*retired*.py'))+[R/'scripts/cfd_reference3d_workspace_retirement.py',Path(__file__),R/'tests/test_cfd_reference3d_workspace_retirement.py']+list((R/'docs').glob('cfd_3d_workspace_retirement*.md'))
 result=dict(status='PROGRESS: all completed workspace owners released, strict full fields preserved; memory improves but original time gates fail, matched withheld',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,workspace_retirement_adopted=False,matched_eligible=eligible,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256=receipts,native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),controls=results,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),matched_eligible=eligible,control_cost={k:{n:x[n] for n in ('whole_time_ratio','owned_peak_ratio','owned_improvement_vs_rejected')} for k,x in results.items()})))
if __name__=='__main__':main()
