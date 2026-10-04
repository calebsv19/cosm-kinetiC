"""Seal one matched packed-velocity control with unchanged physical/resource gates."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-packed-matched'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists()
 pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='d75debbdc57fa2dedeb7cb2a3191afc0c24b62c0731994d4fd11421023a5f08c'
 changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==2 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
 t=json.loads((D/'runner-transform.json').read_text());assert sha(D/'runner-transform.json')==s['runner_transform_sha256'] and sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert v==(R/t['output']).read_text()
 p=next((D/'runs').glob('*/L4-body6-second-normal-packed-inner8-receipt.json'));r=json.loads(p.read_text())
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 output=Path(r['command'][r['command'].index('--output')+1]);row=json.loads(output.read_text()) if output.exists() else {}
 oldp=next((R/'build/c3d-force-resume/runs').glob('*/L4-body6-second-normal-complement10-receipt.json'));oldr,old=verify_receipt(oldp.resolve())
 strict=bool(r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and row.get('numerically_accepted'))
 diff={};saving=None;candidate=None
 if strict:
  verify_receipt(p.resolve());assert row['target']==1e-10 and row['retained_target']==1e-11 and row['outer_iteration']['restart']==30 and row['final_residual']['true_residual']<=1e-10
  assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
  diff={k:abs(row[k][0]/old[k][0]-1) if isinstance(row[k],list) else abs(row[k]/old[k]-1) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w')};assert max(diff.values())<1e-7
  assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
  pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['constant_pressure_direction_retained'] and pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5
  f=row['preconditioner'];candidate=f['factor_storage_bytes']+f['distributed_work_reservation_bytes'];saving=873198472-candidate
  assert row['peak_rss_bytes']<=1800*2**20 and r['peak_observed_rss_bytes']<=1800*2**20 and r['wall_s']<=180
 useful=bool(strict and r['wall_s']<=106.642085 and saving>=300*2**20)
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 e=dict(receipt=str(p),receipt_sha256=sha(p),strict_field_passed=strict,finer_trial_permitted=useful,whole_wall_s=r['wall_s'],whole_wall_limit_s=106.642085,complete_velocity_factor_work_bytes=candidate,saving_bytes=saving,minimum_saving_bytes=300*2**20,force_scalar_equivalence=diff,physical_accuracy_certified=False)
 assert not (D/'matched-eligibility.json').exists();(D/'matched-eligibility.json').write_text(json.dumps(e,indent=2)+'\n')
 files=list(s['source_sha256'])+['scripts/audit_cfd_3d_packed_matched.py','docs/cfd_3d_packed_matched_checkpoint.md']
 out.write_text(json.dumps(dict(status='USEFUL MATCHED FIELD: finer force trial earned' if useful else 'MATCHED CONTROL REJECTED: retain measurements and no finer trial',persistent_goal_complete=False,tests_passed=2,source_sha256={q:sha(R/q) for q in files},predecessor_sha256=pre['sha256'],eligibility=e,receipt_sha256=sha(p),diagnostic_failure=r['diagnostic_failure'],final_residual=row.get('final_residual'),iterations=row.get('iterations'),owned_peak_mib=row.get('peak_rss_bytes',0)/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
