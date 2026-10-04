"""Once-only sampled pressure diagnostic audit; no physical certification."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_pressure_modes_evidence import verify,sha,R,D
from audit_cfd_3d_spatial import verify_receipt

def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='249aa382b12d9f1e3ab8b2dd5b732937ffbf89db6bc8bd827d4ec50cb6e42357'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4 and sha(D/support['log'])==support['log_sha256']
 for p,h in support['source_sha256'].items():
  f=Path(support['frozen_source'])/Path(p).name;assert sha(f)==h
  if p=='scripts/cfd_reference3d_pressure_modes.py':assert sha(R/'scripts/cfd_reference3d_pressure_coverage.py')==h
  elif p=='tests/test_cfd_reference3d_pressure_modes.py':assert f.read_text().replace('scripts.cfd_reference3d_pressure_modes','scripts.cfd_reference3d_pressure_coverage')==(R/'tests/test_cfd_reference3d_pressure_coverage.py').read_text()
  else:assert sha(R/p)==h
 t=json.loads((D/'source-transform-control.json').read_text());assert sha(D/'source-transform-control.json')==support['transformation_sha256'] and sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text().split(t['prefix_end'])[0]
 for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
 original=s+t['diagnostic_suffix'];assert hashlib.sha256(original.encode()).hexdigest()==t['output_sha256']
 assert original.replace('from cfd_reference3d_pressure_modes import','from cfd_reference3d_pressure_coverage import')==(R/t['output']).read_text()
 restoration=json.loads((D/'naming-restoration.json').read_text())
 for p,h in restoration['restored_sha256'].items():assert sha(R/p)==h==baseline[p]
 rows={};receipts={}
 for case,oldroot,pattern in (('L4-body2','c3d-size-selected','L4-body2-original-selected-quadratic-receipt.json'),('L8-exact-target','c3d-exact-target','*-receipt.json')):
  p=next((D/'runs').glob('*/'+case+'-pressure-modes-receipt.json'));r,row,a=verify(p);op=next((R/'build'/oldroot/'runs').glob('*/'+pattern));orr,old=verify_receipt(op)
  assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256'];rows[case]=dict(diagnostic=row['diagnostic'],numeric_stage_admission=a,whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20);receipts[str(p)]=sha(p);receipts[str(op)]=sha(op)
 e=json.loads((D/'target-eligibility.json').read_text());assert e['target_diagnostic_eligible'] and e['attempts_declared']==1 and not e['physical_or_flow_acceptance_inferred']
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 files=list((R/'scripts').glob('*pressure_modes*.py'))+[R/'scripts/cfd_reference3d_pressure_coverage.py',R/'tests/test_cfd_reference3d_pressure_coverage.py']+list((R/'docs').glob('cfd_3d_pressure_modes*.md'))
 audit=dict(status='PROGRESS: exact sampled Schur coverage/conditioning completed; complementary pressure scaling next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,complete_spectrum_certified=False,flow_field_published=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256=receipts,native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),observations=rows,naming_restoration_sha256=sha(D/'naming-restoration.json'))
 out.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out))))
if __name__=='__main__':main()
