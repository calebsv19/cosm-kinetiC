import json,hashlib
from pathlib import Path
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_coarse import reserve
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-residency';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='b260e538503e72fd827eba18156bc175a861fa35364cd339463259304908e00a'
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 for support_name,count,transform_name in [('support-test-receipt.json',4,'source-transform-control.json'),('correction-support-receipt.json',5,'correction-transform-control.json')]:
  support=json.loads((D/support_name).read_text());assert support['passed'] and support['test_count']==count
  for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
  assert sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text() and sha(D/transform_name)==support['transformation_sha256']
  for t in json.loads((D/transform_name).read_text()):
   assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
   for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
   assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 support=json.loads((D/'support-test-receipt.json').read_text());assert sha(D/'support-tests-01.log')==support['initial_failure_log_sha256']
 pp=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'));_,previous=verify_receipt(pp);records={};receipts={str(pp):sha(pp)}
 for p in sorted((D/'stage-runs').glob('*/*-receipt.json')):
  r,row=parent.frozen(p);receipts[str(p)]=sha(p)
  assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*2**20
  assert row['diagnostic_accepted'] and not row['numeric_factor_attempted'] and not row['numerical_field_published'] and not row['numerically_accepted'] and not row['physical_accuracy_certified']
  assert row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved'] and row['identity']==previous['identity'] and row['tetrahedra']==33216 and not p.with_name(p.name.removesuffix('-receipt.json')+'.npz').exists()
  a=row['admission'];m=row['condensation'];n=m['reduced_free_dofs'];np_=m['condensed_pressure_dofs'];assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
  assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*2**20)
  assert a['earlier_post_relief_estimate_bytes']==a['estimated_numeric_stage_bytes']-a['current_rss_before_numeric_bytes']+a['earlier_post_relief_rss_bytes']
  q=a['pressure'];assert q['live_input_preserved'] and q['action_preserved'] and q['action_max_absolute_change']==0 and not q['explicit_collection_performed']
  c=q['owner_inventory'];corrected='-owners-' in p.name
  if corrected:assert c['physical_operator_inventory_accepted'] and c['groups']['physical_mixed']['logical_array_bytes']>=c['minimum_physical_mixed_logical_bytes']==m['block_storage']['stored_array_bytes']+8*np_
  else:assert c['groups']['physical_mixed']['unique_arrays']==0 and c['groups']['physical_mixed']['logical_array_bytes']==0
  assert c['known_unique_backing_bytes']==sum(v['attributed_backing_bytes'] for v in c['groups'].values()) and c['known_unique_backing_owners']==sum(v['attributed_backing_owners'] for v in c['groups'].values())
  records[p.name]=dict(inventory_accepted=corrected,admission=a,whole_wall_s=r['wall_s'],owned_peak_bytes=row['peak_rss_bytes'],sampled_peak_bytes=r['peak_observed_rss_bytes'])
 assert len(records)==2 and sum(v['inventory_accepted'] for v in records.values())==1
 sources=list((R/'scripts').glob('*residency*.py'))+list((R/'tests').glob('test_cfd_reference3d_residency*.py'))+list((R/'docs').glob('cfd_3d_residency*.md'))
 result=dict(status='PROGRESS: corrected live ownership and fresh symbolic admission measured; full cycle control next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=native,source_sha256={str(p.relative_to(R)):sha(p) for p in sources},receipt_sha256=receipts,support_receipt_sha256={p:sha(D/p) for p in ('support-test-receipt.json','correction-support-receipt.json')},records=records,numeric_factor_attempted=False,numerical_field_published=False,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out))))
if __name__=='__main__':main()
