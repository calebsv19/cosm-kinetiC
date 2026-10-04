"""Once-only same-storage tighter-fill diagnostic readback."""
import hashlib
import json
from pathlib import Path
import numpy as np
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_column_proxy import diagnostic_reserve
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_p3_cg8_scalar import work_reserve
R=Path(__file__).resolve().parents[1]
D=R/'build/c3d-tight-fill-bound'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=D/'checkpoint-audit.json';assert not out.exists()
    pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='783496d444289a7dc7faa8274787dc7324c20cdfc269842a64c9c7d97d14eac3'
    changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
    s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/factor.dylib')==s['library_sha256']
    for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
    assert sha(D/'transforms.json')==s['transforms_sha256']
    for t in json.loads((D/'transforms.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256']
        v=(R/t['parent']).read_text()
        if t.get('prefix_preserved'):
            prefix,tail=v.split('void *cfd_fill1_factor_create',1)
            for a,b in t['tail_replacements']:assert a in tail;tail=tail.replace(a,b)
            v=prefix+t['helper']+'void *cfd_fill1_factor_create'+tail
        else:
            for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
        assert v==(R/t['output']).read_text()
    native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for p,h in native.items():assert sha(R/p)==h
    e=json.loads((D/'control-eligibility.json').read_text());p=Path(e['control_receipt']);assert sha(p)==e['control_receipt_sha256'];r=json.loads(p.read_text())
    assert r['schema']=='physics_sim_c3d_pressure_column_diagnostic_receipt_v1' and r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
    assert r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['mesh_cap']==50000 and r['linear_iteration_cap']==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    assert r['coarse_factor_build']['create_abi_arguments']==7 and r['double_factor_build']['create_abi_arguments']==6
    x=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert x['diagnostic_accepted'] and not x['numerically_accepted'] and not x['flow_field_published'] and x['diagnostic_factor_owners_retired'] and x['original_inputs_preserved'] and x['pressure_constant_kept']
    op=Path(e['baseline_receipt']);assert sha(op)==e['baseline_receipt_sha256'];orr=json.loads(op.read_text());old=json.loads(Path(orr['command'][orr['command'].index('--output')+1]).read_text());assert x['identity']==old['identity']
    f=x['factors']['p3'];lf=f['local_factor'];oldlf=old['factors']['p3']['local_factor']
    assert lf['pattern']==oldlf['pattern'] and lf['factor_storage_bytes']==oldlf['factor_storage_bytes']==39917400 and lf['shifted_pivots']==0 and lf['rounded_values_sha256']==oldlf['rounded_values_sha256']
    ratios={k:x['columns']['fixed'][k]/old['columns']['fixed'][k] for k in ('schur_column_relative_error','projected_coarse_relative_error')};assert ratios==e['error_ratios'] and e['threshold']==.9 and not e['full_trial_permitted'] and max(ratios.values())>.9
    nv=lf['solve_workspace_bytes']//32;np_=1248;nc=f['coarse_factor']['dofs'];total=basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc)+diagnostic_reserve(nv,np_);assert total==x['reserved_work_bytes']
    for a in x['admissions']:assert a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))+total and a['reserve_bytes']==32*2**20
    assert x['columns']['fixed']['coarse_relative_skew']<1e-5 and min(x['columns']['fixed']['velocity_positive_work'])>0 and max(x['columns']['exact_double']['velocity_true_relative_residuals'])<1e-10
    with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
        assert 'velocity_coefficients' not in z.files and 'pressure_coefficients' not in z.files;Z=z['pressure_basis'];Wr=z['pressure_schur_exact_double']
        for name,m in x['columns'].items():
            W=z['pressure_schur_'+name];assert np.isclose(np.linalg.norm(W-Wr)/np.linalg.norm(Wr),m['schur_column_relative_error'],rtol=1e-10,atol=1e-14)
    assert not (D/'runs').exists()
    files=list(R.glob('scripts/*tight_fill_bound*'))+[R/'tests/test_cfd_reference3d_tight_fill_bound.py',R/'docs/cfd_3d_tight_fill_bound_goal.md',R/'docs/cfd_3d_tight_fill_bound_checkpoint.md']
    out.write_text(json.dumps(dict(status='DIAGNOSTIC PASS, USEFULNESS REJECTED: compensation and pressure errors essentially unchanged',persistent_goal_complete=False,full_trial_permitted=False,large_or_finer_trial_permitted=False,physical_accuracy_certified=False,tests_passed=4,source_sha256={str(q.relative_to(R)):sha(q) for q in files},control_receipt=str(p),control_receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],selection=e,whole_wall_s=r['wall_s'],owned_peak_mib=x['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,compensation_relative_reduction=1-lf['fill_diagonal_compensation_sum']/oldlf['fill_diagonal_compensation_sum'],native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
