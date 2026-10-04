"""Once-only exact matched diagnostic timing readback, never flow qualification."""
import hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_packed_matched_profile import diagnostic_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_packed_inner8_factor import work_reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-packed-matched-profile'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='d09a106e16efac538805c4cde011d782991fec52f9f8416804d58a420e22580d'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h]
 own='scripts/cfd_reference3d_packed_matched_profile.py';before=(R/own).read_text().replace('coarse.solve == solve and coarse.base_solve == base and coarse.action == action','coarse.solve is solve and coarse.base_solve is base and coarse.action is action');assert hashlib.sha256(before.encode()).hexdigest()==baseline[own]
 assert set(changed)<={'docs/current_truth.md','docs/README.md',own}
 # This own newly created source fix preceded all support/numerical measurements.
 assert own in json.loads((D/'support/receipt.json').read_text())['source_sha256']
 changed=[q for q in changed if q!=own]
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==3 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
 t=json.loads((D/'runner-transform.json').read_text());assert sha(D/'runner-transform.json')==s['runner_transform_sha256'] and sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert v==(R/t['output']).read_text()
 p=next((D/'runs').glob('*/L4-body6-second-normal-packed-profile-receipt.json'));r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and not row['numerically_accepted'] and not row['flow_field_published'] and row['diagnostic_factor_owners_retired'] and row['tetrahedra']==28416
 oldp=next((R/'build/c3d-force-resume/runs').glob('*/L4-body6-second-normal-complement10-receipt.json'));oldr=json.loads(oldp.read_text());old=json.loads(Path(oldr['command'][oldr['command'].index('--output')+1]).read_text());assembled=next(x for x in r['progress'] if x.get('phase')=='assembled');assert assembled['identity']==old['identity']
 f=row['factor'];assert f['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256'] and f['local_inner_iteration_cap']==8 and f['native_inner_scratch_vectors']==7 and f['local_factor']['shifted_pivots']==0 and f['factor_storage_bytes']==498235168 and f['distributed_work_reservation_bytes']==80385536
 pr=row['pressure_preconditioner'];assert pr['columns']==10 and pr['constant_pressure_direction_retained'] and pr['coarse_relative_skew']<1e-5 and pr['coarse_reproduction_relative_error']<1e-5
 a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');nv=f['local_factor']['solve_workspace_bytes']//32;np_=7104;nc=f['coarse_factor']['dofs'];total=basis_reservation(nv+np_,30)+diagnostic_reserve(nv+np_)+reserve(nv,np_)+work_reserve(nv,nc);assert a['numeric_stage_admitted'] and a['basis_reservation_bytes']==total and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
 a=next(x for x in r['progress'] if x.get('phase')=='fill1_symbolic_preflight');assert a['complete_work_reservation_bytes']==total and a['estimated_stage_bytes']==a['current_rss_bytes']+a['construction_workspace_bound_bytes']+32*2**20+total<=1800*2**20
 x=row['profile'];assert x['load_count']==7 and x['repeats']==2 and x['instrumentation_preserves_original_action'] and x['instrumentation_restored'] and x['call_counts']==dict(coarse_Float_solve=84,coarse_Double_action=56,coarse_correction=28,outer_velocity_action=28,local_inner8=14) and min(x['momentum_positive_work'])>0
 assert abs(x['total_preconditioner_s']-sum(sum(q['seconds']) for q in x['per_load']))<1e-12
 with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
  assert z.files==['per_load_seconds'];np.testing.assert_array_equal(z['per_load_seconds'],np.array([q['seconds'] for q in x['per_load']]))
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 files=list(s['source_sha256'])+['scripts/audit_cfd_3d_packed_matched_profile.py','scripts/audit_cfd_3d_packed_matched_profile_readback.py','docs/cfd_3d_packed_matched_profile_checkpoint.md']
 out.write_text(json.dumps(dict(status='MATCHED TIMING DIAGNOSTIC: no full field or finer eligibility',persistent_goal_complete=False,physical_accuracy_certified=False,tests_passed=3,source_sha256={q:sha(R/q) for q in files},predecessor_sha256=pre['sha256'],receipt=str(p),receipt_sha256=sha(p),profile=x,wall_s=r['wall_s'],owned_peak_mib=row['owned_peak_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
