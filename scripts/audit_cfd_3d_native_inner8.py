"""Once-only same-math native action benchmark cost rejection."""
import hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_native_inner8_control import control_reserve
from cfd_reference3d_native_inner8_factor import work_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-native-inner8'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='6614946aa5bb2cba20525c92657054738ebac5a884a4d70f6fd0f9e94748cbd1'
    changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
    s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/initial-tests.log')==s['initial_log_sha256'] and sha(D/'support/initial-build-diagnostic.json')==s['initial_build_diagnostic_sha256'] and sha(D/'support/factor.dylib')==s['library_sha256']
    for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
    assert sha(D/'transforms.json')==s['transforms_sha256']
    for t in json.loads((D/'transforms.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
        v+=t['append'];assert v==(R/t['output']).read_text()
    native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for p,h in native.items():assert sha(R/p)==h
    e=json.loads((D/'control-eligibility.json').read_text());p=Path(e['control_receipt']);assert sha(p)==e['control_receipt_sha256'];r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert '-DACCELERATE_NEW_LAPACK' in r['factor_build']['command'] and '-DACCELERATE_LAPACK_ILP64' in r['factor_build']['command']
    x=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert x['diagnostic_accepted'] and not x['numerically_accepted'] and not x['flow_field_published'] and x['diagnostic_factor_owners_retired'] and x['original_inputs_preserved'] and x['load_count']==20 and x['batches']==3
    op=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'));orr=json.loads(op.read_text());old=json.loads(Path(orr['command'][orr['command'].index('--output')+1]).read_text())
    for k,v in x['identity'].items():assert v==old['identity'][k]
    f=x['factor'];assert f['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256'] and f['factor_storage_bytes']==64902436 and f['native_inner_scratch_vectors']==7 and f['native_inner_output_vectors']==1 and f['local_inner_iteration_cap']==8
    sel=x['selection'];ratio=float(np.median(x['paired_batch_seconds']['native'])/np.median(x['paired_batch_seconds']['baseline']));assert sel==e['selection'] and ratio==sel['time_ratio'];gate=ratio<=.95 and sel['max_relative_action_difference']<=1e-10 and sel['min_positive_work']>0;assert gate==sel['eligibility_gate_passed']==e['full_trial_permitted']==False
    nv=f['local_factor']['solve_workspace_bytes']//32;np_=1248;nc=f['coarse_factor']['dofs'];total=basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc)+control_reserve(nv,nc);assert total==x['reserved_work_bytes']
    for a in x['admissions']:assert a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))+total
    a=next(v for v in r['progress'] if v.get('phase')=='fill1_symbolic_preflight');assert a['complete_work_reservation_bytes']==total and a['estimated_stage_bytes']==a['current_rss_bytes']+a['construction_workspace_bound_bytes']+32*2**20+total<=1800*2**20
    with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
        loads=z['load_vectors'];a=z['baseline_responses'];b=z['native_responses'];assert storage_hash(loads)==x['loads_sha256'];assert max(np.linalg.norm(b-a,axis=0)/np.linalg.norm(a,axis=0))<=1e-10 and min(np.sum(loads*b,axis=0))>0 and 'velocity_coefficients' not in z.files
    assert not (D/'runs').exists();files=list(R.glob('scripts/*native_inner8*'))+[R/'tests/test_cfd_reference3d_native_inner8.py',R/'docs/cfd_3d_native_inner8_goal.md',R/'docs/cfd_3d_native_inner8_checkpoint.md'];out.write_text(json.dumps(dict(status='ACTION EQUIVALENCE PASS, USEFULNESS REJECTED: 2.32 percent faster misses prospective 5 percent',persistent_goal_complete=False,full_trial_permitted=False,large_or_finer_trial_permitted=False,physical_accuracy_certified=False,tests_passed=4,source_sha256={str(q.relative_to(R)):sha(q) for q in files},control_receipt=str(p),control_receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],selection=sel,whole_wall_s=r['wall_s'],owned_peak_mib=x['peak_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
def storage_hash(a):
    from cfd_reference3d_shared_factor import storage_sha
    return storage_sha(a)
if __name__=='__main__':main()
