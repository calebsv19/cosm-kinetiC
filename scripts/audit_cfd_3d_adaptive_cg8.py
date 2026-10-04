"""Once-only readback of the adaptive inner-PC accuracy rejection."""
import hashlib
import json
from pathlib import Path
import numpy as np
from cfd_reference3d_energy_cg8 import diagnostic_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_p3_cg8_scalar import work_reserve
R = Path(__file__).resolve().parents[1]
D = R / 'build/c3d-adaptive-cg8'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out = D / 'checkpoint-audit.json'
    assert not out.exists()
    pre = json.loads((D / 'predecessor.json').read_text())
    assert sha(Path(pre['path'])) == pre['sha256'] == 'a69b4c53afb0e97545dc20baf7ee6517cb13c1839b2ab63d03e337616c7c1927'
    changed = [p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p) != h]
    assert set(changed) <= {'docs/current_truth.md', 'docs/README.md'}
    support = json.loads((D/'support/receipt.json').read_text())
    assert support['tests_passed'] == 4 and sha(D/'support/tests.log') == support['test_log_sha256']
    for p,h in support['source_sha256'].items():
        assert sha(R/p) == h == sha(D/'support/frozen'/p)
    for name,h in support['transform_sha256'].items():
        p = D/name
        assert sha(p) == h
        t = json.loads(p.read_text()); v = (R/t['parent']).read_text()
        for a,b in t['literal_replacements']:
            assert a in v; v = v.replace(a,b)
        assert v == (R/t['output']).read_text() and sha(R/t['output']) == t['output_sha256']
    native = json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for p,h in native.items():
        assert sha(R/p) == h
    p = next((D/'control-runs').glob('*/*-receipt.json'))
    r = json.loads(p.read_text())
    assert r['schema'] == 'physics_sim_c3d_pressure_column_diagnostic_receipt_v1'
    assert r['returncode'] == 0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
    assert r['rss_cap_bytes'] == 1800*2**20 and r['wall_cap_s'] == 180 and r['linear_iteration_cap'] == 3000 and r['mesh_cap'] == 50000
    for q,h in r['source_sha256'].items():
        assert sha(R/'scripts'/q) == h == sha(p.parent/'source'/q)
    for q,h in r['artifact_sha256'].items():
        assert sha(Path(q)) == h
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py')) == r['runner_sha256']
    assert r['coarse_factor_build']['create_abi_arguments'] == 7 and r['double_factor_build']['create_abi_arguments'] == 6
    x = json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text())
    assert x['diagnostic_accepted'] and not x['numerically_accepted'] and not x['flow_field_published'] and x['diagnostic_factor_owners_retired'] and x['original_inputs_preserved'] and x['pressure_constant_kept']
    oldp = next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'))
    oldr = json.loads(oldp.read_text()); old = json.loads(Path(oldr['command'][oldr['command'].index('--output')+1]).read_text())
    for k,v in x['identity'].items():
        assert v == old['identity'][k]
    pc = x['factors']['p3']
    assert pc['rounded_values_sha256'] == old['preconditioner']['rounded_values_sha256']
    nv = pc['local_factor']['solve_workspace_bytes']//32; np_ = 1248; nc = pc['coarse_factor']['dofs']
    total = basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc)+diagnostic_reserve(nv,np_)
    assert total == x['reserved_work_bytes']
    for a in x['admissions']:
        assert a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes'] == sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))+total
    sel = x['selection']; ow = sel['baseline_work']; nw = sel['adaptive_work']; ratios = sel['energy_error_ratios']
    gate = nw['factor_applications'] <= .75*ow['factor_applications'] and nw['velocity_applications'] <= ow['velocity_applications'] and max(ratios) <= 1.25
    assert not gate and sel['eligibility_gate_passed'] == gate
    assert ow == {'factor_applications':80,'velocity_applications':80}
    traces = x['adaptive_proxy_traces']; steps = [v[-1]['step'] for v in traces]
    assert len(traces) == 10 and all(0 < k < 8 for k in steps) and sum(steps) == nw['steps'] == 48
    assert nw['factor_applications'] == nw['steps']+10 == 58 and nw['velocity_applications'] == 48 and nw['early_returns'] == 10
    assert all(len(v) <= 8 and v[-1]['relative_preconditioned_residual'] <= .25 for v in traces)
    with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
        assert 'velocity_coefficients' not in z.files and 'pressure_coefficients' not in z.files
        Q=z['coverage_basis_mass']; assert np.linalg.norm(Q.T@Q-np.eye(Q.shape[1])) < 1e-10
        ref=z['velocity_load_response_exact_double']
        for name in ('cg8','adaptive'):
            u=z['velocity_load_response_'+name]
            np.testing.assert_allclose(np.linalg.norm(u-ref,axis=0)/np.linalg.norm(ref,axis=0),x['columns'][name]['velocity_solution_relative_errors'],rtol=1e-10,atol=1e-14)
    assert not (D/'runs').exists()
    files=list(R.glob('scripts/*adaptive_cg8*'))+[R/'scripts/cfd_reference3d_energy_cg8.py',R/'tests/test_cfd_reference3d_adaptive_cg8.py',R/'docs/cfd_3d_adaptive_cg8_goal.md',R/'docs/cfd_3d_adaptive_cg8_checkpoint.md']
    out.write_text(json.dumps(dict(status='DIAGNOSTIC PASS, ACCURACY REJECTED: work saved but per-column energy error gate fails',persistent_goal_complete=False,full_trial_permitted=False,large_or_finer_trial_permitted=False,physical_accuracy_certified=False,tests_passed=4,source_sha256={str(q.relative_to(R)):sha(q) for q in files},control_receipt=str(p),control_receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],selection=sel,whole_wall_s=r['wall_s'],owned_peak_mib=x['peak_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n')
    print(sha(out))
if __name__ == '__main__':
    main()
