#!/usr/bin/env python3
"""Complete vector storage equivalence, cost, admission and force-gate audit."""
import json,hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-sparse-load'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def frozen(path):
    receipt=json.loads(path.read_text())
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==h and sha(ROOT/'scripts'/n)==h
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    if 'factor_build' in receipt:
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_vector_storage.c']
        library=path.parent/'source/factor.dylib';build=path.parent/'source/factor-build.json'
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
    return receipt

def observer(path):
    receipt=frozen(path);assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    row=json.loads(path.with_name(path.name.removesuffix('-receipt.json')+'.json').read_text())
    assert row['diagnostic_accepted'] and row['input_complete_numerical_gates_passed'] and not row['physical_accuracy_certified']
    assert row['peak_rss_bytes']<1800*1024**2 and row['wall_s']<180
    assert sha(Path(row['input_receipt']))==row['input_receipt_sha256'];original=verify_receipt(Path(row['input_receipt']))[1]
    assert sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
    scores=np.array(row['equilibrium_indicator_squared_per_tet']);assert len(scores)==row['tetrahedra'] and np.all(np.isfinite(scores)) and np.all(scores>=0)
    assert abs(scores.sum()-sum(row['h_squared_volume_defect_centroid_buckets'])-sum(row['h_weighted_jump_defect_centroid_buckets']))<1e-12
    for lift,old in zip(row['lifts'],original['consistency_diagnostics']['volume_lifts']):
        assert abs(sum(lift[p]['weak_load_n'][0] for p in ('pressure','viscous'))-old['symmetric_stress_load_n'])<1e-9
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            assert np.max(np.abs(np.array(lift[part]['raw_surface_load_n'])-original[key]))<1e-10
            assert np.max(np.abs(lift[part]['identity_error_n']))<1e-9
    return dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=row['interior_stress_jump_l2'],
        maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for p in ('pressure','viscous') for v in lift[p]['identity_error_n']))

