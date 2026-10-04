#!/usr/bin/env python3
"""Audit quartic stress attribution and measured mesh-conditioning controls."""
import hashlib
import json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-graded'


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda:source.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()


def verify_solver(path):
    # One pre-correction immutable receipt mislabeled linear success as full
    # acceptance. Retain and reject it explicitly rather than relaxing a gate.
    if path.name!='outer3-L4-normal-receipt.json':return verify_receipt(path)
    receipt=json.loads(path.read_text());directory=path.parent
    for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
    for name,digest in receipt['source_sha256'].items():assert sha(directory/'source'/name)==digest
    assert directory.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180 and receipt['linear_iteration_cap']==3000
    assert receipt['returncode']==0 and receipt['diagnostic_failure'] is None and receipt['stop_reason'] is None
    assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
    row=json.loads(path.with_name('outer3-L4-normal.json').read_text())
    assert row['numerically_accepted'] and not row['physical_accuracy_certified']
    assert row['final_residual']['true_residual']<1e-8 and row['flux_error']<1e-8 and row['physical_energy_imbalance']<.03
    assert row['volume_divergence_max_s_inv']>=1e-8
    assert row['tetrahedra']<=50000 and row['iterations']<=3000 and row['peak_rss_bytes']<1800*1024**2
    with np.load(path.with_name('outer3-L4-normal.npz'),allow_pickle=False) as field:
        assert np.all(np.isfinite(field['velocity_coefficients'])) and np.all(np.isfinite(field['pressure_coefficients']))
    return receipt,row


