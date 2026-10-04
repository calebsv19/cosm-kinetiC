#!/usr/bin/env python3
"""Once-only audit of resource refusal and signed-force-local cube diagnostics."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-end-plateau'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def frozen(path):
    receipt=json.loads(path.read_text())
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
    assert receipt.get('linear_iteration_cap',receipt.get('input_solver_iteration_cap'))==3000
    assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    for q,h in receipt['artifact_sha256'].items():assert sha(Path(q))==h
    for name,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/name)==h==sha(ROOT/'scripts'/name)
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    if 'factor_build' in receipt:
        build=receipt['factor_build'];assert build['source_sha256']==receipt['source_sha256']['cfd_reference3d_mixed_storage.c']
        assert sha(path.parent/'source/factor.dylib')==build['library_sha256']==receipt['factor_library_sha256']
        assert sha(path.parent/'source/factor-build.json')==receipt['factor_build_record_sha256']
        assert json.loads((path.parent/'source/factor-build.json').read_text())==build
        assert all(flag in build['command'] for flag in ('-std=c11','-Wall','-Wextra','-Werror'))
    return receipt

def main():
    output=DATA/'checkpoint-audit.json';assert not output.exists()
    predecessor=json.loads((DATA/'predecessor.json').read_text())
    assert sha(Path(predecessor['audit_path']))==predecessor['audit_sha256']=='65f48c10c0fc050c9b18fd97d510890c1be3fac907b29645c1476b7b40c014d0'
    prior=json.loads(Path(predecessor['audit_path']).read_text())
    for q,h in prior['source_sha256'].items():assert sha(ROOT/q)==h
    assert sha(ROOT/'build/c3d-mixed-precision/readback-correction.json')==predecessor['correction_sha256']
    for key in ('reference_receipts_sha256','control_receipts_sha256','stage_receipts_sha256','observer_receipts_sha256'):
        for q,h in prior[key].items():
            assert sha(Path(q))==h
            receipt=json.loads(Path(q).read_text())
            for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
    for q,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/q)==h
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[q for q,h in baseline.items() if sha(ROOT/q)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['mesh_tests']==2 and tests['signed_tests']==4
    for name,h in tests['logs_sha256'].items():
        log=DATA/name;assert sha(log)==h and '\nOK\n' in log.read_text()
        assert f"Ran {2 if name=='mesh-tests.log' else 4} tests" in log.read_text()
    for q,h in tests['source_sha256'].items():assert sha(ROOT/q)==h
    local=json.loads((DATA/'local-test-receipt.json').read_text());assert local['test_count']==3
    log=DATA/'local-tests-02.log';assert sha(log)==local['log_sha256'] and 'Ran 3 tests' in log.read_text() and '\nOK\n' in log.read_text()
    assert sha(DATA/'local-tests-01.log')==local['retained_initial_log_sha256']
    for q,h in local['source_sha256'].items():assert sha(ROOT/q)==h
    stagepath=next((DATA/'stage-runs').glob('*/*-receipt.json'));sr=frozen(stagepath)
    stage=json.loads(stagepath.with_name(stagepath.name.removesuffix('-receipt.json')+'.json').read_text())
    assert stage['diagnostic_accepted'] and stage['symbolic_handle_cleanup_verified'] and stage['original_mixed_input_preserved']
    assert stage['tetrahedra']==33216 and stage['count']==6 and stage['split_first_normal'] and stage['outer_layers']==3 and stage['domain_mesh_mode']=='held_l4'
    assert not any(stage[k] for k in ('numeric_factor_attempted','numerically_accepted','numerical_field_published','physical_accuracy_certified'))
    assert not stagepath.with_name(stagepath.name.removesuffix('-receipt.json')+'.npz').exists()
    admission=stage['admission'];assert admission['estimated_numeric_stage_bytes']==sum(admission[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes'))
    assert admission['reserve_bytes']==32*1024**2 and not admission['numeric_stage_admitted'] and admission['estimated_numeric_stage_bytes']>1800*1024**2
    assert admission['pressure']['live_input_preserved'] and admission['pressure']['action_preserved']
    assert stage['identity']['pressure_coupling_bitwise_preserved'] and stage['identity']['conversion_full_mixed_relative_action_change']<1e-12
    op=next((DATA/'observer-runs').glob('*/*-receipt.json'));obsreceipt=frozen(op)
    row=json.loads(op.with_name(op.name.removesuffix('-receipt.json')+'.json').read_text())
    assert row['diagnostic_accepted'] and row['input_complete_numerical_gates_passed'] and not row['physical_accuracy_certified']
    assert row['tetrahedra']==28416 and row['peak_rss_bytes']<1800*1024**2 and row['wall_s']<180
    inputpath=Path(row['input_receipt']);assert sha(inputpath)==row['input_receipt_sha256']==tests['accepted_input_receipt_sha256']
    _,original=verify_receipt(inputpath)
    assert original['numerically_accepted'] and original['final_residual']['true_residual']<=original['target']==1e-10
    assert original['outer_layers']==2 and original['outer_iteration']['restart']==60
    assert sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
    oldpath=next((ROOT/'build/c3d-mixed-precision/observer-runs').glob('*/*mixed-stress.json'));old=json.loads(oldpath.read_text())
    for key in ('lifts','volume_strong_equilibrium_defect_l2','interior_stress_jump_l2','h_squared_volume_defect_centroid_buckets','h_weighted_jump_defect_centroid_buckets','equilibrium_indicator_squared_per_tet'):
        assert row[key]==old[key],key
    maximum_identity=max(abs(v) for lift in row['lifts'] for part in ('pressure','viscous') for v in lift[part]['identity_error_n']);assert maximum_identity<1e-9
    signedpath=Path(row['signed_attribution_path']);assert sha(signedpath)==row['signed_attribution_sha256']==obsreceipt['artifact_sha256'][str(signedpath)]
    with np.load(signedpath,allow_pickle=False) as signed:
        volume=signed['weighted_volume_divergence_n'];faces=signed['interior_jump_n'];net=signed['cell_net_n'];score=signed['cell_score_n'];adj=signed['adjacency']
        assert volume.shape==net.shape==(2,2,3,28416) and faces.shape[:3]==(2,2,3) and adj.shape==(2,faces.shape[-1])
        assert np.all(np.isfinite(net)) and np.all(np.isfinite(score)) and np.all(score>=0)
        assert np.all((adj>=0)&(adj<28416)) and np.all(adj[0]!=adj[1])
        np.testing.assert_array_equal(score,np.max(np.abs(net.sum(axis=1)[:,0]),axis=0))
        np.testing.assert_allclose(net.sum(axis=-1),faces.sum(axis=-1)-volume.sum(axis=-1),rtol=1e-11,atol=1e-12)
        for i,lift in enumerate(row['lifts']):
            for j,part in enumerate(('pressure','viscous')):
                np.testing.assert_allclose(volume[i,j].sum(axis=-1),lift[part]['weighted_volume_divergence_n'],rtol=1e-10,atol=1e-12)
                np.testing.assert_allclose(faces[i,j].sum(axis=-1),lift[part]['interior_jump_n'],rtol=1e-10,atol=1e-12)
            target=np.sum([lift[p]['raw_minus_weak_n'] for p in ('pressure','viscous')],axis=0)
            np.testing.assert_allclose(net[i].sum(axis=(0,2)),target,rtol=1e-10,atol=1e-12)
            np.testing.assert_allclose(net[i].sum(axis=(0,2)),np.array(original['pressure_force_n'])+original['raw_symmetric_viscous_force_n']-np.array(original['reaction_force_n']),rtol=1e-8,atol=1e-10)
            summary=row['signed_force_attribution']['lifts'][i]
            np.testing.assert_allclose(np.sum([b['signed_total_n'] for b in summary['centroid_bands']],axis=0),target,rtol=1e-10,atol=1e-12)
            assert sum(b['cell_count'] for b in summary['centroid_bands'])==28416
    surveys=[]
    for suffix,dirname in (('','geometry-survey-source-v1'),('-v2','geometry-survey-source-v2')):
        path=DATA/('force-local-geometry-survey'+suffix+'.json');survey=json.loads(path.read_text())
        for name,h in survey['source_sha256'].items():
            assert sha(DATA/dirname/name)==h
            if suffix:assert sha(ROOT/'scripts'/name)==h
        assert sha(Path(survey['diagnostic_path']))==survey['diagnostic_sha256']
        assert survey['owned_peak_rss_bytes']<1800*1024**2 and survey['wall_s']<180
        assert len(survey['candidates'])==16 and not survey['numeric_factor_attempted'] and not survey['physical_accuracy_certified']
        assert not any(c['geometry_admitted'] for c in survey['candidates'])
        for c in survey['candidates']:
            m=c['metadata'];assert 'affected intrinsic worst shape worsened' in c['reasons']
            assert m['refined_affected_worst_shape']>m['original_affected_worst_shape']*(1+1e-8)
            assert m['observer_receipt_sha256']==sha(op) and m['signed_attribution_sha256']==sha(signedpath)
            assert m['original_tetrahedra']==28416 and m['refined_tetrahedra']<=50000
            for name,area in {'body':6.,'inlet':4.,'outlet':4.,'walls':64.}.items():assert abs(m['boundary_areas_m2'][name]-area)<1e-9
        surveys.append(survey)
    assert surveys[0]['candidates']==surveys[1]['candidates']
    new_sources=('scripts/audit_cfd_3d_end_plateau.py','scripts/cfd_reference3d_force_local_mesh.py','scripts/cfd_reference3d_force_attribution.py','docs/cfd_3d_end_plateau_goal.md','docs/cfd_3d_signed_force_attribution_goal.md','docs/cfd_3d_force_local_geometry_goal.md','docs/cfd_3d_end_plateau_checkpoint.md','docs/cfd_3d_force_transition_quality_goal.md')
    audit=dict(schema='physics_sim_c3d_end_plateau_audit_v1',status='resource_gap_preserved_signed_force_attribution_verified_geometry_rejected',prior_turn_classification='progress',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,new_fields_published=False,reference_diagnostic_adopted=True,new_mesh_adopted=False,
        predecessor_audit_sha256=predecessor['audit_sha256'],baseline_sha256=sha(DATA/'baseline.json'),changed_preexisting_files=changed,stage_receipt_sha256={str(stagepath):sha(stagepath)},stage_admission=admission,end3_force_plateau_established=False,
        signed_observer_receipt_sha256={str(op):sha(op)},signed_npz_sha256=sha(signedpath),maximum_signed_identity_error_n=maximum_identity,original_stress_rows_preserved_exactly=True,signed_force_summary=row['signed_force_attribution'],observer_cost=dict(supervisor_wall_s=obsreceipt['wall_s'],sampled_peak_rss_bytes=obsreceipt['peak_observed_rss_bytes'],owned_peak_rss_bytes=row['peak_rss_bytes']),
        geometry_survey_sha256={str(DATA/('force-local-geometry-survey'+suffix+'.json')):sha(DATA/('force-local-geometry-survey'+suffix+'.json')) for suffix in ('','-v2')},geometry_admitted_count=0,geometry_rejected_count=16,test_count=9,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),local_test_receipt_sha256=sha(DATA/'local-test-receipt.json'),source_sha256={q:sha(ROOT/q) for q in new_sources},native_source_unchanged=True,committed=False,packaged=False,installed=False,next_gate='bounded force-local transition-cell quality optimization before unchanged fullFE/resource/force tests')
    output.write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps({k:audit[k] for k in ('status','test_count','geometry_rejected_count','maximum_signed_identity_error_n','next_gate')}),flush=True)
if __name__=='__main__':main()
