#!/usr/bin/env python3
"""Audit domain/near-plane separation, outer rejection and early resource policy."""
import hashlib
import json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_graded import sha,verify_solver,verify_observer
from audit_cfd_3d_cholesky import verify_current_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-domain'


def observe(path):
    receipt=json.loads(path.read_text());assert receipt['returncode']==0 and receipt['diagnostic_failure'] is None and receipt['stop_reason'] is None
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
    scores=np.array(row['equilibrium_indicator_squared_per_tet']);assert len(scores)==row['tetrahedra'] and np.all(np.isfinite(scores)) and np.all(scores>=0)
    assert abs(scores.sum()-sum(row['h_squared_volume_defect_centroid_buckets'])-sum(row['h_weighted_jump_defect_centroid_buckets']))<1e-12
    for lift,old in zip(row['lifts'],original['consistency_diagnostics']['volume_lifts']):
        assert abs(sum(lift[p]['weak_load_n'][0] for p in ('pressure','viscous'))-old['symmetric_stress_load_n'])<1e-9
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            assert np.max(np.abs(np.array(lift[part]['raw_surface_load_n'])-original[key]))<1e-10
            assert np.max(np.abs(lift[part]['identity_error_n']))<1e-9
    return receipt,row


def domain_comparison(a,b):
    comparison=force_comparison(a,b)
    comparison['domain_force_gate_passed']=max([*comparison['component_relative_changes'].values(),*comparison['raw_surface_reaction_relative_mismatch'].values()])<=.01
    comparison['criterion_scope']='body forces and raw/reaction agreement; total inlet pressure and dissipation reported as length-dependent physical responses'
    return comparison


