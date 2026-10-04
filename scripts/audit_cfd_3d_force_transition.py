#!/usr/bin/env python3
"""Once-only audit of constrained geometry and actual uniform-cube force evidence."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-force-transition'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def frozen(path):
    r=json.loads(path.read_text())
    assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
    assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180
    assert r.get('linear_iteration_cap',r.get('input_solver_iteration_cap'))==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for name,h in r['source_sha256'].items():assert sha(path.parent/'source'/name)==h==sha(ROOT/'scripts'/name)
    assert path.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    if 'factor_build' in r:
        b=r['factor_build'];assert b['source_sha256']==r['source_sha256']['cfd_reference3d_mixed_storage.c']
        assert sha(path.parent/'source/factor.dylib')==r['factor_library_sha256']==b['library_sha256']
        assert sha(path.parent/'source/factor-build.json')==r['factor_build_record_sha256']
        assert b==json.loads((path.parent/'source/factor-build.json').read_text())
        assert all(x in b['command'] for x in ('-std=c11','-Wall','-Wextra','-Werror'))
    return r

def budget(a):
    assert a['reserve_bytes']==32*1024**2
    assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes'))
    assert a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*1024**2)
    if 'pressure' in a:assert a['pressure']['live_input_preserved'] and a['pressure']['action_preserved']

def main():
    output=DATA/'checkpoint-audit.json';assert not output.exists()
    p=json.loads((DATA/'predecessor.json').read_text());assert sha(Path(p['audit']))==p['sha256']=='a2e253d4de1be443a027c8f15b6b135e2ed32ba2d408cdcd21156ac6049f983e'
    prior=json.loads(Path(p['audit']).read_text())
    for name,h in prior['source_sha256'].items():assert sha(ROOT/name)==h
    for key in ('stage_receipt_sha256','signed_observer_receipt_sha256','geometry_survey_sha256'):
        for name,h in prior[key].items():assert sha(Path(name))==h
    for name,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/name)==h
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[q for q,h in baseline.items() if sha(ROOT/q)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    support=json.loads((DATA/'support-test-receipt.json').read_text());assert support['test_count']==7
    for name,h in support['logs_sha256'].items():
        log=DATA/name;assert sha(log)==h and '\nOK\n' in log.read_text()
        assert f"Ran {5 if name=='support-tests-02.log' else 2} tests" in log.read_text()
    for name,h in support['source_sha256'].items():assert sha(ROOT/name)==h
    assert sha(DATA/'support-tests-01.log')==support['initial_support_log_sha256']
    assert sha(DATA/'pressure-extraction-control.json')==support['pressure_extraction_control_sha256']
    pressure=json.loads((DATA/'pressure-extraction-control.json').read_text())
    assert pressure['cases'][0]['maximum_retained_field_recovery_error']>1e-10 and pressure['cases'][1]['maximum_retained_field_recovery_error']<1e-10
    for name,h in pressure['source_sha256'].items():assert sha(ROOT/'scripts'/name)==h
    transitions=json.loads((DATA/'geometry-survey.json').read_text());assert len(transitions['candidates'])==8
    for name,h in transitions['source_sha256'].items():assert sha(Path(transitions['frozen_source'])/name)==h==sha(ROOT/'scripts'/name)
    assert transitions['fractions']==[.375,.4375,.5,.5625,.625] and transitions['pass_cap']==4
    for c in transitions['candidates']:
        m=c['metadata'];o=m['optimization'];assert not c['geometry_admitted'] and 'affected intrinsic worst shape worsened' in c['reasons']
        assert o['passes']==1 and all(f==.5 for f in o['fractions'])
        assert o['maximum_parent_volume_partition_error_m3']<1e-12
        assert m['original_vertices_preserved'] and m['shared_original_edge_constraints_preserved']
    assert not transitions['numeric_factor_attempted'] and transitions['wall_s']<180 and transitions['owned_peak_rss_bytes']<1800*1024**2
    balanced=json.loads((DATA/'balanced-geometry-survey.json').read_text());assert len(balanced['candidates'])==6
    assert sha(ROOT/'scripts/cfd_reference3d_balanced_normal_mesh.py')==sha(DATA/'balanced-geometry-source/cfd_reference3d_balanced_normal_mesh.py')==balanced['source_sha256']
    assert sha(ROOT/'docs/cfd_3d_balanced_normal_goal.md')==sha(DATA/'balanced-geometry-source/cfd_3d_balanced_normal_goal.md')==balanced['goal_sha256']
    assert not any(c['geometry_admitted'] for c in balanced['candidates']) and not balanced['numeric_factor_attempted']
    for c in balanced['candidates']:assert any('near-edge' in reason for reason in c['reasons'])
    uniform=json.loads((DATA/'uniform-geometry-survey.json').read_text());assert len(uniform['candidates'])==2
    assert sha(ROOT/'scripts/cfd_reference3d_uniform_normal_mesh.py')==uniform['source_sha256']
    assert sha(ROOT/'docs/cfd_3d_uniform_normal_goal.md')==uniform['goal_sha256']
    for m in uniform['candidates']:
        assert m['refined_near_worst_shape']<m['original_near_worst_shape'] and m['refined_near_mean_shape']<m['original_near_mean_shape']
        assert m['refined_global_worst_shape']<m['original_global_worst_shape'] and m['refined_global_max_condition']<m['original_global_max_condition']
        assert m['physical_body_and_domain_preserved'] and not m['body_planes_preserved']
        assert m['uniform_body_interval_m']>m['original_first_cosine_interval_m']
    stagehashes={};stages={};stageids={}
    for count in (6,8):
        path=next((DATA/'stage-runs').glob(f'*/L4-body{count}-uniform-normal-stage-receipt.json'));r=frozen(path)
        row=json.loads(path.with_name(path.name.removesuffix('-receipt.json')+'.json').read_text())
        assert row['diagnostic_accepted'] and row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved']
        assert not any(row[k] for k in ('numeric_factor_attempted','numerically_accepted','numerical_field_published'))
        assert not path.with_name(path.name.removesuffix('-receipt.json')+'.npz').exists()
        assert row['count']==count and row['tetrahedra']==(23616 if count==6 else 36096)
        budget(row['admission']);assert row['admission']['numeric_stage_admitted']==(count==6)
        assert row['identity']['pressure_coupling_bitwise_preserved'] and row['identity']['conversion_full_mixed_relative_action_change']<1e-12
        stages[str(count)]=row['admission'];stageids[count]=row['identity'];stagehashes[str(path)]=sha(path)
    path=next((DATA/'runs').glob('*/L4-body6-uniform-normal-receipt.json'));r=frozen(path);_,row=verify_receipt(path)
    assert row['numerically_accepted'] and row['final_residual']['true_residual']<=row['target']==1e-10
    assert row['count']==6 and row['tetrahedra']==23616 and row['chunk_size']==512 and row['outer_iteration']['restart']==60
    assert row['geometry_control']==uniform['candidates'][0]
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert row['identity'][key]==stageids[6][key]
    pc=row['preconditioner'];assert pc['physical_operator_dtype']=='float64' and pc['preconditioner_value_dtype']=='float32'
    assert pc['shared_input_preserved_after_factor'] and pc['shared_input_preserved_after_solve'] and pc['scaling']=='none'
    assert pc['pressure_control']['live_input_preserved'] and pc['pressure_control']['action_preserved']
    assert row['identity']['pressure_coupling_bitwise_preserved'] and row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
    m=row['condensation']['factor_metadata_residency'];assert m['restored_bitwise'] and m['catalogs_restored_bitwise']
    guard=next(p for p in r['progress'] if p.get('phase')=='numeric_stage_admission');budget(guard);assert guard['numeric_stage_admitted']
    assert pc['flexible_iteration']['basis_array_bytes']<=pc['flexible_iteration']['basis_reservation_bytes']==guard['basis_reservation_bytes']
    compare=json.loads((DATA/'force-comparison.json').read_text());assert compare['input_receipt_sha256']==sha(path)
    for name,control in compare['controls'].items():
        oldpath=Path(control['receipt']);assert sha(oldpath)==control['sha256'];oldr,old=verify_receipt(oldpath)
        assert force_comparison(row,old)==compare['comparisons'][name]
        assert control['time_ratio']==r['wall_s']/oldr['wall_s'] and control['memory_ratio']==row['peak_rss_bytes']/old['peak_rss_bytes']
        assert not compare['comparisons'][name]['physical_force_gate_passed']
    obpath=next((DATA/'observer-runs').glob('*/*-receipt.json'));obr=frozen(obpath)
    observation=json.loads(obpath.with_name(obpath.name.removesuffix('-receipt.json')+'.json').read_text())
    assert observation['diagnostic_accepted'] and observation['input_complete_numerical_gates_passed'] and not observation['physical_accuracy_certified']
    assert observation['input_receipt_sha256']==sha(path)
    assert observation['wall_s']<180 and observation['peak_rss_bytes']<1800*1024**2
    max_identity=0.
    for lift,old in zip(observation['lifts'],row['consistency_diagnostics']['volume_lifts']):
        assert abs(sum(lift[p]['weak_load_n'][0] for p in ('pressure','viscous'))-old['symmetric_stress_load_n'])<1e-9
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            np.testing.assert_allclose(lift[part]['raw_surface_load_n'],row[key],rtol=0,atol=1e-10)
            max_identity=max(max_identity,max(abs(v) for v in lift[part]['identity_error_n']))
    assert max_identity<1e-9
    oldobs=next((ROOT/'build/c3d-factor-catalog/observer-runs').glob('*/L4-body6-normal-catalog-stress.json'));old=json.loads(oldobs.read_text())
    assert observation['volume_strong_equilibrium_defect_l2']<old['volume_strong_equilibrium_defect_l2'] and observation['interior_stress_jump_l2']<old['interior_stress_jump_l2']
    sources=('scripts/audit_cfd_3d_force_transition.py','docs/cfd_3d_force_transition_checkpoint.md','docs/cfd_3d_second_normal_goal.md','docs/cfd_3d_uniform_normal_goal.md','docs/cfd_3d_balanced_normal_goal.md','scripts/cfd_reference3d_uniform_normal_probe.py','scripts/cfd_reference3d_uniform_normal_stage_probe.py')
    audit=dict(schema='physics_sim_c3d_force_transition_audit_v1',status='uniform_control_numerically_verified_physical_force_gate_open',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,physical_mesh_adopted=False,reference_diagnostic_field_retained=True,
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=p['sha256'],changed_preexisting_files=changed,test_count=7,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),rejected_transition_candidates=8,rejected_balanced_cosine_candidates=6,
        geometry_evidence_sha256={name:sha(DATA/name) for name in ('geometry-survey.json','balanced-geometry-survey.json','uniform-geometry-survey.json','pressure-extraction-control.json')},stage_receipts_sha256=stagehashes,stage_admission=stages,numerical_receipts_sha256={str(path):sha(path)},cost=dict(wall_s=r['wall_s'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],owned_peak_rss_bytes=row['peak_rss_bytes'],iterations=row['iterations'],full_residual=row['final_residual']),
        force_comparison_sha256=sha(DATA/'force-comparison.json'),force_comparisons=compare['comparisons'],matched_costs=compare['controls'],observer_receipts_sha256={str(obpath):sha(obpath)},stress=dict(volume_equilibrium_defect_l2=observation['volume_strong_equilibrium_defect_l2'],interior_jump_l2=observation['interior_stress_jump_l2'],maximum_identity_error_n=max_identity,normal_anchor_sha256=sha(oldobs)),source_sha256={name:sha(ROOT/name) for name in sources},native_source_unchanged=True,committed=False,packaged=False,installed=False,next_gate='streamwise first-normal refinement with cosine cube surface held and unchanged fullFE/resource/force gates')
    output.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps({k:audit[k] for k in ('status','test_count','physical_mesh_adopted','next_gate')}),flush=True)
if __name__=='__main__':main()
