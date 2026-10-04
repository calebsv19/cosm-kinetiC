#!/usr/bin/env python3
"""Audit a matched normal-domain experiment without rewriting predecessor evidence."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-normal-domain'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def frozen(path):
    receipt=json.loads(path.read_text())
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==h==sha(ROOT/'scripts'/n)
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    if 'factor_build' in receipt:
        build=receipt['factor_build'];assert build['source_sha256']==receipt['source_sha256']['cfd_reference3d_vector_storage.c']
        assert sha(path.parent/'source/factor.dylib')==receipt['factor_library_sha256']==build['library_sha256']
        assert sha(path.parent/'source/factor-build.json')==receipt['factor_build_record_sha256']
        assert json.loads((path.parent/'source/factor-build.json').read_text())==build
    return receipt

def observe(path):
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
    return dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=row['interior_stress_jump_l2'],maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for p in ('pressure','viscous') for v in lift[p]['identity_error_n']))

def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','docs/cfd_3d_force_restart_goal.md','make/rules-tools.mk'},changed
    prior=json.loads((DATA/'predecessor.json').read_text());assert sha(Path(prior['audit']))==prior['sha256']
    assert sha(ROOT/'build/c3d-factor-catalog/force-readiness.json')==prior['force_readiness_sha256']
    for key in ('reference_receipts_sha256','stage_receipts_sha256','observer_receipts_sha256'):
        for p,h in prior[key].items():
            path=Path(p);assert sha(path)==h
            for q,d in json.loads(path.read_text())['artifact_sha256'].items():assert sha(Path(q))==d
    for p,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['test_count']==5
    log=Path(tests['log']);assert sha(log)==tests['log_sha256'] and 'Ran 5 tests' in log.read_text() and '\nOK\n' in log.read_text()
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert tests['library_sha256']==sha(ROOT/'build/c3d-vector-storage/support/factor.dylib')
    assert tests['predecessor_test_receipt_sha256']==sha(ROOT/'build/c3d-factor-catalog/support-test-receipt.json')
    oldpath=next((ROOT/'build/c3d-factor-catalog/runs').glob('*/L4-body6-normal-catalog-receipt.json'));oldreceipt,old=verify_receipt(oldpath)
    stagepath=next((DATA/'stage-runs').glob('*/L8-body6-normal-held-stage-receipt.json'));stager=frozen(stagepath)
    assert stager['returncode']==0 and stager['stop_reason'] is None and stager['diagnostic_failure'] is None
    stage=json.loads(stagepath.with_name('L8-body6-normal-held-stage.json').read_text())
    assert stage['diagnostic_accepted'] and not stage['numerically_accepted'] and not stage['numerical_field_published'] and not stage['numeric_factor_attempted']
    assert stage['symbolic_handle_cleanup_verified'] and stage['original_mixed_input_preserved'] and not stagepath.with_name('L8-body6-normal-held-stage.npz').exists()
    assert stage['tetrahedra']==23616 and stage['peak_rss_bytes']<1800*1024**2 and stage['wall_s']<180 and stage['domain_mesh_mode']=='held_l4'
    budget=stage['admission'];assert budget['reserve_bytes']==32*1024**2
    assert budget['estimated_numeric_stage_bytes']==sum(budget[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))
    assert budget['numeric_stage_admitted']==(budget['estimated_numeric_stage_bytes']<=1800*1024**2)
    assert budget['factor_storage_bytes']==1336344064 and budget['numeric_workspace_bytes']==49771768
    np.testing.assert_array_equal(stage['axis_nodes_m'][1:],old['axis_nodes_m'][1:])
    np.testing.assert_array_equal(np.array(stage['axis_nodes_m'][0])[1:-1],np.array(old['axis_nodes_m'][0])[1:-1]+2.)
    paths=list((DATA/'runs').glob('*/L8-body6-normal-held-receipt.json'));cost={};comparison={};pair={};target_met=False;accepted=False;hashes={}
    if paths:
        assert len(paths)==1 and budget['numeric_stage_admitted'];path=paths[0];receipt=frozen(path);_,row=verify_receipt(path);hashes[str(path)]=sha(path)
        cost=dict(returncode=receipt['returncode'],wall_s=receipt['wall_s'],sampled_peak_rss_bytes=receipt['peak_observed_rss_bytes'],diagnostic_failure=receipt['diagnostic_failure'],progress=receipt['progress'])
        if row is not None:cost.update(owned_peak_rss_bytes=row.get('peak_rss_bytes'),iterations=row.get('iterations'),full_residual=row.get('final_residual'))
        accepted=receipt['returncode']==0
        if accepted:
            assert row['tetrahedra']==23616 and row['count']==6 and row['split_first_normal'] and row['domain_mesh_mode']=='held_l4'
            assert row['length']==8 and row['mu']==old['mu']==.1 and row['flow_m3_s']==old['flow_m3_s']==.008
            assert row['chunk_size']==old['chunk_size']==512 and row['target']==old['target']==1e-10
            assert row['velocity_degree']==4 and row['pressure_degree']==3 and row['verified_volume_product_degree']==6
            target_met=row['final_residual']['true_residual']<=row['target']
            pc=row['preconditioner'];assert pc['kind']=='vector_workspace_coupled_cholesky' and pc['scaling']=='none' and pc['block_size']==3
            assert pc['user_factor_storage_verified'] and pc['factor_input_allocation_bytes']==0 and pc['numeric_workspace_retained_bytes']==0
            assert pc['shared_input_preserved_after_factor'] and pc['shared_input_preserved_after_solve']
            assert pc['pressure_control']['live_input_preserved'] and pc['pressure_control']['action_preserved']
            residency=row['condensation']['factor_metadata_residency'];assert residency['old_arrays_detached'] and residency['restored_bitwise'] and residency['old_catalog_owners_detached'] and residency['catalogs_restored_bitwise']
            assert row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
            assert row['condensation']['block_storage']['velocity']['source_coefficients_preserved_bitwise']
            for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert row['identity'][k]==stage['identity'][k]
            assert row['identity']['pressure_coupling_bitwise_preserved'] and row['identity']['conversion_full_mixed_relative_action_change']<1e-12
            admission=next(r for r in receipt['progress'] if r.get('phase')=='numeric_stage_admission');assert admission['numeric_stage_admitted'] and admission['reserve_bytes']==32*1024**2 and admission['estimated_numeric_stage_bytes']<=1800*1024**2
            with np.load(oldpath.with_name('L4-body6-normal-catalog.npz')) as a,np.load(path.with_name('L8-body6-normal-held.npz')) as b:
                np.testing.assert_array_equal(a['tetrahedra'],b['tetrahedra']);np.testing.assert_array_equal(a['vertices_m'][1:],b['vertices_m'][1:])
                np.testing.assert_array_equal(b['lo'],a['lo']+np.array([2.,0.,0.]));np.testing.assert_array_equal(b['hi'],a['hi']+np.array([2.,0.,0.]))
                inner=(a['vertices_m'][0]>=old['axis_nodes_m'][0][1])&(a['vertices_m'][0]<=old['axis_nodes_m'][0][-2])
                np.testing.assert_allclose(b['vertices_m'][0,inner],a['vertices_m'][0,inner]+2.,rtol=0,atol=1e-14)
                pair=dict(same_connectivity=True,same_cross_section=True,body_and_inner_planes_translation_m=2.,end_slabs_only_extended=True)
            raw=force_comparison(row,old)
            comparison=dict(component_relative_changes=raw['component_relative_changes'],raw_surface_reaction_relative_mismatch=raw['raw_surface_reaction_relative_mismatch'],length_dependent_scalar_relative_changes=raw['scalar_relative_changes'],length_dependent_scalars_excluded_from_flat_domain_gate=True,
                domain_component_force_gate_passed=max([*raw['component_relative_changes'].values(),*raw['raw_surface_reaction_relative_mismatch'].values()])<=.01,
                forces_n={k:{name:r[k] for name,r in (('L4',old),('L8',row))} for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n')},diagnostics=row['diagnostics'])
    else:assert not budget['numeric_stage_admitted']
    observers={};observer_hashes={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):observers[path.name.removesuffix('-receipt.json')]=observe(path);observer_hashes[str(path)]=sha(path)
    if accepted:assert 'L8-body6-normal-held-stress' in observers
    localization=json.loads((DATA/'force-localization.json').read_text())
    assert not localization['physical_accuracy_certified'] and localization['reference_equations_unchanged']
    assert localization['analysis_source_sha256']==sha(ROOT/'scripts/analyze_cfd_3d_normal_domain.py')
    for q,h in localization['input_sha256'].items():assert sha(Path(q))==h
    for name,entry in localization['results'].items():
        assert sum(r['tetrahedra'] for r in entry['geometry_groups'].values())==23616
        for lift in entry['signed_lifts']:
            assert abs(sum(v[0] for v in lift['signed_total_jump_minus_volume_centroid_buckets_n'])-lift['total_signed_drag_defect_n'])<1e-12
            assert abs(lift['total_signed_drag_defect_n']-entry['raw_minus_reaction_force_n'][0])<1e-9
            assert lift['weak_reaction_max_absolute_difference_n']<1e-9
    files=('scripts/audit_cfd_3d_normal_domain.py','scripts/analyze_cfd_3d_normal_domain.py','docs/cfd_3d_normal_domain_goal.md','docs/cfd_3d_normal_domain_checkpoint.md')
    audit=dict(schema='physics_sim_c3d_normal_domain_audit_v1',status='matched_normal_domain_numerical_accepted_physical_gate_open' if accepted else 'matched_normal_domain_rejected_no_field',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        longer_normal_numerically_accepted=accepted,requested_full_residual_target_met=target_met,normal_domain_stage=budget,cost=cost,geometry_pair=pair,force_domain_comparison=comparison,stress_defects=observers,
        reference_receipts_sha256=hashes,stage_receipts_sha256={str(stagepath):sha(stagepath)},observer_receipts_sha256=observer_hashes,anchor_l4_receipt_sha256=sha(oldpath),predecessor_audit_sha256=prior['sha256'],test_count=5,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),baseline_sha256=sha(DATA/'baseline.json'),changed_preexisting_files=changed,
        force_localization_sha256=sha(DATA/'force-localization.json'),source_sha256={f:sha(ROOT/f) for f in files},native_source_unchanged=True,committed=False,packaged=False,installed=False,next_gate='signed force-specific refinement with complete force/raw/physical gates' if accepted else 'resource/block residual diagnosis before longer normal adoption')
    output=DATA/'checkpoint-audit.json';assert not output.exists();output.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','requested_full_residual_target_met','force_domain_comparison','stress_defects')},indent=2))
if __name__=='__main__':main()
