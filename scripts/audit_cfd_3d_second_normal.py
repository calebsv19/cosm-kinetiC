#!/usr/bin/env python3
"""Once-only closure of nested resource stops and independent tensor force control."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-second-normal'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def receipt(path):
    r=json.loads(path.read_text())
    assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180
    assert r.get('linear_iteration_cap',r.get('input_solver_iteration_cap'))==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(path.parent/'source'/q)==h==sha(R/'scripts'/q)
    assert path.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    if 'factor_build' in r:
        b=r['factor_build'];assert b['source_sha256']==r['source_sha256']['cfd_reference3d_mixed_storage.c']
        assert sha(path.parent/'source/factor.dylib')==r['factor_library_sha256']==b['library_sha256']
        assert sha(path.parent/'source/factor-build.json')==r['factor_build_record_sha256']
        assert all(x in b['command'] for x in ('-std=c11','-Wall','-Wextra','-Werror'))
    row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text())
    return r,row

def budget(a):
    assert a['reserve_bytes']==32*1024**2
    assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes'))
    assert a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*1024**2)
    assert a['pressure']['live_input_preserved'] and a['pressure']['action_preserved']

def main():
    output=D/'checkpoint-audit.json';assert not output.exists()
    prior=json.loads((D/'predecessor.json').read_text());assert sha(Path(prior['path']))==prior['sha256']=='ff4dc5f32f9aed77a4b77f080fc41a452400d7280a8bbcef736c0f1c1d10600d'
    protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for q,h in protected.items():assert sha(R/q)==h
    baseline=json.loads((D/'baseline.json').read_text());changed=[q for q,h in baseline.items() if sha(R/q)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
    for name in ('support-test-receipt.json','additional-support-receipt.json'):
        s=json.loads((D/name).read_text());assert s['passed']
        for q,h in s['source_sha256'].items():assert sha(R/q)==h
        for q,h in s['logs_sha256'].items():
            p=R/q if q.startswith('build/') else D/q;assert sha(p)==h
        if name.startswith('support-'):
            assert s['test_count']==4;assert '\nOK\n' in (D/'support-tests-03.log').read_text()
            for q,h in s['initial_sources_sha256'].items():assert sha(R/q)==h
        else:
            assert s['test_count']==8
            for q in s['logs_sha256']:assert 'Ran 4 tests' in (D/q).read_text() and '\nOK\n' in (D/q).read_text()
            assert sha(D/'scalar-support-factor.dylib')==s['scalar_support_factor_sha256']
            assert sha(R/'scripts/cfd_reference3d_mixed_storage.c')==s['factor_source_sha256']
    controls=json.loads((D/'pressure-reconstruction-controls.json').read_text())
    for q,h in controls['source_sha256'].items():assert sha(R/'scripts'/q)==h
    a,b,c=controls['controls'];assert not a['absolute_mean_target_met'] and not b['absolute_coefficient_target_met'] and c['absolute_coefficient_target_met']
    assert max(b['original_full_residual'],c['original_full_residual'])<1e-10
    for name,folder,goal in (('geometry-survey.json','geometry-source','cfd_3d_second_normal_goal.md'),('tensor-geometry-survey.json','tensor-geometry-source','cfd_3d_second_normal_tensor_goal.md')):
        s=json.loads((D/name).read_text());assert s['geometry_admitted'] and not s['physical_mesh_adopted']
        for q,h in s['source_sha256'].items():assert sha(D/folder/q)==h==sha(R/'scripts'/q)
        assert sha(D/folder/goal)==s['goal_sha256']==sha(R/'docs'/goal)
        m=s['metadata'];assert m['body_surface_triangles_preserved'] and m['refined_global_worst_shape']<=m['original_global_worst_shape']*(1+1e-8) and m['refined_global_max_condition']<=m['original_global_max_condition']*(1+1e-8)
    stages={};stagehashes={}
    for stem in ('L4-body6-second-normal-stage','L4-body6-second-normal-tensor-stage','L8-body6-second-normal-tensor-held-outer2-stage'):
        p=next((D/'stage-runs').glob('*/'+stem+'-receipt.json'));r,row=receipt(p)
        assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
        assert row['diagnostic_accepted'] and row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved']
        assert not row['numeric_factor_attempted'] and not row['numerical_field_published'] and not row['numerically_accepted']
        assert not p.with_name(stem+'.npz').exists();budget(row['admission']);stages[stem]=row;stagehashes[str(p)]=sha(p)
    nested=stages['L4-body6-second-normal-stage'];tensor=stages['L4-body6-second-normal-tensor-stage']
    assert nested['tetrahedra']==41216 and not nested['admission']['numeric_stage_admitted']
    assert tensor['tetrahedra']==28416 and tensor['admission']['numeric_stage_admitted']
    l8=stages['L8-body6-second-normal-tensor-held-outer2-stage'];assert l8['tetrahedra']==33216 and not l8['admission']['numeric_stage_admitted']
    held=json.loads((D/'held-support-receipt.json').read_text());assert held['test_count']==1 and held['passed']
    for q,h in held['source_sha256'].items():assert sha(R/q)==h
    assert sha(D/'held-support-tests-01.log')==held['log_sha256'] and '\nOK\n' in (D/'held-support-tests-01.log').read_text()
    assert sha(R/'scripts/cfd_reference3d_second_normal_tensor_mesh.py')==held['geometry_source_sha256']
    p=next((D/'scalar-stage-runs').glob('*/L4-body6-second-normal-scalar-stage-receipt.json'));r,scalar=receipt(p)
    assert r['returncode']==2 and r['stop_reason'] is None and not scalar['diagnostic_accepted']
    rejection=scalar['resource_phase_rejected'];assert rejection['phase']=='workspace_symbolic_ready' and rejection['peak_rss_bytes']>1800*1024**2
    assert not any(p.parent.glob('*.npz'))
    assembled=next(x for x in r['progress'] if x.get('phase')=='assembled')
    assert assembled['identity']==nested['identity'];assert r['diagnostic_failure']['kind']=='resource_cap'
    scalar_capacity=assembled['block_storage']['stored_dense_block_value_count']
    scalar_owned_input_bytes=8*(3*assembled['block_storage']['velocity']['node_count']+1)+8*scalar_capacity
    stagehashes[str(p)]=sha(p)
    p=next((D/'runs').glob('*/L4-body6-second-normal-tensor-receipt.json'));r,row=verify_receipt(p.resolve());receipt(p)
    assert r['returncode']==0 and row['target']==1e-10 and row['final_residual']['true_residual']<1e-10
    assert row['identity']==tensor['identity'] and row['tetrahedra']==28416
    assert row['geometry_control']==json.loads((D/'tensor-geometry-survey.json').read_text())['metadata']
    assert row['preconditioner']['shared_input_preserved_after_solve'] and row['preconditioner']['user_factor_storage_verified']
    guard=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');budget(dict(guard,pressure=row['preconditioner']['pressure_control']));assert guard['estimated_numeric_stage_bytes']<=1800*1024**2
    assert row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
    assert row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise']
    comparison=json.loads((D/'force-comparison.json').read_text());anchor_path=Path(comparison['anchor_receipt']);_,anchor=verify_receipt(anchor_path)
    assert sha(anchor_path)==comparison['anchor_receipt_sha256'] and force_comparison(row,anchor)==comparison['comparison']
    assert not comparison['physical_mesh_adopted'] and not comparison['comparison']['physical_force_gate_passed']
    obp=next((D/'observer-runs').glob('*/L4-body6-second-normal-tensor-signed-force-receipt.json'));obr,ob=receipt(obp)
    assert obr['returncode']==0 and obr['stop_reason'] is None and obr['diagnostic_failure'] is None and ob['diagnostic_accepted']
    assert ob['input_receipt_sha256']==sha(p) and ob['original_linear_residual']==row['final_residual']
    identity=max(abs(x) for lift in ob['lifts'] for part in ('pressure','viscous') for x in lift[part]['identity_error_n']);assert identity<1e-9
    assert sha(Path(ob['signed_attribution_path']))==ob['signed_attribution_sha256']
    with np.load(ob['signed_attribution_path'],allow_pickle=False) as arrays:
        assert arrays['cell_net_n'].shape==(2,2,3,28416) and np.all(np.isfinite(arrays['cell_net_n']))
    new_sources=[q for q in sorted((R/'scripts').glob('*second_normal*.py'))]+[R/'scripts/cfd_reference3d_scalar_workspace.py',R/'scripts/cfd_reference3d_scalar_graph_stage_probe.py']
    new_sources += [R/'tests'/n for n in ('test_cfd_reference3d_second_normal.py','test_cfd_reference3d_scalar_workspace.py','test_cfd_reference3d_second_normal_tensor.py','test_cfd_reference3d_second_normal_held.py')]
    new_sources += [R/'docs'/n for n in ('cfd_3d_second_normal_goal.md','cfd_3d_scalar_graph_goal.md','cfd_3d_second_normal_tensor_goal.md','cfd_3d_second_normal_checkpoint.md','cfd_3d_normal_force_next_goal.md')]
    result=dict(status='same_surface_tensor_field_verified_physical_force_gate_open',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,physical_mesh_adopted=False,test_count=13,predecessor_sha256=prior['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,source_sha256={str(q.relative_to(R)):sha(q) for q in new_sources},stage_receipts_sha256=stagehashes,nested_cut_numeric_admission=nested['admission'],scalar_symbolic_rejection=rejection,scalar_input_capacity_owned_bytes=scalar_owned_input_bytes,tensor_numeric_admission=tensor['admission'],matched_l8_numeric_admission=l8['admission'],numerical_receipt_sha256={str(p):sha(p)},cost=dict(wall_s=r['wall_s'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],owned_peak_rss_bytes=row['peak_rss_bytes'],iterations=row['iterations'],full_residual=row['final_residual']),force_comparison_sha256=sha(D/'force-comparison.json'),force_comparison=comparison['comparison'],observer_receipt_sha256={str(obp):sha(obp)},stress=dict(volume_equilibrium_defect_l2=ob['volume_strong_equilibrium_defect_l2'],interior_jump_l2=ob['interior_stress_jump_l2'],maximum_identity_error_n=identity),pressure_reconstruction_limitation_retained=True,pressure_controls_sha256=sha(D/'pressure-reconstruction-controls.json'),support_receipts_sha256={n:sha(D/n) for n in ('support-test-receipt.json','additional-support-receipt.json','held-support-receipt.json')},geometry_sha256={n:sha(D/n) for n in ('geometry-survey.json','tensor-geometry-survey.json')},committed=False,packaged=False,installed=False)
    output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(output),sha256=sha(output),status=result['status'])))

if __name__=='__main__':main()
