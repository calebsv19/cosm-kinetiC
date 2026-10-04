#!/usr/bin/env python3
"""Audit exact caller-owned factor controls and withheld normal admission."""
import json,hashlib
from pathlib import Path
from audit_cfd_3d_graded import sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-workspace-cholesky'

def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-hierarchical/checkpoint-audit.json'
    for p,h in json.loads(predecessor.read_text())['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['test_count']==6
    assert sha(DATA/'support-tests.log')==tests['log_sha256']
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    for name,key in (('factor.dylib','library_sha256'),('factor-build.json','build_record_sha256'),('Solve.h','sdk_header_sha256'),('api-receipt.json','api_receipt_sha256')):assert sha(DATA/'support'/name)==tests[key]
    stage_tests=json.loads((DATA/'stage-support-test-receipt.json').read_text());assert stage_tests['test_count']==2
    assert sha(DATA/'stage-support-tests.log')==stage_tests['log_sha256']
    for p,h in stage_tests['source_sha256'].items():assert sha(ROOT/p)==h
    costs={};equivalence={};differences={};hashes={};matched={}
    cases=(('original-L4-workspace','c3d-cholesky','original-adopted-defaults'),('L4-body6-base-workspace','c3d-shared-factor','L4-body6-base-shared'))
    for name,oldfolder,oldname in cases:
        path=next((DATA/'runs').glob('*/'+name+'-receipt.json'));receipt,row=verify_receipt(path)
        assert receipt['returncode']==0 and row['numerically_accepted'] and row['linear_solve_accepted'] and not row['physical_accuracy_certified']
        for source,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/source)==h
        assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_workspace_cholesky.c']
        pc=row['preconditioner'];assert pc['kind']=='workspace_coupled_cholesky' and pc['scaling']=='none' and pc['user_factor_storage_verified']
        assert pc['shared_input_preserved_after_factor'] and pc['shared_input_preserved_after_solve'] and pc['numeric_workspace_retained_bytes']==0
        assert pc['pressure_control']['live_input_preserved'] and pc['pressure_control']['action_preserved']
        assert pc['pressure_control']['owned_high_water_after_bytes']>=pc['pressure_control']['owned_high_water_before_bytes']
        oldpath=next((ROOT/'build'/oldfolder/'runs').glob('*/'+oldname+'-receipt.json'));oldreceipt,old=verify_receipt(oldpath)
        for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==old['identity'][key]
        assert row['chunk_size']==old['chunk_size']==128
        comparison=force_comparison(row,old)
        assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
        equivalence[name]=comparison;differences[name]=fields(path.with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
        matched[name]=dict(owned_memory_reduction=1-row['peak_rss_bytes']/old['peak_rss_bytes'],total_time_ratio=receipt['wall_s']/oldreceipt['wall_s'],
            comparison_scope='same-mesh accepted prior exact path; original comparator predates shared storage, base comparator uses shared storage')
        costs[name]=dict(wall_s=receipt['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],sampled_peak_rss_bytes=receipt['peak_observed_rss_bytes'],iterations=row['iterations'],
            full_residual=row['final_residual'],timings=row['timings'],resource_samples=row['resource_samples'],preconditioner=pc)
        hashes[str(path)]=sha(path)
    path=next((DATA/'stage-runs').glob('*/L4-body6-normal-workspace-stage-receipt.json'));receipt=json.loads(path.read_text())
    assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180 and receipt['linear_iteration_cap']==3000
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==h and sha(ROOT/'scripts'/n)==h
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    diagnostic=json.loads(path.with_name('L4-body6-normal-workspace-stage.json').read_text())
    assert diagnostic['diagnostic_accepted'] and not diagnostic['numerically_accepted'] and not diagnostic['numerical_field_published'] and not diagnostic['numeric_factor_attempted']
    assert diagnostic['symbolic_handle_cleanup_verified'] and diagnostic['original_mixed_input_preserved'] and not path.with_name('L4-body6-normal-workspace-stage.npz').exists()
    assert diagnostic['tetrahedra']==23616 and diagnostic['peak_rss_bytes']<1800*1024**2 and receipt['wall_s']<180
    budget=diagnostic['admission'];assert budget['reserve_bytes']==32*1024**2
    assert budget['estimated_numeric_stage_bytes']==sum(budget[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))
    assert not budget['numeric_stage_admitted'] and budget['estimated_numeric_stage_bytes']>1800*1024**2
    assert budget['factor_storage_bytes']==1325505336 and budget['numeric_workspace_bytes']==26931968
    prior=next((ROOT/'build/c3d-shared-factor/runs').glob('*/L4-body6-normal-shared-receipt.json'));failed,_=verify_receipt(prior)
    identity=next(p['identity'] for p in failed['progress'] if p.get('phase')=='assembled')
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert diagnostic['identity'][key]==identity[key]
    audit=dict(schema='physics_sim_c3d_workspace_cholesky_audit_v1',status='exact_caller_owned_factor_base_accepted_normal_budget_withheld',persistent_goal_complete=False,stage_1_complete=False,
        physical_accuracy_certified=False,optional_exact_workspace_path_adopted=True,adoption_scope='measured original and count6 base only',finer_normal_admitted=False,force_convergence_testing_resumed_on_normal=False,
        native_source_unchanged=True,reference_receipts_sha256=hashes,stage_receipts_sha256={str(path):sha(path)},costs=costs,equivalence=equivalence,field_differences=differences,
        matched_cost=matched,normal_stage_admission=budget,test_count=8,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),stage_test_receipt_sha256=sha(DATA/'stage-support-test-receipt.json'),
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),changed_preexisting_files=changed,
        next_gate='complete three-component block velocity representation shared by exact operator and caller-owned factor; measured stage budget before normal numeric admission')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps({k:audit[k] for k in ('status','matched_cost','normal_stage_admission')},indent=2))

if __name__=='__main__':main()
