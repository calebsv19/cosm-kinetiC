import hashlib,json
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-local-cost-split'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='fb8184bc953abb668b80764766c0aad888439aefeaaa1c49de6df5ab5a982f86'
 changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/factor.dylib')==s['library_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
 for p,h in s['transform_sha256'].items():assert sha(D/p)==h
 t=json.loads((D/'native-transform.json').read_text());assert (R/t['output']).read_text()==(R/t['parent']).read_text()+t['appended_function'] and sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256']
 t=json.loads((D/'runner-transform.json').read_text());v=(R/t['parent']).read_text()
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert v==(R/t['output']).read_text() and sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256']
 p=next((D/'runs').glob('*/*-receipt.json'));r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and row['diagnostic_factor_owners_retired'] and not row['flow_field_published'] and not row['numerically_accepted']
 oldp=next((R/'build/c3d-force-resume/runs').glob('*/*-receipt.json'));oldr=json.loads(oldp.read_text());old=json.loads(Path(oldr['command'][oldr['command'].index('--output')+1]).read_text());assert next(x for x in r['progress'] if x.get('phase')=='assembled')['identity']==old['identity'] and row['factor']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
 from cfd_reference3d_flexible import basis_reservation
 from cfd_reference3d_pressure_complement10 import reserve
 from cfd_reference3d_packed_inner8_factor import work_reserve
 from cfd_reference3d_packed_matched_profile import diagnostic_reserve
 f=row['factor'];nv=f['local_factor']['solve_workspace_bytes']//32;np_=7104;total=basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,f['coarse_factor']['dofs'])+diagnostic_reserve(nv+np_)+2**20;a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['basis_reservation_bytes']==total and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
 x=row['profile']['native_local_split'];assert x['timed_calls']==14 and x['baseline_original_rhs_bitwise_match'] and len(x['samples'])==14
 assert x['totals']=={k:sum(s[k] for s in x['samples']) for k in x['samples'][0]} and x['totals']['iterations']==112 and x['totals']['packed_solve_calls']==112 and x['totals']['physical_action_calls']==112 and x['totals']['native_total_s']>=x['totals']['physical_action_s']+x['totals']['packed_solve_s']
 with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:assert z.files==['per_load_seconds']
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 files=list(s['source_sha256'])+['scripts/audit_cfd_3d_local_cost_split.py','docs/cfd_3d_local_cost_split_checkpoint.md'];out.write_text(json.dumps(dict(status='LOCAL COST SPLIT COMPLETE: physical action dominates; no finer field',persistent_goal_complete=False,tests_passed=4,source_sha256={q:sha(R/q) for q in files},receipt=str(p),receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],native_local_split=x,wall_s=r['wall_s'],owned_peak_mib=row['owned_peak_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