def verify_observer(path):
    receipt=json.loads(path.read_text())
    if path.name=='reject-incomplete-input-receipt.json':
        assert receipt['returncode']==1 and receipt['stop_reason'] is None
        assert receipt['diagnostic_failure']['kind']=='observer_error'
        for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
        for name,digest in receipt['source_sha256'].items():assert sha(path.parent/'source'/name)==digest
        assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
        assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
        assert not path.with_name('reject-incomplete-input.json').exists()
        text=path.with_name('reject-incomplete-input.log').read_text()
        assert "assert original['flux_error']<1e-8 and original['volume_divergence_max_s_inv']<1e-8" in text and 'AssertionError' in text
        input_path=Path(receipt['command'][receipt['command'].index('--input-receipt')+1])
        assert input_path.name=='outer3-L4-normal-receipt.json'
        verify_solver(input_path)
        return receipt,None
    assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
    assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
    for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
    for name,digest in receipt['source_sha256'].items():assert sha(path.parent/'source'/name)==digest
    bundle=hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert bundle==path.parent.name
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    name=path.name.removesuffix('-receipt.json');row=json.loads(path.with_name(name+'.json').read_text())
    assert row['diagnostic_accepted'] and not row['physical_accuracy_certified']
    assert (row['velocity_degree'],row['pressure_degree'])==(4,3)
    assert row['tetrahedra']<=50000 and row['peak_rss_bytes']<1800*1024**2 and row['wall_s']<180
    input_path=Path(row['input_receipt']);assert sha(input_path)==row['input_receipt_sha256']
    original=verify_solver(input_path)[1]
    assert sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
    scores=np.array(row['equilibrium_indicator_squared_per_tet'])
    assert len(scores)==row['tetrahedra'] and np.all(np.isfinite(scores)) and np.all(scores>=0)
    assert abs(scores.sum()-sum(row['h_squared_volume_defect_centroid_buckets'])-sum(row['h_weighted_jump_defect_centroid_buckets']))<1e-12
    for lift,old in zip(row['lifts'],original['consistency_diagnostics']['volume_lifts']):
        weak=sum(lift[part]['weak_load_n'][0] for part in ('pressure','viscous'))
        assert abs(weak-old['symmetric_stress_load_n'])<1e-9
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            assert np.max(np.abs(np.array(lift[part]['raw_surface_load_n'])-original[key]))<1e-10
            assert np.max(np.abs(lift[part]['identity_error_n']))<1e-9
    return receipt,row


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,digest in baseline.items() if sha(ROOT/p)!=digest]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    rows={};costs={};solver_hashes={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_solver(path);name=path.name.removesuffix('-receipt.json')
        assert name not in rows and row is not None
        assert (row['velocity_degree'],row['pressure_degree'])==(4,3)
        local=row['diagnostics']['local_macro_pressure_modes'];global_check=row['diagnostics']['global_macro_pressure_modes']
        assert local['all_mean_zero_modes_detected'] and local['rank_counts']=={'79':local['macro_count']}
        assert local['maximum_constant_gradient_relative_error']<1e-12
        assert local['minimum_positive_generalized_eigenvalue']>1e-6
        assert global_check['near_null_modes_below_1e_12']==0
        assert min(global_check['smallest_eigenvalues'])>1e-6
        rows[name]=row;solver_hashes[str(path)]=sha(path)
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes')}
        costs[name].update(tetrahedra=row['tetrahedra'],iterations=row['iterations'])
    required={'original-modes-control','original-publication-control','balanced-L4','balanced-L4-normal','balanced-L4-body4','outer3-L4','outer3-L4-normal','outer3-L4-normal-tight'}
    assert required<=rows.keys()
    original=rows['original-modes-control']
    prior=next((ROOT/'build/c3d-quartic/runs').glob('*/cube-L4.json'))
    older=json.loads(prior.read_text());assert original['identity']==older['identity']
    publication=rows['original-publication-control']
    assert publication['identity']==original['identity'] and publication['linear_solve_accepted']
    assert publication['numerical_failure_reasons']==[]
    publication_equivalence=force_comparison(publication,original)
    assert max([*publication_equivalence['component_relative_changes'].values(),*publication_equivalence['scalar_relative_changes'].values()])<1e-7
    equivalent=force_comparison(original,older)
    assert max([*equivalent['component_relative_changes'].values(),*equivalent['scalar_relative_changes'].values()])<1e-7
    comparisons={'balanced_spacing':force_comparison(rows['balanced-L4'],original),
        'balanced_normal':force_comparison(rows['balanced-L4-normal'],rows['balanced-L4']),
        'balanced_body4':force_comparison(rows['balanced-L4-body4'],rows['balanced-L4']),
        'outer3_spacing':force_comparison(rows['outer3-L4'],original),
        'outer3_normal':force_comparison(rows['outer3-L4-normal-tight'],rows['outer3-L4'])}
    assert all(not row['physical_force_gate_passed'] for row in comparisons.values())
    assert original['tetrahedra']==4992 and rows['balanced-L4-normal']['tetrahedra']==6720
    assert rows['balanced-L4-body4']['tetrahedra']==10752
    assert rows['outer3-L4']['tetrahedra']==8448 and rows['outer3-L4-normal-tight']['tetrahedra']==10176
    tight=rows['outer3-L4-normal-tight'];failed=rows['outer3-L4-normal']
    assert tight['identity']==failed['identity'] and tight['linear_solve_accepted']
    assert tight['numerical_failure_reasons']==[] and tight['volume_divergence_max_s_inv']<1e-8
    residual_equivalence=force_comparison(tight,failed)
    assert max([*residual_equivalence['component_relative_changes'].values(),*residual_equivalence['scalar_relative_changes'].values()])<1e-7
    assert rows['balanced-L4']['iterations']<original['iterations']
    for a,b in zip(rows['outer3-L4']['axis_nodes_m'][1:],original['axis_nodes_m'][1:]):assert a==b
    assert set(original['axis_nodes_m'][0]).issubset(rows['outer3-L4']['axis_nodes_m'][0])
    observers={};observer_hashes={};observer_costs={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        receipt,row=verify_observer(path);name=path.name.removesuffix('-receipt.json')
        assert name not in observers
        observer_hashes[str(path)]=sha(path)
        if row is None:continue
        observers[name]=row
        observer_costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes')}
    assert {'quartic-original','quartic-normal','quartic-body4','balanced','balanced-body4','outer3','outer3-normal','outer3-normal-tight'}<=observers.keys()
    defects={name:dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],
        interior_stress_jump_l2=row['interior_stress_jump_l2'],weighted_indicator_squared=sum(row['equilibrium_indicator_squared_per_tet']),
        raw_minus_weak_force_n=sum(row['lifts'][0][p]['raw_minus_weak_n'][0] for p in ('pressure','viscous')),
        maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for part in ('pressure','viscous') for v in lift[part]['identity_error_n']))
        for name,row in observers.items()}
    assert defects['balanced']['volume_equilibrium_defect_l2']<defects['quartic-original']['volume_equilibrium_defect_l2']
    assert abs(defects['balanced']['raw_minus_weak_force_n'])>abs(defects['quartic-original']['raw_minus_weak_force_n'])
    for filename,count in (('observer-tests.log',4),('mesh-tests.log',6)):
        log=(DATA/filename).read_text()
        assert f'Ran {count} tests' in log and '\nOK\n' in log and 'Traceback' not in log and 'FAILED (' not in log
    predecessor=json.loads((ROOT/'build/c3d-quartic/checkpoint-audit.json').read_text())
    for p,digest in predecessor['reference_receipts_sha256'].items():assert sha(Path(p))==digest;verify_receipt(Path(p))
    equilibrium=json.loads((ROOT/'build/c3d-equilibrium/checkpoint-audit.json').read_text())
    for p,digest in equilibrium['observer_receipts_sha256'].items():
        path=Path(p);assert sha(path)==digest
        receipt=json.loads(path.read_text())
        for artifact,value in receipt['artifact_sha256'].items():assert sha(Path(artifact))==value
        for name,value in receipt['source_sha256'].items():assert sha(path.parent/'source'/name)==value
    spatial=json.loads((ROOT/'build/c3d-spatial/checkpoint-audit.json').read_text())
    for p,digest in spatial['reference_receipts_sha256'].items():assert sha(Path(p))==digest;verify_receipt(Path(p))
    method=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,digest in method['protected_build_hashes'].items():assert sha(ROOT/p)==digest
    sources={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*')
             if p.is_file() and str(p.relative_to(ROOT)) not in baseline}
    audit=dict(schema='physics_sim_c3d_graded_audit_v1',status='quartic_stress_and_pressure_modes_verified_mesh_force_gates_failed',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,
        reference_receipts_sha256=solver_hashes,observer_receipts_sha256=observer_hashes,
        costs=costs,observer_costs=observer_costs,force_comparisons=comparisons,stress_defects=defects,
        original_equivalence=equivalent,publication_control_equivalence=publication_equivalence,tighter_normal_force_equivalence=residual_equivalence,
        legacy_normal_complete_numerical_acceptance=False,
        atomic_publication_resource_regression_passed=True,incomplete_input_observer_rejection_verified=True,legacy_normal_failed_gate='volume_divergence',
        tighter_normal_requested_target_attained=tight['final_residual']['true_residual']<=tight['target'],current_source_sha256=sources,changed_preexisting_files=changed,
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(ROOT/'build/c3d-quartic/checkpoint-audit.json'),
        test_log_sha256={name:sha(DATA/name) for name in ('observer-tests.log','mesh-tests.log')},
        committed=False,packaged=False,installed=False,
        next_gate='prove exact local condensation/reconstruction on polynomial and matched original-cube controls, then use measured memory savings for finer raw-force resolution under original residual/resource gates')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','force_comparisons','stress_defects','predecessor_fields_and_workers_unchanged')}),flush=True)


if __name__=='__main__':main()
