#!/usr/bin/env python3
"""Audit precision-only preconditioning against original float64 field authority."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
from cfd_reference3d_flexible import basis_reservation
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-mixed-precision'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def frozen(path):
    r=json.loads(path.read_text());assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180 and r.get('linear_iteration_cap',r.get('input_solver_iteration_cap'))==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(path.parent/'source'/q)==h==sha(ROOT/'scripts'/q)
    assert path.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    if 'factor_build' in r:
        b=r['factor_build'];factor_source='cfd_reference3d_mixed_storage.c' if 'cfd_reference3d_mixed_storage.c' in r['source_sha256'] else 'cfd_reference3d_vector_storage.c'
        assert b['source_sha256']==r['source_sha256'][factor_source]
        assert sha(path.parent/'source/factor.dylib')==b['library_sha256']==r['factor_library_sha256']
        assert sha(path.parent/'source/factor-build.json')==r['factor_build_record_sha256'] and json.loads((path.parent/'source/factor-build.json').read_text())==b
    return r

def admission(row):
    assert row['reserve_bytes']==32*1024**2 and row['basis_reservation_bytes']>0
    assert row['estimated_numeric_stage_bytes']==sum(row[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
    assert row['numeric_stage_admitted']==(row['estimated_numeric_stage_bytes']<=1800*1024**2)


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
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    prior=json.loads((DATA/'predecessor.json').read_text());assert sha(Path(prior['audit']))==prior['sha256']
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['test_count']==11
    log=DATA/'support-tests-04.log';assert sha(log)==tests['log_sha256'] and 'Ran 11 tests' in log.read_text() and '\nOK\n' in log.read_text()
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert tests['library_sha256']==sha(DATA/'support/factor.dylib') and tests['api_receipt_sha256']==sha(DATA/'support/api-receipt.json')
    api=json.loads((DATA/'support/api-receipt.json').read_text());assert sha(Path(api['source']))==api['sha256']==sha(Path(api['snapshot']))
    hashes={};rows={};costs={};equivalence={};differences={};matched={};paths={};target_met={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        r=frozen(path);_,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');assert name not in paths;paths[name]=path;hashes[str(path)]=sha(path);rows[name]=row
        costs[name]=dict(wall_s=r['wall_s'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],returncode=r['returncode'],diagnostic_failure=r['diagnostic_failure'])
        target_met[name]=None
        if r['returncode']!=0:continue
        target_met[name]=row['final_residual']['true_residual']<=row['target'];assert row['target']==1e-10
        pc=row['preconditioner'];assert pc['kind']=='mixed_workspace_coupled_cholesky' and pc['scaling']=='none' and pc['preconditioner_value_dtype']=='float32' and pc['physical_operator_dtype']=='float64'
        assert pc['user_factor_storage_verified'] and pc['numeric_workspace_retained_bytes']==0 and pc['shared_input_preserved_after_factor'] and pc['shared_input_preserved_after_solve']
        assert pc['factor_input_allocation_bytes']==4*pc['lower_input_scalar_values_count']
        assert pc['pressure_control']['live_input_preserved'] and pc['pressure_control']['action_preserved'] and pc['pressure_control']['owned_high_water_after_bytes']>=pc['pressure_control']['owned_high_water_before_bytes']
        f=pc['flexible_iteration'];assert f['restart']==60 and f['iterations']==row['iterations'] and f['basis_array_bytes']<=f['basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],60) and f['final_true_metric']<=row['target']
        assert row['outer_iteration']['kind']=='flexible_right_preconditioned_arnoldi' and row['pressure_preconditioner']['kind']=='negative_exact_macro_constant_mass_in_upper_triangular_preconditioner'
        identity=row['identity'];assert identity['pressure_coupling_bitwise_preserved'] and identity['conversion_full_mixed_relative_action_change']<1e-12
        m=row['condensation']['factor_metadata_residency'];assert m['old_arrays_detached'] and m['restored_bitwise'] and m['old_catalog_owners_detached'] and m['catalogs_restored_bitwise']
        assert row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup'] and row['condensation']['block_storage']['velocity']['source_coefficients_preserved_bitwise']
        guard=next(p for p in r['progress'] if p.get('phase')=='numeric_stage_admission');admission(guard);assert guard['numeric_stage_admitted'] and guard['basis_reservation_bytes']==f['basis_reservation_bytes']
        costs[name].update(iterations=row['iterations'],owned_peak_rss_bytes=row['peak_rss_bytes'],full_residual=row['final_residual'],factor_storage_bytes=pc['symbolic_factor_storage_bytes'],flexible=f,timings=row['timings'])
    assert 'original-L4-mixed' in rows and costs['original-L4-mixed']['returncode']==0
    anchors=(('original-L4-mixed','c3d-factor-catalog','original-L4-catalog'),('L4-body6-base-mixed','c3d-factor-catalog','L4-body6-base-catalog'),('L4-body6-base-mixed-chunk512','c3d-factor-catalog','L4-body6-base-catalog'),('L4-body6-normal-mixed','c3d-factor-catalog','L4-body6-normal-catalog'),('L8-body6-normal-held-mixed','c3d-normal-domain','L8-body6-normal-held'))
    for name,folder,oldname in anchors:
        if name not in rows or costs[name]['returncode']!=0:continue
        oldpath=next((ROOT/'build'/folder/'runs').glob('*/'+oldname+'-receipt.json'));oldr,old=verify_receipt(oldpath);row=rows[name]
        for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert row['identity'][k]==old['identity'][k]
        comparison=force_comparison(row,old);assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
        equivalence[name]=comparison;differences[name]=fields(paths[name].with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
        matched[name]=dict(time_ratio=costs[name]['wall_s']/oldr['wall_s'],owned_memory_ratio=row['peak_rss_bytes']/old['peak_rss_bytes'],same_chunk=row['chunk_size']==old['chunk_size'],cost_scope='historical complete-run comparison; differing chunk sizes do not isolate precision')
    stages={};stagehashes={}
    for path in sorted((DATA/'stage-runs').glob('*/*-receipt.json')):
        r=frozen(path);name=path.name.removesuffix('-receipt.json');row=json.loads(path.with_name(name+'.json').read_text())
        assert r['returncode']==0 and row['diagnostic_accepted'] and row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved']
        assert not any(row[k] for k in ('numeric_factor_attempted','numerically_accepted','numerical_field_published')) and not path.with_name(name+'.npz').exists()
        admission(row['admission']);stages[name]=row['admission'];stagehashes[str(path)]=sha(path)
        numerical=name.removesuffix('-stage')
        if numerical in rows and costs[numerical]['returncode']==0:
            for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert row['identity'][k]==rows[numerical]['identity'][k]
    controls={};controlhashes={}
    for path in (DATA/'control-runs').glob('*/*-receipt.json'):
        r=frozen(path);_,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');controlhashes[str(path)]=sha(path)
        assert r['returncode']==0 and row['final_residual']['true_residual']<=row['target']==1e-10 and row['chunk_size']==512
        controls[name]=dict(wall_s=r['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],iterations=row['iterations'],full_residual=row['final_residual'])
        if name=='L4-body6-base-double-chunk512':
            current=rows['L4-body6-base-mixed-chunk512']
            for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert current['identity'][k]==row['identity'][k]
            assert current['chunk_size']==row['chunk_size']==512
            comparison=force_comparison(current,row);assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
            equivalence['base_chunk512_matched_precision']=comparison
            differences['base_chunk512_matched_precision']=fields(paths['L4-body6-base-mixed-chunk512'].with_name('L4-body6-base-mixed-chunk512.npz'),path.with_name('L4-body6-base-double-chunk512.npz'))
            matched['base_chunk512_matched_precision']=dict(time_ratio=costs['L4-body6-base-mixed-chunk512']['wall_s']/r['wall_s'],owned_memory_ratio=current['peak_rss_bytes']/row['peak_rss_bytes'],same_chunk=True,cost_scope='serial same-host matched chunk512 and geometry, precision/outer iteration differ')
    observers={};observerhashes={};end_force={};refined_domain={}
    for path in (DATA/'observer-runs').glob('*/*-receipt.json'):
        observers[path.name.removesuffix('-receipt.json')]=observe(path);observerhashes[str(path)]=sha(path)
    end='L8-body6-normal-held-outer2-mixed'
    if end in costs and costs[end]['returncode']==0:
        previous=next((ROOT/'build/c3d-normal-domain/runs').glob('*/L8-body6-normal-held-receipt.json'));_,old=verify_receipt(previous)
        new=rows[end];assert new['length']==old['length']==8 and new['flow_m3_s']==old['flow_m3_s'] and new['mu']==old['mu']
        assert new['tetrahedra']==28416 and new['outer_layers']==2 and new['domain_mesh_mode']=='held_l4'
        stopped=next((ROOT/'build/c3d-end-slab/stage-runs').glob('*/L8-body6-normal-held-outer2-stage.json'));original=json.loads(stopped.read_text())
        for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert new['identity'][k]==original['identity'][k]
        endaudit=json.loads((ROOT/'build/c3d-end-slab/checkpoint-audit.json').read_text())
        assert sha(ROOT/'build/c3d-end-slab/support-test-receipt.json')==endaudit['support_test_receipt_sha256']
        end_force=force_comparison(new,old)
        if 'L4-body6-normal-mixed' in rows:
            short=rows['L4-body6-normal-mixed'];raw=force_comparison(new,short)
            refined_domain=dict(component_relative_changes=raw['component_relative_changes'],length_dependent_scalar_relative_changes=raw['scalar_relative_changes'],raw_surface_reaction_relative_mismatch=raw['raw_surface_reaction_relative_mismatch'],scope='held near-body/cross-section/flow comparison with differing end subdivisions; not proof of a mesh/domain plateau or physical certification')
        assert end+'-stress' in observers
    m=matched.get('base_chunk512_matched_precision',{})
    optional_adopted=target_met.get('L4-body6-base-mixed-chunk512') is True and m.get('owned_memory_ratio',2)<1 and m.get('time_ratio',2)<1
    source_files=('scripts/audit_cfd_3d_mixed_precision.py','docs/cfd_3d_mixed_precision_execution_goal.md','docs/cfd_3d_mixed_precision_checkpoint.md')
    audit=dict(schema='physics_sim_c3d_mixed_precision_audit_v1',status='precision_control_measured_full_equations_preserved',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,optional_measured_reference_path_adopted=optional_adopted,adoption_scope='only accepted meshes with useful measured complete target and cost; not a universal default',observer_receipts_sha256=observerhashes,stress_defects=observers,end_slab_force_comparison=end_force,refined_domain_observation=refined_domain,reference_receipts_sha256=hashes,control_receipts_sha256=controlhashes,controls=controls,stage_receipts_sha256=stagehashes,costs=costs,requested_full_target_met=target_met,stage_admission=stages,field_equivalence=equivalence,field_maximum_absolute_differences=differences,matched_cost=matched,
        test_count=11,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=prior['sha256'],changed_preexisting_files=changed,source_sha256={p:sha(ROOT/p) for p in source_files},native_source_unchanged=True,committed=False,packaged=False,installed=False,next_gate='admitted end-slab/force refinement with preserved physical gates')
    out=DATA/'checkpoint-audit.json';assert not out.exists();out.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps({k:audit[k] for k in ('status','optional_measured_reference_path_adopted','matched_cost','requested_full_target_met','stage_admission')},indent=2))
if __name__=='__main__':main()
