#!/usr/bin/env python3
"""Audit and reject the insufficient allocator pressure control from measured cost."""
import json
import hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_graded import sha
from audit_cfd_3d_shared_factor import observer as prior_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-factor-peak'

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



def fields(new_path,old_path):
    with np.load(new_path,allow_pickle=False) as new,np.load(old_path,allow_pickle=False) as old:
        np.testing.assert_array_equal(new['vertices_m'],old['vertices_m']);np.testing.assert_array_equal(new['tetrahedra'],old['tetrahedra'])
        delta={key:float(np.max(np.abs(new[key]-old[key]))) for key in ('velocity_coefficients','pressure_coefficients')}
    assert delta['velocity_coefficients']<1e-8 and delta['pressure_coefficients']<1e-6
    return delta


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor_path=ROOT/'build/c3d-shared-factor/checkpoint-audit.json'
    predecessor=json.loads(predecessor_path.read_text())
    for p,h in predecessor['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in predecessor['observer_receipts_sha256'].items():assert sha(Path(p))==h;prior_observer(Path(p))
    protected=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in protected['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text())
    assert tests['test_count']==5 and sha(DATA/'support-tests.log')==tests['log_sha256']
    log=(DATA/'support-tests.log').read_text();assert 'Ran 5 tests' in log and chr(10)+'OK'+chr(10) in log
    for n,h in tests['source_sha256'].items():assert sha(ROOT/n)==h
    assert tests['library_sha256']==sha(ROOT/'build/c3d-cholesky/support/factor.dylib')
    assert tests['api_receipt_sha256']==sha(DATA/'support/api-receipt.json')
    api=json.loads((DATA/'support/api-receipt.json').read_text())
    assert tests['sdk_header_sha256']==sha(DATA/'support/malloc.h')==api['header_sha256']
    assert api['api']=='malloc_zone_pressure_relief(NULL,0)'
    paths=list((DATA/'runs').glob('*/*-receipt.json'));assert len(paths)==1
    path=paths[0];receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json')
    assert name=='L4-body6-base-pressure' and receipt['returncode']==0
    for n,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/n)==h
    library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
    assert library.parent==path.parent/'source'
    assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
    assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
    assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
    assert row['linear_solve_accepted'] and not row['numerical_failure_reasons'] and row['tetrahedra']==18816
    assert row['preconditioner']['shared_input_preserved_after_factor'] and row['preconditioner']['shared_input_preserved_after_solve']
    pressure=row['allocator_pressure_relief']
    assert pressure['live_input_preserved'] and pressure['action_preserved'] and pressure['action_max_absolute_change']==0
    assert pressure['owned_high_water_after_bytes']>=pressure['owned_high_water_before_bytes']
    assert pressure['reported_released_bytes']>=0 and pressure['pressure_call_wall_s']<=pressure['total_control_wall_s']
    oldname='L4-body6-base-shared';oldpath=next((ROOT/'build/c3d-shared-factor/runs').glob('*/'+oldname+'-receipt.json'));oldreceipt,old=verify_receipt(oldpath)
    for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][k]==old['identity'][k]
    equivalence=force_comparison(row,old)
    assert max([*equivalence['component_relative_changes'].values(),*equivalence['scalar_relative_changes'].values()])<1e-7
    delta=fields(path.with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
    memory_reduction=1-row['peak_rss_bytes']/old['peak_rss_bytes'];time_ratio=receipt['wall_s']/oldreceipt['wall_s']
    assert memory_reduction<.05
    audit=dict(schema='physics_sim_c3d_factor_peak_audit_v1',status='allocator_pressure_control_rejected_insufficient_total_headroom',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,physical_mesh_adopted=False,
        allocator_pressure_control_adopted=False,normal_retry_attempted=False,native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,
        reference_receipts_sha256={str(path):sha(path)},observer_receipts_sha256={},preserved_observer_receipts_sha256=predecessor['observer_receipts_sha256'],
        matched_owned_memory_reduction=memory_reduction,matched_total_time_ratio=time_ratio,pressure_observation=pressure,equivalence=equivalence,field_differences=delta,
        cost=dict(wall_s=receipt['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],peak_observed_rss_bytes=receipt['peak_observed_rss_bytes'],iterations=row['iterations'],full_residual=row['final_residual'],phase_timings=row['timings']),
        test_count=5,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor_path),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='exact symbolic storage/workspace diagnostics and bounded vector-graph ordering controls')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+chr(10))
    print(json.dumps({k:audit[k] for k in ('status','matched_owned_memory_reduction','matched_total_time_ratio','pressure_observation','field_differences')},indent=2))


if __name__=='__main__':main()
