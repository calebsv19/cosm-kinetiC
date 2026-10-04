"""Once-only streamed factor-informed ranking diagnostic audit."""
import hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_column_proxy import diagnostic_reserve
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_seed_fill1_factor import work_reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-seed-fill1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=D/'checkpoint-audit.json';assert not out.exists()
    pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='579c19554fcdc5273b2c42085c87b6d90a1b8d21ab59ef85b0a7e45398e707d4'
    changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
    s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==5 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/factor.dylib')==s['library_sha256']
    assert True
    for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
    assert sha(D/'transforms.json')==s['transforms_sha256']
    for t in json.loads((D/'transforms.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
        v+=t['append']
        assert v==(R/t['output']).read_text()
    native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for p,h in native.items():assert sha(R/p)==h
    e=json.loads((D/'control-eligibility.json').read_text());p=Path(e['control_receipt']);assert sha(p)==e['control_receipt_sha256'];r=json.loads(p.read_text())
    assert r['schema']=='physics_sim_c3d_pressure_column_diagnostic_receipt_v1' and r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
    assert r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['mesh_cap']==50000 and r['linear_iteration_cap']==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert r['coarse_factor_build']['create_abi_arguments']==7 and r['double_factor_build']['create_abi_arguments']==6
    x=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert x['diagnostic_accepted'] and not x['numerically_accepted'] and not x['flow_field_published'] and x['diagnostic_factor_owners_retired'] and x['original_inputs_preserved'] and x['pressure_constant_kept']
    op=Path(e['baseline_receipt']);assert sha(op)==e['baseline_receipt_sha256'];orr=json.loads(op.read_text());old=json.loads(Path(orr['command'][orr['command'].index('--output')+1]).read_text());assert x['identity']==old['identity']
    f=x['factors']['p3'];lf=f['local_factor'];oldlf=old['factors']['p3']['local_factor'];pattern=lf['pattern'];assert lf['factor_storage_bytes']==72*pattern['pattern_blocks']+96 and lf['factor_storage_bytes']<=72*pattern['pattern_block_cap']+1024 and lf['shifted_pivots']==0 and lf['rounded_values_sha256']==oldlf['rounded_values_sha256']==pattern['ranking_predictor_sha256']
    for k in ('original_blocks','pattern_block_cap','permutation_sha256'):assert pattern[k]==oldlf['pattern'][k]
    assert lf['seed_handle_retired_before_final_numeric'] and lf['seed_graph_inputs_retired_before_final_numeric'] and pattern['seed_factor_preserved'] and pattern['seed_factor_blocks']==oldlf['pattern']['pattern_blocks']
    assert lf['fill_diagonal_compensation_sum']<oldlf['fill_diagonal_compensation_sum']
    ratios={k:x['columns']['fixed'][k]/old['columns']['fixed'][k] for k in ('schur_column_relative_error','projected_coarse_relative_error')};vr=[a/b for a,b in zip(x['columns']['cg8']['velocity_solution_relative_errors'],old['columns']['cg8']['velocity_solution_relative_errors'])];assert ratios==e['error_ratios'] and vr==e['velocity_error_ratios'];gate=max(ratios.values())<=.9 and max(vr)<=1.05;assert gate==e['full_trial_permitted']==False and max(vr)<1
    nv=lf['solve_workspace_bytes']//32;np_=1248;nc=f['coarse_factor']['dofs'];total=basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc)+diagnostic_reserve(nv,np_);assert total==x['reserved_work_bytes']
    a=next(v for v in r['progress'] if v.get('phase')=='seed_graph_symbolic_preflight');assert a['complete_work_reservation_bytes']==total and a['estimated_stage_bytes']==a['current_rss_bytes']+a['construction_workspace_bound_bytes']+32*2**20+total<=1800*2**20 and a['construction_workspace_bound_bytes']<=384*2**20
    for a in x['admissions']:assert a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))+total
    assert x['columns']['fixed']['coarse_relative_skew']<1e-5 and max(x['columns']['exact_double']['velocity_true_relative_residuals'])<1e-10
    with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
        assert 'velocity_coefficients' not in z.files;Wr=z['pressure_schur_exact_double']
        for name,m in x['columns'].items():assert np.isclose(np.linalg.norm(z['pressure_schur_'+name]-Wr)/np.linalg.norm(Wr),m['schur_column_relative_error'],rtol=1e-10,atol=1e-14)
    assert len(x['admissions'])==4 and len([a for a in x['admissions'] if a['phase']=='p3_numeric_admission'])==2
    assert not (D/'runs').exists()
    files=list(R.glob('scripts/*seed_fill1*'))+[R/'tests/test_cfd_reference3d_seed_fill1.py',R/'docs/cfd_3d_seed_fill1_goal.md',R/'docs/cfd_3d_seed_fill1_checkpoint.md'];out.write_text(json.dumps(dict(status='DIAGNOSTIC PASS, USEFULNESS REJECTED: stronger velocity benefit but fixed-pressure error improvement below gate',persistent_goal_complete=False,full_trial_permitted=False,large_or_finer_trial_permitted=False,physical_accuracy_certified=False,tests_passed=5,source_sha256={str(q.relative_to(R)):sha(q) for q in files},control_receipt=str(p),control_receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],selection=e,whole_wall_s=r['wall_s'],owned_peak_mib=x['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,compensation_relative_reduction=1-lf['fill_diagonal_compensation_sum']/oldlf['fill_diagonal_compensation_sum'],native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
