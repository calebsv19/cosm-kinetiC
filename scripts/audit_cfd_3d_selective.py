#!/usr/bin/env python3
"""Audit selective outer refinement without mistaking linear success for accuracy."""
import json
import hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_domain import observe as domain_observe
from audit_cfd_3d_graded import sha
from cfd_reference3d_selective_mesh import selective_mesh
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-selective'


def observer(path):
    receipt=json.loads(path.read_text())
    assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
    assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==h
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    name=path.name.removesuffix('-receipt.json');row=json.loads(path.with_name(name+'.json').read_text())
    assert row['diagnostic_accepted'] and row['input_complete_numerical_gates_passed'] and not row['physical_accuracy_certified']
    assert row['peak_rss_bytes']<1800*1024**2 and row['wall_s']<180
    source=Path(row['input_receipt']);assert sha(source)==row['input_receipt_sha256'];original=verify_receipt(source)[1]
    assert sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
    scores=np.array(row['equilibrium_indicator_squared_per_tet'])
    assert len(scores)==row['tetrahedra'] and np.all(np.isfinite(scores)) and np.all(scores>=0)
    assert abs(scores.sum()-sum(row['h_squared_volume_defect_centroid_buckets'])-sum(row['h_weighted_jump_defect_centroid_buckets']))<1e-12
    for lift,old in zip(row['lifts'],original['consistency_diagnostics']['volume_lifts']):
        assert abs(sum(lift[p]['weak_load_n'][0] for p in ('pressure','viscous'))-old['symmetric_stress_load_n'])<1e-9
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            assert np.max(np.abs(np.array(lift[part]['raw_surface_load_n'])-original[key]))<1e-10
            assert np.max(np.abs(lift[part]['identity_error_n']))<1e-9
    return receipt,row


def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=json.loads((ROOT/'build/c3d-domain/checkpoint-audit.json').read_text())
    for p,h in predecessor['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in predecessor['observer_receipts_sha256'].items():assert sha(Path(p))==h;domain_observe(Path(p))
    older=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in older['protected_build_hashes'].items():assert sha(ROOT/p)==h
    diagnostic=next((ROOT/'build/c3d-domain/observer-runs').glob('*/L8-held-normal-stress.json'))
    base_path=next((ROOT/'build/c3d-domain/runs').glob('*/L8-held-normal-receipt.json'))
    base_receipt,base=verify_receipt(base_path);old_stress=json.loads(diagnostic.read_text())
    rows={};hashes={};costs={};comparisons={};geometry={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');assert name not in rows
        library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
        assert library.parent==path.parent/'source'
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
        rows[name]=row;hashes[str(path)]=sha(path)
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','diagnostic_failure','stop_reason')}
        if row is None or not row['numerically_accepted']:continue
        assert row['numerical_failure_reasons']==[] and row['linear_solve_accepted']
        assert (row['length'],row['count'],row['body'],row['split_first_normal'])==(8.,4,True,True)
        assert (row['velocity_degree'],row['pressure_degree'])==(4,3) and row['domain_mesh_mode']=='held_l4'
        assert row['condensation']['maximum_local_elimination_residual']<1e-10 and row['condensation']['maximum_local_schur_asymmetry']<1e-10
        for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n'):assert np.max(np.abs(row[key][1:]))<1e-8
        metadata=row['selective_refinement'];(mesh,*_),rebuilt=selective_mesh(diagnostic,metadata['marked_macros_per_octant'])
        assert metadata==rebuilt
        with np.load(path.with_name(name+'.npz'),allow_pickle=False) as saved:
            np.testing.assert_array_equal(saved['vertices_m'],mesh.p);np.testing.assert_array_equal(saved['tetrahedra'],mesh.t)
        assert row['axis_nodes_m']==base['axis_nodes_m']
        costs[name].update(iterations=row['iterations'],tetrahedra=row['tetrahedra'],owned_peak_rss_bytes=row['peak_rss_bytes'],full_residual=row['final_residual'],factor_storage_bytes=row['preconditioner']['symbolic_factor_storage_bytes'])
        comparisons[name]=force_comparison(row,base);geometry[name]=dict(refinement=metadata,base_condition=base['diagnostics'],refined_condition=row['diagnostics'])
    assert {'L8-held-outer-select2'}==rows.keys()
    candidate=rows['L8-held-outer-select2'];assert candidate['numerically_accepted'] and candidate['tetrahedra']==14208 and candidate['iterations']==330
    assert candidate['diagnostics']['Jacobian_condition_max']>base['diagnostics']['Jacobian_condition_max']
    assert comparisons['L8-held-outer-select2']['raw_surface_reaction_relative_mismatch']['refined']>comparisons['L8-held-outer-select2']['raw_surface_reaction_relative_mismatch']['base']
    assert not comparisons['L8-held-outer-select2']['physical_force_gate_passed']
    stress={};observer_hashes={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        receipt,row=observer(path);name=path.name.removesuffix('-receipt.json');observer_hashes[str(path)]=sha(path)
        stress[name]=dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=row['interior_stress_jump_l2'],
            volume_defect_ratio_to_base=row['volume_strong_equilibrium_defect_l2']/old_stress['volume_strong_equilibrium_defect_l2'],
            jump_defect_ratio_to_base=row['interior_stress_jump_l2']/old_stress['interior_stress_jump_l2'],
            total_weighted_indicator=float(np.array(row['equilibrium_indicator_squared_per_tet']).sum()),
            maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for p in ('pressure','viscous') for v in lift[p]['identity_error_n']))
    assert {'L8-held-outer-select2-stress'}==stress.keys()
    assert stress['L8-held-outer-select2-stress']['volume_defect_ratio_to_base']>1 and stress['L8-held-outer-select2-stress']['jump_defect_ratio_to_base']>1
    survey=json.loads((DATA/'mesh-survey.json').read_text())
    assert len(survey)==4
    for record in survey:
        (mesh,*_),metadata=selective_mesh(diagnostic,record['marked_macros_per_octant'])
        for key,value in metadata.items():
            if key in record:assert record[key]==value
        assert mesh.nelements==record['refined_tetrahedra']
    log=(DATA/'mesh-tests.log').read_text();assert 'Ran 4 tests' in log and '\nOK\n' in log and 'FAILED (' not in log
    audit=dict(schema='physics_sim_c3d_selective_audit_v1',status='selective_outer_candidate_numerically_admitted_but_accuracy_rejected',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,physical_candidate_adopted=False,
        native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,reference_receipts_sha256=hashes,observer_receipts_sha256=observer_hashes,
        costs=costs,force_comparisons=comparisons,geometry_and_conditioning=geometry,stress_defects=stress,
        larger_candidates_not_run_reason='small paired refinement increases conditioning, volume/jump defects and raw/reaction mismatch',
        numerical_field_retained=True,force_error_bound_claimed=False,test_count=4,
        test_log_sha256=sha(DATA/'mesh-tests.log'),mesh_survey_sha256=sha(DATA/'mesh-survey.json'),geometry_construction_rejection_sha256=sha(DATA/'mesh-survey-rejection.json'),
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(ROOT/'build/c3d-domain/checkpoint-audit.json'),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='prove a body/corner refinement with improved macro shape, then repeat exact raw-force and matched-domain gates within original caps')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','costs','force_comparisons','stress_defects')}),flush=True)


if __name__=='__main__':main()