def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-vector-storage/checkpoint-audit.json';prior=json.loads(predecessor.read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in prior['stage_receipts_sha256'].items():
        assert sha(Path(p))==h
        for q,digest in json.loads(Path(p).read_text())['artifact_sha256'].items():assert sha(Path(q))==digest
    for p,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['test_count']==5
    log=DATA/'support-tests-01.log';assert sha(log)==tests['log_sha256'] and 'Ran 5 tests' in log.read_text() and '\nOK\n' in log.read_text()
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert tests['exact_vector_test_receipt_sha256']==sha(ROOT/'build/c3d-vector-storage/support-test-receipt.json')
    assert tests['library_sha256']==sha(ROOT/'build/c3d-vector-storage/support/factor.dylib')
    rows={};costs={};hashes={};equivalence={};differences={};matched={}
    paths={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        frozen(path);receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json')
        assert name not in paths;paths[name]=path;hashes[str(path)]=sha(path);rows[name]=row
        costs[name]=dict(wall_s=receipt['wall_s'],sampled_peak_rss_bytes=receipt['peak_observed_rss_bytes'],returncode=receipt['returncode'],diagnostic_failure=receipt['diagnostic_failure'])
        if receipt['returncode']!=0:continue
        assert row['linear_solve_accepted'] and row['numerically_accepted'] and not row['physical_accuracy_certified']
        assert row['final_residual']['true_residual']<=row['target']
        pc=row['preconditioner'];assert pc['kind']=='vector_workspace_coupled_cholesky' and pc['scaling']=='none' and pc['block_size']==3
        assert pc['factor_input_allocation_bytes']==0 and pc['user_factor_storage_verified'] and pc['numeric_workspace_retained_bytes']==0
        assert pc['shared_input_preserved_after_factor'] and pc['shared_input_preserved_after_solve']
        assert pc['pressure_control']['live_input_preserved'] and pc['pressure_control']['action_preserved']
        assert pc['pressure_control']['owned_high_water_after_bytes']>=pc['pressure_control']['owned_high_water_before_bytes']
        identity=row['identity'];assert identity['conversion_full_mixed_relative_action_change']<1e-12 and identity['pressure_coupling_bitwise_preserved']
        load=row['condensation']['full_load_residency'];assert load['restored_bitwise_after_factor_cleanup'] and load['indexed_array_bytes']<load['dense_array_bytes']
        velocity=row['condensation']['block_storage']['velocity'];assert velocity['source_coefficients_preserved_bitwise'] and velocity['symmetric_diagonal_blocks_verified']
        assert pc['lower_input_scalar_values_count']==9*pc['lower_input_block_nnz']==velocity['dense_block_values']
        assert pc['shared_row_value_bytes']==76*pc['lower_input_block_nnz']
        admission=next(r for r in receipt['progress'] if r.get('phase')=='numeric_stage_admission');assert admission['numeric_stage_admitted']
        assert admission['reserve_bytes']==32*1024**2 and admission['estimated_numeric_stage_bytes']<=1800*1024**2
        assert admission['factor_storage_bytes']==pc['symbolic_factor_storage_bytes'] and admission['numeric_workspace_bytes']==pc['numeric_workspace_bytes']
        costs[name].update(owned_peak_rss_bytes=row['peak_rss_bytes'],iterations=row['iterations'],full_residual=row['final_residual'],timings=row['timings'],resource_samples=row['resource_samples'],preconditioner=pc,conversion=velocity)
    for name,oldname in (('original-L4-sparse-load','original-L4-vector'),('L4-body6-base-sparse-load','L4-body6-base-vector')):
        assert name in rows and costs[name]['returncode']==0
        oldpath=next((ROOT/'build/c3d-vector-storage/runs').glob('*/'+oldname+'-receipt.json'));oldreceipt,old=verify_receipt(oldpath);row=rows[name]
        for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert row['identity'][key]==old['identity'][key]
        assert row['chunk_size']==old['chunk_size']==128
        comparison=force_comparison(row,old);assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
        equivalence[name]=comparison;differences[name]=fields(paths[name].with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
        matched[name]=dict(owned_memory_reduction=1-row['peak_rss_bytes']/old['peak_rss_bytes'],total_time_ratio=costs[name]['wall_s']/oldreceipt['wall_s'])
    stagepath=next((DATA/'stage-runs').glob('*/L4-body6-normal-sparse-load-stage-receipt.json'));stager=frozen(stagepath)
    assert stager['returncode']==0 and stager['stop_reason'] is None and stager['diagnostic_failure'] is None
    stage=json.loads(stagepath.with_name('L4-body6-normal-sparse-load-stage.json').read_text())
    assert stage['diagnostic_accepted'] and not stage['numerically_accepted'] and not stage['numerical_field_published'] and not stage['numeric_factor_attempted']
    assert stage['symbolic_handle_cleanup_verified'] and stage['original_mixed_input_preserved'] and not stagepath.with_name('L4-body6-normal-sparse-load-stage.npz').exists()
    assert stage['tetrahedra']==23616 and stage['peak_rss_bytes']<1800*1024**2 and stager['wall_s']<180
    budget=stage['admission'];assert budget['reserve_bytes']==32*1024**2
    assert budget['estimated_numeric_stage_bytes']==sum(budget[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))
    assert budget['numeric_stage_admitted']==(budget['estimated_numeric_stage_bytes']<=1800*1024**2)
    assert budget['factor_storage_bytes']==1336344064 and budget['numeric_workspace_bytes']==49771768
    priorstage=next((ROOT/'build/c3d-vector-storage/stage-runs').glob('*/L4-body6-normal-vector-stage.json'));oldstage=json.loads(priorstage.read_text())
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert stage['identity'][key]==oldstage['identity'][key]
    normal='L4-body6-normal-sparse-load';normal_accepted=normal in costs and costs[normal]['returncode']==0
    refinement={}
    if normal in costs:
        assert budget['numeric_stage_admitted']
        if normal_accepted:
            for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert rows[normal]['identity'][key]==stage['identity'][key]
            assert rows[normal]['tetrahedra']==23616 and rows[normal]['axis_nodes_m'][1:]==rows['L4-body6-base-sparse-load']['axis_nodes_m'][1:]
            refinement['body6_base_to_normal']=force_comparison(rows[normal],rows['L4-body6-base-sparse-load'])
    else:assert not budget['numeric_stage_admitted']
    observers={};observer_hashes={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        observers[path.name.removesuffix('-receipt.json')]=observer(path);observer_hashes[str(path)]=sha(path)
    if normal_accepted:assert {'L4-body6-base-sparse-load-stress','L4-body6-normal-sparse-load-stress'}<=set(observers)
    assert matched['L4-body6-base-sparse-load']['owned_memory_reduction']>0
    audit=dict(schema='physics_sim_c3d_sparse_load_audit_v1',status='exact_sparse_load_normal_accepted_force_testing_resumed' if normal_accepted else 'exact_sparse_load_base_accepted_normal_not_admitted',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        optional_lossless_load_path_adopted=True,adoption_scope='measured accepted reference meshes only',native_source_unchanged=True,finer_normal_admitted=normal_accepted,force_convergence_testing_resumed_on_normal=normal_accepted,
        reference_receipts_sha256=hashes,stage_receipts_sha256={str(stagepath):sha(stagepath)},observer_receipts_sha256=observer_hashes,costs=costs,equivalence=equivalence,field_differences=differences,matched_cost=matched,
        normal_stage_admission=budget,force_refinement=refinement,stress_defects=observers,test_count=5,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),changed_preexisting_files=changed,
        committed=False,packaged=False,installed=False,next_gate='measured force/domain/component/raw-reaction and stress refinement' if normal_accepted else 'additional measured exact live-memory reduction before normal numeric launch')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps({k:audit[k] for k in ('status','matched_cost','normal_stage_admission','force_refinement','stress_defects')},indent=2))
if __name__=='__main__':main()
