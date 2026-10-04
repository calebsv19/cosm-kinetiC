#!/usr/bin/env python3
"""Audit exact bounded assembly, matched cost/equivalence and finer force evidence."""
import json
import hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_graded import sha
from audit_cfd_3d_selective import observer as prior_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-bounded'

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
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    prior=json.loads((ROOT/'build/c3d-corner/checkpoint-audit.json').read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    prior=json.loads((ROOT/'build/c3d-selective/checkpoint-audit.json').read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in prior['observer_receipts_sha256'].items():assert sha(Path(p))==h;prior_observer(Path(p))
    prior=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in prior['protected_build_hashes'].items():assert sha(ROOT/p)==h
    rows={};receipts={};paths={};hashes={};costs={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');assert name not in rows
        rows[name]=row;receipts[name]=receipt;paths[name]=path;hashes[str(path)]=sha(path)
        library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
        assert library.parent==path.parent/'source'
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
        assert row['numerically_accepted'] and row['linear_solve_accepted'] and not row['numerical_failure_reasons']
        assert (row['velocity_degree'],row['pressure_degree'])==(4,3)
        assert row['condensation']['maximum_local_elimination_residual']<1e-10 and row['condensation']['maximum_local_schur_asymmetry']<1e-10
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','stop_reason','diagnostic_failure')}
        costs[name].update(owned_peak_rss_bytes=row['peak_rss_bytes'],tetrahedra=row['tetrahedra'],iterations=row['iterations'],full_residual=row['final_residual'],phase_timings=row['timings'],factor_storage_bytes=row['preconditioner']['symbolic_factor_storage_bytes'])
        if row['schema']=='physics_sim_c3d_bounded_probe_v1':
            for source_name,digest in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/source_name)==digest
        if row['schema']=='physics_sim_c3d_bounded_probe_v1':
            assembly=row['condensation']['assembly'];assert assembly['macro_batch_cap']==64
            assert assembly['coo_batch_allocation_bytes']==64*103**2*16
            assert assembly['legacy_all_entry_coo_bytes']==row['tetrahedra']//4*103**2*16
            costs[name]['assembly']=assembly
    required={'L4-body4-normal-bounded','L4-body4-normal-fresh-control','L4-body6-base-bounded','L4-body4-base-bounded','original-L4-bounded-default'}
    assert required==rows.keys()
    test_receipt=json.loads((DATA/'support-test-receipt.json').read_text())
    assert sha(DATA/'support-tests-final.log')==test_receipt['log_sha256'] and test_receipt['test_count']==5
    for name,digest in test_receipt['source_sha256'].items():assert sha(ROOT/name)==digest
    equivalence={};deltas={};small_default_tradeoff=None
    for newname,oldname in (('L4-body4-normal-bounded','body4-normal'),('L4-body4-base-bounded','body4-zero-cache'),('original-L4-bounded-default','original-adopted-defaults')):
        oldpath=next((ROOT/'build/c3d-cholesky/runs').glob('*/'+oldname+'-receipt.json'));oldreceipt,old=verify_receipt(oldpath);new=rows[newname]
        for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert new['identity'][key]==old['identity'][key]
        comparison=force_comparison(new,old);equivalence[newname]=comparison
        assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
        deltas[newname]=fields(paths[newname].with_name(newname+'.npz'),oldpath.with_name(oldname+'.npz'))
        if newname=='original-L4-bounded-default':
            small_default_tradeoff=dict(new_owned_peak_rss_bytes=new['peak_rss_bytes'],historic_owned_peak_rss_bytes=old['peak_rss_bytes'],new_wall_s=receipts[newname]['wall_s'],historic_wall_s=oldreceipt['wall_s'],scope='historic small-cube comparison; bounded method is not a universal default promotion')
    a=rows['L4-body4-normal-bounded'];b=rows['L4-body4-normal-fresh-control']
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert a['identity'][key]==b['identity'][key]
    fresh_equivalence=force_comparison(a,b)
    assert max([*fresh_equivalence['component_relative_changes'].values(),*fresh_equivalence['scalar_relative_changes'].values()])<1e-7
    fresh_fields=fields(paths['L4-body4-normal-bounded'].with_name('L4-body4-normal-bounded.npz'),paths['L4-body4-normal-fresh-control'].with_name('L4-body4-normal-fresh-control.npz'))
    memory_reduction=1-a['peak_rss_bytes']/b['peak_rss_bytes'];assert memory_reduction>.15
    fresh_time_ratio=receipts['L4-body4-normal-bounded']['wall_s']/receipts['L4-body4-normal-fresh-control']['wall_s']
    failed_path=next((ROOT/'build/c3d-corner/runs').glob('*/L4-body6-base-receipt.json'));failed,_=verify_receipt(failed_path)
    identity=next(row['identity'] for row in failed['progress'] if row.get('phase')=='assembled')
    finer=rows['L4-body6-base-bounded'];assert finer['tetrahedra']==18816
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert finer['identity'][key]==identity[key]
    refinement=force_comparison(finer,rows['L4-body4-base-bounded']);assert not refinement['physical_force_gate_passed']
    assert refinement['raw_surface_reaction_relative_mismatch']['refined']<refinement['raw_surface_reaction_relative_mismatch']['base']
    stress={};observer_hashes={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        receipt,row=observer(path);name=path.name.removesuffix('-receipt.json');observer_hashes[str(path)]=sha(path)
        stress[name]=dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=row['interior_stress_jump_l2'],
            maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for p in ('pressure','viscous') for v in lift[p]['identity_error_n']))
    assert {'L4-body4-base-bounded-stress','L4-body6-base-bounded-stress'}==stress.keys()
    assert stress['L4-body6-base-bounded-stress']['volume_equilibrium_defect_l2']>stress['L4-body4-base-bounded-stress']['volume_equilibrium_defect_l2']
    assert stress['L4-body6-base-bounded-stress']['interior_stress_jump_l2']>stress['L4-body4-base-bounded-stress']['interior_stress_jump_l2']
    log=(DATA/'support-tests-final.log').read_text();assert 'Ran 5 tests' in log and chr(10)+'OK'+chr(10) in log and 'FAILED (' not in log
    audit=dict(schema='physics_sim_c3d_bounded_audit_v1',status='bounded_exact_assembly_adopted_finer_body_admitted_physical_gates_open',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,native_source_unchanged=True,
        predecessor_fields_and_workers_unchanged=True,optional_reference_bounded_assembly_adopted=True,bounded_adoption_scope='new optional large-mesh runner; earlier runner defaults remain unchanged',physical_mesh_adopted=False,
        reference_receipts_sha256=hashes,observer_receipts_sha256=observer_hashes,costs=costs,equivalence=equivalence,field_differences=deltas,
        fresh_control_equivalence=fresh_equivalence,fresh_control_field_differences=fresh_fields,fresh_owned_memory_reduction=memory_reduction,fresh_total_time_ratio=fresh_time_ratio,small_default_tradeoff=small_default_tradeoff,
        finer_mesh_identity_matches_resource_failure=True,force_refinement=refinement,stress_defects=stress,
        test_count=5,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),test_log_sha256={p.name:sha(p) for p in DATA.glob('*tests*.log')},baseline_sha256=sha(DATA/'baseline.json'),
        predecessor_audit_sha256=sha(ROOT/'build/c3d-corner/checkpoint-audit.json'),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='exact symmetric single-triangle storage proof and memory control before finer-body normal/domain qualification')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+chr(10))
    print(json.dumps({k:audit[k] for k in ('status','fresh_owned_memory_reduction','fresh_total_time_ratio','force_refinement','stress_defects')},indent=2),flush=True)


if __name__=='__main__':main()
