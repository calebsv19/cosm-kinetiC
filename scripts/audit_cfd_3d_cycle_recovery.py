import json,hashlib
from pathlib import Path
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_coarse import reserve
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-cycle-recovery';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='da648b0d21f7538caf1a82799034bf48f49a5dd6175117f34b3f3b180a5da5b6'
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text() and sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 pp=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'));_,previous=verify_receipt(pp);receipts={str(pp):sha(pp)};records={}
 for p in sorted((D/'stage-runs').glob('*/*-receipt.json')):
  r,row=parent.frozen(p);receipts[str(p)]=sha(p);assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
  assert row['diagnostic_accepted'] and not row['numeric_factor_attempted'] and not row['numerical_field_published'] and not row['numerically_accepted'] and not row['physical_accuracy_certified'] and row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved'] and row['identity']==previous['identity'] and row['tetrahedra']==33216
  assert r['wall_s']<180 and max(r['peak_observed_rss_bytes'],row['peak_rss_bytes'])<1800*2**20 and not p.with_name(p.name.removesuffix('-receipt.json')+'.npz').exists()
  a=row['admission'];n=row['condensation']['reduced_free_dofs'];np_=row['condensation']['condensed_pressure_dofs'];assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
  assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*2**20)
  q=a['pressure'];assert q['live_input_preserved'] and q['action_preserved'] and q['owner_inventory']['physical_operator_inventory_accepted'];gc=q['cycle_recovery']
  if gc:
   assert q['explicit_collection_performed'] and gc['collected_objects']>0 and gc['live_input_preserved_bitwise'] and gc['full_action_preserved_bitwise'] and gc['gc_enabled_before']==gc['gc_enabled_after'] and gc['owned_high_water_after_bytes']>=gc['owned_high_water_before_bytes']
  else:assert not q['explicit_collection_performed']
  records['collection' if gc else 'none']=dict(admission=a,wall_s=r['wall_s'],owned_peak_bytes=row['peak_rss_bytes'],sampled_peak_bytes=r['peak_observed_rss_bytes'])
 assert len(records)==2
 a,b=records['none']['admission'],records['collection']['admission'];assert a['pressure']['live_input_sha256']==b['pressure']['live_input_sha256']
 for k in ('factor_storage_bytes','numeric_workspace_bytes','outer_basis_reservation_bytes','coarse_pressure_reservation_bytes','reserve_bytes'):assert a[k]==b[k]
 drop=a['current_rss_before_numeric_bytes']-b['current_rss_before_numeric_bytes'];passed=drop>=32*2**20 and b['numeric_stage_admitted'];assert not passed
 sources=list((R/'scripts').glob('*cycle_recovery*.py'))+[R/'tests/test_cfd_reference3d_cycle_recovery.py']+list((R/'docs').glob('cfd_3d_cycle_recovery*.md'))
 result=dict(status='PROGRESS: exact full-GC control rejected below useful recovery; lossless coefficient catalogue next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in sources},receipt_sha256=receipts,native_hashes_preserved=native,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),paired_fresh_rss_reduction_bytes=drop,stage_recovery_gate_passed=passed,optional_collection_adopted=False,records=records,numeric_factor_attempted=False,numerical_field_published=False,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),rss_drop_mib=drop/2**20,stage_gate_passed=passed)))
if __name__=='__main__':main()
