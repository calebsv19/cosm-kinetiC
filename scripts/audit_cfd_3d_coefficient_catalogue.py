import json,hashlib
from pathlib import Path
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-coefficient-catalogue';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='8763919c535e3a19d640ffa40cdf2be5a38d4552db650c9ceac5f2de54c3f06d'
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
 p=next((D/'representation-runs').glob('*/*-receipt.json'));r,row=parent.frozen(p);assert r['returncode']==2 and r['stop_reason'] is None and r['diagnostic_failure']['kind']=='resource_cap'
 assert not row['diagnostic_accepted'] and not row['numeric_factor_attempted'] and not row['symbolic_factor_attempted'] and not row['numerical_field_published'] and not row['numerically_accepted'] and not row['physical_accuracy_certified'] and not Path(r['command'][r['command'].index('--catalogue')+1]).exists()
 g=row['resource_phase_rejected'];assert g['phase']=='catalogue_merge' and g['reserve_bytes']==32*2**20 and g['estimated_phase_bytes']==g['current_rss_bytes']+g['projected_workspace_bytes']+g['reserve_bytes']>g['rss_cap_bytes']==1800*2**20 and g['wall_cap_s']==180
 assert r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*2**20 and not any(q.get('phase') in ('workspace_symbolic_ready','workspace_numeric_ready','solve','publication_complete') for q in r['progress'])
 pp=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'));_,previous=verify_receipt(pp);assembly=next(q for q in r['progress'] if q.get('phase')=='assembled');assert assembly['identity']==previous['identity']
 n=assembly['block_storage']['stored_dense_block_value_count'];lower=max(0,(g['projected_workspace_bytes']-24*262144+39)//40);upper_saved=4*n-8*lower;assert n==22411620 and lower==9401044 and upper_saved<32*2**20
 sources=list((R/'scripts').glob('*coefficient_catalogue*.py'))+[R/'tests/test_cfd_reference3d_coefficient_catalogue.py']+list((R/'docs').glob('cfd_3d_coefficient_catalogue*.md'))
 result=dict(status='PROGRESS: full-word catalogue resource stop and negative storage bound; shared predictor exact-word correction next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in sources},receipt_sha256={str(p):sha(p),str(pp):sha(pp)},native_hashes_preserved=native,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),minimum_unique_words_already_encountered=lower,maximum_possible_full_catalogue_saving_bytes=upper_saved,storage_potential_accepted=False,numeric_factor_attempted=False,numerical_field_published=False,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),upper_saved_mib=upper_saved/2**20)))
if __name__=='__main__':main()
