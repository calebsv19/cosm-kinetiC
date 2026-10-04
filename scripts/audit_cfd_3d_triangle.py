#!/usr/bin/env python3
"""Audit explicit triangle storage, original equations and force refinement."""
import json
import hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_graded import sha
from audit_cfd_3d_bounded import observer as bounded_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-triangle'

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
    predecessor_path=ROOT/'build/c3d-bounded/checkpoint-audit.json'
    predecessor=json.loads(predecessor_path.read_text())
    for p,h in predecessor['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in predecessor['observer_receipts_sha256'].items():assert sha(Path(p))==h;bounded_observer(Path(p))
    protected=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in protected['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text())
    assert tests['test_count']==7 and sha(DATA/'support-tests-final.log')==tests['log_sha256']
    assert tests['library_sha256']==sha(ROOT/'build/c3d-cholesky/support/factor.dylib')
    log=(DATA/'support-tests-final.log').read_text();assert 'Ran 7 tests' in log and chr(10)+'OK'+chr(10) in log
    for n,h in tests['source_sha256'].items():assert sha(ROOT/n)==h
    rows={};receipts={};paths={};hashes={};costs={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json')
        assert name not in rows;rows[name]=row;receipts[name]=receipt;paths[name]=path;hashes[str(path)]=sha(path)
        for source_name,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/source_name)==h
        library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
        assert library.parent==path.parent/'source'
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','stop_reason','diagnostic_failure')}
        if row and receipt['returncode']==0:
            assert row['linear_solve_accepted'] and not row['numerical_failure_reasons']
            assert (row['velocity_degree'],row['pressure_degree'])==(4,3)
            assert row['condensation']['maximum_local_elimination_residual']<1e-10
            assert row['condensation']['maximum_local_schur_asymmetry']<1e-10
            assert row['preconditioner']['operator_representation']=='T*x + T.T*x - diag(T)*x; all mixed entries retained'
            assert row['preconditioner']['scaling']=='none'
            assembly=row['condensation']['assembly'];assert assembly['coo_batch_allocation_bytes']==64*103*104//2*16
            costs[name].update(owned_peak_rss_bytes=row['peak_rss_bytes'],tetrahedra=row['tetrahedra'],iterations=row['iterations'],
                full_residual=row['final_residual'],phase_timings=row['timings'],factor_storage_bytes=row['preconditioner']['symbolic_factor_storage_bytes'],assembly=assembly)
    assert {'L4-body4-normal-triangle','L4-body6-base-triangle'}<=rows.keys()
    equivalent={};deltas={};matched_cost={}
    for newname,oldname in (('L4-body4-normal-triangle','L4-body4-normal-bounded'),('L4-body6-base-triangle','L4-body6-base-bounded')):
        oldpath=next((ROOT/'build/c3d-bounded/runs').glob('*/'+oldname+'-receipt.json'));oldreceipt,old=verify_receipt(oldpath);new=rows[newname]
        assert receipts[newname]['returncode']==0
        for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert new['identity'][key]==old['identity'][key]
        comparison=force_comparison(new,old)
        assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
        equivalent[newname]=comparison
        deltas[newname]=fields(paths[newname].with_name(newname+'.npz'),oldpath.with_name(oldname+'.npz'))
        matched_cost[newname]=dict(owned_memory_reduction=1-new['peak_rss_bytes']/old['peak_rss_bytes'],total_time_ratio=receipts[newname]['wall_s']/oldreceipt['wall_s'],
            old_owned_rss_bytes=old['peak_rss_bytes'],old_wall_s=oldreceipt['wall_s'])
    refinement={};normal_admitted=False
    if 'L4-body6-normal-triangle' in rows:
        normal=rows['L4-body6-normal-triangle'];normal_admitted=receipts['L4-body6-normal-triangle']['returncode']==0
        if normal_admitted:
            assert normal['tetrahedra']==23616 and normal['axis_nodes_m'][1:]==rows['L4-body6-base-triangle']['axis_nodes_m'][1:]
            refinement['L4_body6_normal']=force_comparison(normal,rows['L4-body6-base-triangle'])
    if 'L8-body6-base-held-triangle' in rows:
        longrow=rows['L8-body6-base-held-triangle']
        if receipts['L8-body6-base-held-triangle']['returncode']==0:
            base=rows['L4-body6-base-triangle']
            assert longrow['tetrahedra']==base['tetrahedra']==18816
            assert longrow['axis_nodes_m'][1:]==base['axis_nodes_m'][1:]
            np.testing.assert_allclose(np.array(longrow['axis_nodes_m'][0][1:-1])-2,np.array(base['axis_nodes_m'][0][1:-1]),atol=1e-14,rtol=0)
            domain=force_comparison(longrow,base)
            domain['domain_component_and_raw_gate_passed']=max([*domain['component_relative_changes'].values(),*domain['raw_surface_reaction_relative_mismatch'].values()])<=.01
            domain['scalar_scope']='length-dependent inlet pressure and dissipation; not flat domain qualification criteria'
            refinement['matched_L4_to_L8_body6_base']=domain
    observers={};observer_hashes={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        receipt,row=observer(path);name=path.name.removesuffix('-receipt.json');observer_hashes[str(path)]=sha(path)
        observers[name]=dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=row['interior_stress_jump_l2'],
            maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for p in ('pressure','viscous') for v in lift[p]['identity_error_n']))
    assert {'L4-body6-base-triangle-stress','L8-body6-base-held-triangle-stress'}==observers.keys()
    assert set(rows)=={'L4-body4-normal-triangle','L4-body6-base-triangle','L4-body6-normal-triangle','L8-body6-base-held-triangle'}
    assert not normal_admitted and receipts['L4-body6-normal-triangle']['diagnostic_failure']['kind']=='resource_cap'
    readiness=json.loads((DATA/'matched-control-readiness.json').read_text())
    for name,value in matched_cost.items():
        assert value['owned_memory_reduction']==readiness[name]['owned_memory_reduction']
        assert value['total_time_ratio']==readiness[name]['total_time_ratio']
    assert matched_cost['L4-body6-base-triangle']['owned_memory_reduction']>.07
    audit=dict(schema='physics_sim_c3d_triangle_audit_v1',status='exact_triangle_storage_measured_force_gates_open',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,physical_mesh_adopted=False,
        native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,
        optional_reference_triangle_adopted=True,triangle_adoption_scope='new optional memory-constrained larger controls; measured body6 base benefit with retained time and small-case memory tradeoffs',
        reference_receipts_sha256=hashes,observer_receipts_sha256=observer_hashes,costs=costs,equivalence=equivalent,field_differences=deltas,
        matched_cost=matched_cost,finer_normal_admitted=normal_admitted,force_refinement=refinement,stress_defects=observers,
        matched_readiness_sha256=sha(DATA/'matched-control-readiness.json'),test_count=7,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),test_log_sha256={p.name:sha(p) for p in DATA.glob('*tests*.log')},
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor_path),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='shared velocity-triangle factor input proof and matched memory control before retrying exact finer normal mesh')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+chr(10))
    print(json.dumps({k:audit[k] for k in ('status','matched_cost','finer_normal_admitted','force_refinement','stress_defects')},indent=2),flush=True)


if __name__=='__main__':main()
