"""Once-only same-math native action benchmark cost rejection."""
import hashlib,json
from pathlib import Path
import numpy as np
from cfd_reference3d_packed_inner8_control import control_reserve
from cfd_reference3d_packed_inner8_factor import work_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-packed-inner8'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='7d24e2f4f0d0bf090bbe1028bf0451ab42f91f5e8872b47af6564e5d9f16410e'
    changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
    s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==5 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/factor.dylib')==s['library_sha256']
    for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/p)
    assert sha(D/'transforms.json')==s['transforms_sha256']
    for t in json.loads((D/'transforms.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
        v+=t.get('append','');assert v==(R/t['output']).read_text()
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
    sel=x['selection'];ratio=float(np.median(x['paired_batch_seconds']['native'])/np.median(x['paired_batch_seconds']['baseline']));assert sel==e['selection'] and ratio==sel['time_ratio'];gate=ratio<=.95 and sel['max_relative_action_difference']<=1e-10 and sel['min_positive_work']>0;assert gate==sel['eligibility_gate_passed']==e['full_trial_permitted']==True
    nv=f['local_factor']['solve_workspace_bytes']//32;np_=1248;nc=f['coarse_factor']['dofs'];total=basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc)+control_reserve(nv,nc);assert total==x['reserved_work_bytes']
    for a in x['admissions']:assert a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))+total
    a=next(v for v in r['progress'] if v.get('phase')=='fill1_symbolic_preflight');assert a['complete_work_reservation_bytes']==total and a['estimated_stage_bytes']==a['current_rss_bytes']+a['construction_workspace_bound_bytes']+32*2**20+total<=1800*2**20
    with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
        loads=z['load_vectors'];a=z['baseline_responses'];b=z['native_responses'];assert storage_hash(loads)==x['loads_sha256'];assert max(np.linalg.norm(b-a,axis=0)/np.linalg.norm(a,axis=0))<=1e-10 and min(np.sum(loads*b,axis=0))>0 and 'velocity_coefficients' not in z.files
    from audit_cfd_3d_spatial import verify_receipt
    eligibility=json.loads((D/'small-eligibility.json').read_text());fp=Path(eligibility['receipt']);assert sha(fp)==eligibility['receipt_sha256'];fr,row=verify_receipt(fp.resolve())
    assert row['target']==1e-10 and row['retained_target']==1e-11 and row['outer_iteration']['restart']==30 and row['final_residual']['true_residual']<=1e-10 and fr['wall_s']<=23.5018023327 and eligibility['matched_trial_permitted']
    assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256'] and row['iterations']==113
    diff={k:abs(row[k][0]/old[k][0]-1) if isinstance(row[k],list) else abs(row[k]/old[k]-1) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w')};assert diff==eligibility['force_scalar_equivalence'] and max(diff.values())<1e-7
    for q,h in fr['source_sha256'].items():assert sha(R/'scripts'/q)==h
    assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
    pc=row['pressure_preconditioner'];assert pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5 and pc['columns']==10 and pc['constant_pressure_direction_retained']
    a=next(a for a in fr['progress'] if a.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['basis_reservation_bytes']==basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc) and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
    files=list(R.glob('scripts/*packed_inner8*'))+[R/'tests/test_cfd_reference3d_packed_inner8.py',R/'docs/cfd_3d_packed_inner8_goal.md',R/'docs/cfd_3d_packed_inner8_checkpoint.md'];out.write_text(json.dumps(dict(status='STRICT SMALL FIELD AND COST PASS: matched cube trial earned',persistent_goal_complete=False,full_trial_permitted=True,matched_trial_permitted=True,finer_trial_permitted=False,physical_accuracy_certified=False,tests_passed=5,source_sha256={str(q.relative_to(R)):sha(q) for q in files},control_receipt=str(p),control_receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],selection=sel,full_receipt=str(fp),full_receipt_sha256=sha(fp),full_residual=row['final_residual'],whole_wall_s=fr['wall_s'],force_scalar_equivalence=diff,control_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
def storage_hash(a):
    from cfd_reference3d_shared_factor import storage_sha
    return storage_sha(a)
if __name__=='__main__':main()
