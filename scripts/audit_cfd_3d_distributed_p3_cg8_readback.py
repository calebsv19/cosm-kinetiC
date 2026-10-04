"""Once-only CG8 pressure coarse setup rejection readback."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_distributed_p3_cg8 import fresh_admission,work_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-distributed-p3-cg8'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='40916d5cffe99d983f6e48d95c910da7a597834bc80a200757a44c6314048d8f'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==10 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/tests-nine.log')==s['initial_test_log_sha256'] and sha(Path(s['reused_P3_support_path']))==s['reused_P3_support_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 t=json.loads((D/'factor-transform-control.json').read_text());assert sha(D/'factor-transform-control.json')==s['factor_transform_sha256'];x=(R/t['parent']).read_text();assert sha(R/t['parent'])==t['parent_sha256']
 for a,b in t['literal_replacements']:assert a in x;x=x.replace(a,b)
 assert x==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 p=next((D/'runs').glob('*/*-receipt.json')).resolve();r,row=verify_receipt(p);assert row is None and r['returncode']==1 and r['stop_reason'] is None and r['diagnostic_failure']['kind']=='child_error'
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h
 stem=p.name.removesuffix('-receipt.json');log=p.parent/(stem+'.log');assert 'coarse Schur asymmetry exceeds declared gate' in log.read_text();assert not any('iteration' in x for x in r['progress'])
 a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assembled=next(x for x in r['progress'] if x.get('phase')=='assembled');np_=assembled['condensed_pressure_dofs'];nv=assembled['reduced_free_dofs']-np_;nc=assembled['distributed_p3']['coarse_dofs'];assert a['numeric_stage_admitted'] and a['outer_basis_reservation_bytes']==basis_reservation(nv+np_,30) and a['coarse_pressure_reservation_bytes']==reserve(nv,np_) and a['distributed_velocity_work_reservation_bytes']==work_reserve(nv,nc)
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
 files=list((R/'scripts').glob('*distributed_p3_cg8*'))+list((R/'tests').glob('*distributed_p3_cg8*'))+list((R/'docs').glob('cfd_3d_distributed_p3_cg8*.md'))
 out.write_text(json.dumps(dict(status='REJECTED: nonlinear inner CG pressure coarse symmetry',persistent_goal_complete=False,numerical_field_published=False,large_or_finer_trial_permitted=False,tests_passed=10,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),whole_wall_s=r['wall_s'],sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,numeric_stage_admission=a,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