def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    rows={};receipts={};hashes={};costs={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');assert name not in rows
        library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
        assert library.parent==path.parent/'source'
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
        rows[name]=row;receipts[name]=receipt;hashes[str(path)]=sha(path)
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','stop_reason','diagnostic_failure')}
        if row is not None:
            costs[name].update(iterations=row['iterations'],full_residual=row['final_residual'])
            if row['numerically_accepted']:
                assert row['length']==8 and row['count']==4 and row['body']
                assert (row['velocity_degree'],row['pressure_degree'])==(4,3)
                assert row['numerical_failure_reasons']==[] and row['linear_solve_accepted']
                assert row['tetrahedra']==(13824 if row['split_first_normal'] else 10752)
                for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n'):assert np.max(np.abs(row[key][1:]))<1e-8
                assert row['condensation']['maximum_local_elimination_residual']<1e-10
                assert row['condensation']['maximum_local_schur_asymmetry']<1e-10
    required={'L8-body4-base','L8-body4-normal','L8-held-base','L8-held-normal','L8-held-outer2','L8-outer2-early-budget','L8-held-normal-budget-control'}
    assert required<=rows.keys()
    old_normal=rows['L8-held-normal'];new_normal=rows['L8-held-normal-budget-control']
    assert old_normal['identity']==new_normal['identity']
    budget_equivalence=force_comparison(new_normal,old_normal)
    assert max([*budget_equivalence['component_relative_changes'].values(),*budget_equivalence['scalar_relative_changes'].values()])<1e-7
    with np.load(next((DATA/'runs').glob('*/L8-held-normal.npz'))) as old,np.load(next((DATA/'runs').glob('*/L8-held-normal-budget-control.npz'))) as new:
        field_differences={k:float(np.max(np.abs(new[k]-old[k]))) for k in ('velocity_coefficients','pressure_coefficients')}
    assert field_differences['velocity_coefficients']<1e-8 and field_differences['pressure_coefficients']<1e-6
    failed=rows['L8-held-outer2'];assert failed['linear_solve_accepted'] and not failed['numerically_accepted']
    assert failed['numerical_failure_reasons']==['resources'] and failed['peak_rss_bytes']>=1800*1024**2
    assert receipts['L8-held-outer2']['peak_observed_rss_bytes']<1800*1024**2
    assert receipts['L8-outer2-early-budget']['diagnostic_failure']['kind']=='resource_cap'
    assert receipts['L8-outer2-early-budget']['wall_s']<20
    l4base_path=next((ROOT/'build/c3d-cholesky/runs').glob('*/body4-zero-cache-receipt.json'));l4base=verify_receipt(l4base_path)[1]
    l4normal_path=next((ROOT/'build/c3d-cholesky/runs').glob('*/body4-normal-receipt.json'));l4normal=verify_receipt(l4normal_path)[1]
    for name,l4 in (('L8-held-base',l4base),('L8-held-normal',l4normal)):
        row=rows[name]
        np.testing.assert_allclose(np.array(row['axis_nodes_m'][0])[1:-1]-4,np.array(l4['axis_nodes_m'][0])[1:-1]-2,rtol=0,atol=1e-14)
        assert row['axis_nodes_m'][1:]==l4['axis_nodes_m'][1:]
    for a,b in (('L8-body4-normal','L8-body4-base'),('L8-held-normal','L8-held-base')):
        assert rows[a]['axis_nodes_m'][1:]==rows[b]['axis_nodes_m'][1:]
        assert len(rows[a]['axis_nodes_m'][0])==len(rows[b]['axis_nodes_m'][0])+2
    mesh_pairs={'L8_original_normal':force_comparison(rows['L8-body4-normal'],rows['L8-body4-base']),
        'L8_held_normal':force_comparison(rows['L8-held-normal'],rows['L8-held-base'])}
    domain_pairs={'base_original':domain_comparison(rows['L8-body4-base'],l4base),
        'normal_original':domain_comparison(rows['L8-body4-normal'],l4normal),
        'base_held':domain_comparison(rows['L8-held-base'],l4base),'normal_held':domain_comparison(rows['L8-held-normal'],l4normal)}
    assert not any(p['physical_force_gate_passed'] for p in mesh_pairs.values())
    observers={};observer_hashes={};stress={}
    for path in sorted((DATA/'observer-runs').glob('*/*-receipt.json')):
        receipt,row=observe(path);name=path.name.removesuffix('-receipt.json');observers[name]=row;observer_hashes[str(path)]=sha(path)
        stress[name]=dict(volume_equilibrium_defect_l2=row['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=row['interior_stress_jump_l2'],
            maximum_identity_error_n=max(abs(v) for lift in row['lifts'] for p in ('pressure','viscous') for v in lift[p]['identity_error_n']),
            volume_indicator_centroid_buckets=row['h_squared_volume_defect_centroid_buckets'],jump_indicator_centroid_buckets=row['h_weighted_jump_defect_centroid_buckets'])
    assert {'L8-original-normal-stress','L8-held-normal-stress'}<=observers.keys()
    prior=json.loads((ROOT/'build/c3d-cholesky/checkpoint-audit.json').read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in prior['observer_receipts_sha256'].items():assert sha(Path(p))==h;verify_current_observer(Path(p))
    for p,h in prior['tighter_uncondensed_receipt_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for folder in ('condensed','quartic','spatial'):
        prior=json.loads((ROOT/f'build/c3d-{folder}/checkpoint-audit.json').read_text())
        for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    prior=json.loads((ROOT/'build/c3d-graded/checkpoint-audit.json').read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_solver(Path(p))
    for p,h in prior['observer_receipts_sha256'].items():assert sha(Path(p))==h;verify_observer(Path(p))
    prior=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in prior['protected_build_hashes'].items():assert sha(ROOT/p)==h
    for filename,count in (('mesh-tests.log',4),('budget-tests.log',2)):
        log=(DATA/filename).read_text();assert f'Ran {count} tests' in log and '\nOK\n' in log and 'FAILED (' not in log
    audit=dict(schema='physics_sim_c3d_domain_audit_v1',status='L8_domain_and_near_mesh_separated_force_gates_open',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,
        reference_receipts_sha256=hashes,observer_receipts_sha256=observer_hashes,costs=costs,mesh_force_comparisons=mesh_pairs,domain_force_comparisons=domain_pairs,stress_defects=stress,
        early_budget_accepted_field_equivalence=budget_equivalence,early_budget_field_differences=field_differences,
        sampler_missed_peak_retained=True,owned_phase_resource_guard_unit_verified=True,early_runtime_stop_authority='supervisor observed RSS cap',
        test_count=6,test_log_sha256={n:sha(DATA/n) for n in ('mesh-tests.log','budget-tests.log','mesh-tests-failed-translation.log')},baseline_sha256=sha(DATA/'baseline.json'),
        predecessor_audit_sha256=sha(ROOT/'build/c3d-cholesky/checkpoint-audit.json'),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for f in ('scripts','tests','docs') for p in (ROOT/f).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='localize dominant outer stress defect and prove a selective conforming refinement within original caps before native qualification')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','mesh_force_comparisons','domain_force_comparisons','stress_defects')}),flush=True)


if __name__=='__main__':main()
