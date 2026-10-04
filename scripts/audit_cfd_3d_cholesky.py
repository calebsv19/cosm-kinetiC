#!/usr/bin/env python3
"""Audit optional sparse Cholesky, measured resource controls and raw-force gates."""
import json
import hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_graded import sha,verify_solver,verify_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-cholesky'


def verify_current_observer(path):
    receipt=json.loads(path.read_text());assert receipt['returncode']==0 and receipt['stop_reason'] is None
    assert receipt['diagnostic_failure'] is None and receipt['mesh_cap']==50000
    assert receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
    assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==h
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    name=path.name.removesuffix('-receipt.json');row=json.loads(path.with_name(name+'.json').read_text())
    assert row['diagnostic_accepted'] and row['input_complete_numerical_gates_passed'] and not row['physical_accuracy_certified']
    assert row['peak_rss_bytes']<1800*1024**2 and row['wall_s']<180
    input_path=Path(row['input_receipt']);assert sha(input_path)==row['input_receipt_sha256']
    original=verify_receipt(input_path)[1]
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
    changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    rows={};receipts={};hashes={};costs={};equivalence={};fields={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');assert name not in rows
        library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
        assert library.parent==path.parent/'source'
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256']
        assert json.loads(build.read_text())==receipt['factor_build']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
        command=receipt['factor_build']['command'];assert command[0]=='/usr/bin/clang' and '-std=c11' in command
        assert command[-4:]==['-framework','Accelerate','-o',str(library)]
        assert str(library.with_name('cfd_reference3d_accelerate.c')) in command
        assert receipt['factor_build']['compiler'] and receipt['factor_build']['platform'] and receipt['factor_build']['sdk']
        rows[name]=row;receipts[name]=receipt;hashes[str(path)]=sha(path)
        costs[name]={k:receipt[k] for k in ('returncode','wall_s','peak_observed_rss_bytes','stop_reason')}
        if row is None:continue
        costs[name].update(iterations=row['iterations'],tetrahedra=row['tetrahedra'],full_residual=row['final_residual'])
        if not row['numerically_accepted']:continue
        pc=row['preconditioner'];assert pc['kind']=='coupled_cholesky' and pc['factor_status']==0
        assert pc['library_sha256']==sha(library) and pc['symbolic_factor_storage_bytes']>0
        assert row['numerical_failure_reasons']==[] and row['linear_solve_accepted']
        assert row['condensation']['maximum_local_elimination_residual']<1e-10
        assert row['condensation']['maximum_local_schur_asymmetry']<1e-10
        assert not row['physical_accuracy_certified']
        if not row['split_first_normal'] and row['count'] in (2,4):
            oldname='cube-L4' if row['count']==2 else 'cube-L4-body4'
            oldpath=next((ROOT/'build/c3d-quartic/runs').glob('*/'+oldname+'-receipt.json'))
            oldreceipt,old=verify_receipt(oldpath)
            for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==old['identity'][key]
            comparison=force_comparison(row,old);equivalence[name]=comparison
            assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
            assert not comparison['physical_force_gate_passed']
            previous=oldpath.with_name(oldname+'.npz')
            if row['count']==4:
                previous=next((ROOT/'build/c3d-quartic/runs').glob('*/cube-L4-body4-tight-equivalence.npz'))
            if row['count']==2:previous=next((ROOT/'build/c3d-quartic/runs').glob('*/cube-L4-tight.npz'))
            with np.load(path.with_name(name+'.npz'),allow_pickle=False) as new,np.load(previous,allow_pickle=False) as before:
                differences={key:float(np.max(np.abs(new[key]-before[key]))) for key in ('velocity_coefficients','pressure_coefficients')}
                assert differences['velocity_coefficients']<1e-8 and differences['pressure_coefficients']<1e-6,differences
                np.testing.assert_array_equal(new['vertices_m'],before['vertices_m']);np.testing.assert_array_equal(new['tetrahedra'],before['tetrahedra'])
            fields[name]=differences
            costs[name].update(previous_wall_s=oldreceipt['wall_s'],previous_peak_rss_bytes=oldreceipt['peak_observed_rss_bytes'],
                memory_fraction_change=receipt['peak_observed_rss_bytes']/oldreceipt['peak_observed_rss_bytes']-1,
                speed_ratio=oldreceipt['wall_s']/receipt['wall_s'])
    assert {'original-adopted-defaults','body4-normal','body4-zero-cache','original-L4','original-memory-phases','original-chunk128','original-metis','original-prefix','body4-matched'}<=rows.keys()
    assert receipts['body4-matched']['diagnostic_failure']['kind']=='resource_cap' and rows['body4-matched'] is None
    adopted=rows['original-adopted-defaults'];assert adopted['numerically_accepted']
    assert adopted['preconditioner']['ordering']=='metis' and adopted['chunk_size']==128
    assert adopted['condensation']['reconstruction_cache_cap_bytes']==0
    assert adopted['identity']==rows['original-L4']['identity']
    for name in ('original-memory-phases','original-chunk128','original-metis','original-prefix'):
        assert rows[name]['identity']==rows['original-L4']['identity']
    for folder in ('condensed','quartic','spatial'):
        prior=json.loads((ROOT/f'build/c3d-{folder}/checkpoint-audit.json').read_text())
        for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    prior=json.loads((ROOT/'build/c3d-graded/checkpoint-audit.json').read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_solver(Path(p))
    for p,h in prior['observer_receipts_sha256'].items():assert sha(Path(p))==h;verify_observer(Path(p))
    prior=json.loads((ROOT/'build/c3d-equilibrium/checkpoint-audit.json').read_text())
    for p,h in prior['observer_receipts_sha256'].items():
        path=Path(p);assert sha(path)==h;receipt=json.loads(path.read_text())
        for a,d in receipt['artifact_sha256'].items():assert sha(Path(a))==d
        for n,d in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==d
    prior=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in prior['protected_build_hashes'].items():assert sha(ROOT/p)==h
    log=(DATA/'support-tests.log').read_text();assert 'Ran 5 tests' in log and '\nOK\n' in log and 'Traceback' not in log
    support=json.loads((DATA/'support-test-receipt.json').read_text())
    assert support['successful'] and support['tests_run']==5
    for path,h in support['source_sha256'].items():assert sha(Path(path))==h
    assert sha(Path(support['library_path']))==support['library_sha256']
    tighter_path=next((ROOT/'build/c3d-quartic/runs').glob('*/cube-L4-body4-tight-equivalence-receipt.json'))
    tighter_receipt,tighter_row=verify_receipt(tighter_path)
    assert rows['body4-zero-cache']['numerically_accepted']
    assert receipts['body4-zero-cache']['peak_observed_rss_bytes']<tighter_receipt['peak_observed_rss_bytes']
    costs['tighter-uncondensed-body4']={k:tighter_receipt[k] for k in ('wall_s','peak_observed_rss_bytes')}
    pairs={}
    for name,row in rows.items():
        if row is not None and row['numerically_accepted'] and row['split_first_normal'] and row['count']==4:
            base=rows['body4-zero-cache'];assert base['numerically_accepted']
            assert row['tetrahedra']==13824 and base['tetrahedra']==10752
            assert row['axis_nodes_m'][1:]==base['axis_nodes_m'][1:]
            assert len(row['axis_nodes_m'][0])==len(base['axis_nodes_m'][0])+2
            pairs[name]=force_comparison(row,base)
    observers={};observer_hashes={};stress={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        receipt,row=verify_current_observer(path);name=path.name.removesuffix('-receipt.json')
        assert name not in observers;observers[name]=row;observer_hashes[str(path)]=sha(path)
        stress[name]=dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],
            interior_stress_jump_l2=row['interior_stress_jump_l2'],
            maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for part in ('pressure','viscous') for v in lift[part]['identity_error_n']))
    assert {'body4-base-stress','body4-normal-stress'}<=observers.keys()
    audit=dict(schema='physics_sim_c3d_cholesky_audit_v1',status='optional_exact_sparse_factor_admits_normal_force_testing',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        bounded_force_convergence_testing_resumed=bool(pairs),
        adopted_optional_reference_defaults=dict(kind='coupled_cholesky',factor_order='metis',cache_mib=0,chunk_size=128),native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,
        reference_receipts_sha256=hashes,observer_receipts_sha256=observer_hashes,stress_defects=stress,costs=costs,original_force_equivalence=equivalence,
        matched_field_differences=fields,normal_force_comparisons=pairs,test_count=5,
        tighter_uncondensed_receipt_sha256={str(tighter_path):sha(tighter_path)},support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),
        baseline_sha256=sha(DATA/'baseline.json'),test_log_sha256=sha(DATA/'support-tests.log'),
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for f in ('scripts','tests','docs') for p in (ROOT/f).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        changed_preexisting_files=changed,committed=False,packaged=False,installed=False,
        tighter_uncondensed_full_residual=tighter_row['final_residual'],
        tighter_uncondensed_requested_target_attained=tighter_row['final_residual']['true_residual']<=tighter_row['target'],
        next_gate='L8 matched normal/domain sensitivity and measured stress-driven refinement to close the remaining raw/reaction gap under unchanged gates')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({key:audit[key] for key in ('status','costs','normal_force_comparisons','predecessor_fields_and_workers_unchanged')}),flush=True)


if __name__=='__main__':main()
