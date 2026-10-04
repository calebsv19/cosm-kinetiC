#!/usr/bin/env python3
"""Verify exact condensed equations, retained failures and measured admission."""
import json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_graded import sha,verify_solver,verify_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-condensed'


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,digest in baseline.items() if sha(ROOT/p)!=digest]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    rows={};receipts={};hashes={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json')
        assert name not in receipts
        receipts[name]=receipt;rows[name]=row;hashes[str(path)]=sha(path)
    required={'original-component-factor','original-component-factor-fixed','original-coupled-factor',
        'original-symmetric-ilu','original-low-fill','original-medium-fill','original-storage-readback'}
    required.update(('original-gmres-fill5','original-gmres-scaled-fill5','original-component-ssor','original-coupled-amg','original-component-ssor4'))
    assert required<=rows.keys()
    assert rows['original-component-factor'] is None
    assert 'not JSON serializable' in next((DATA/'runs').glob('*/original-component-factor.log')).read_text()
    failed=rows['original-component-factor-fixed']
    assert not failed['numerically_accepted'] and failed['iterations']==3000
    assert failed['final_residual']['momentum_relative_to_rhs']>1e-5
    for name in ('original-low-fill','original-medium-fill'):
        assert rows[name] is None
        assert 'requires positive finite diagonal' in next((DATA/'runs').glob('*/'+name+'.log')).read_text()
    oldpath=next((ROOT/'build/c3d-quartic/runs').glob('*/cube-L4-receipt.json'))
    oldreceipt,old=verify_receipt(oldpath)
    tightpath=next((ROOT/'build/c3d-quartic/runs').glob('*/cube-L4-tight-receipt.json'))
    verify_receipt(tightpath)
    solved_line=next(line for line in (DATA/'solved-tests.log').read_text().splitlines() if line.startswith('{'))
    manufactured=json.loads(solved_line);assert len(manufactured['cases'])==24
    manufactured_maxima={key:max(case[key] for case in manufactured['cases']) for key in
        ('velocity_error','pressure_error','pressure_traction_error','viscous_traction_error','true_residual','volume_divergence_max')}
    for key,value in manufactured_maxima.items():assert value<1e-9,(key,value)
    rejected={}
    for name,receipt in receipts.items():
        row=rows[name]
        if receipt['returncode']==0:continue
        rejected[name]=dict(returncode=receipt['returncode'],failure=receipt['diagnostic_failure'],
            wall_s=receipt['wall_s'],peak_observed_rss_bytes=receipt['peak_observed_rss_bytes'],
            iterations=row.get('iterations') if row else None,full_residual=row.get('final_residual') if row else None)
    successful={};equivalence={};field_differences={}
    for name,row in rows.items():
        if row is None or not row['numerically_accepted']:continue
        assert row['numerical_failure_reasons']==[] and row['linear_solve_accepted']
        assert row['tetrahedra']==4992 or name.startswith('finer-')
        assert (row['velocity_degree'],row['pressure_degree'])==(4,3)
        local=row['condensation'];assert local['maximum_local_elimination_residual']<1e-10
        assert local['maximum_local_schur_asymmetry']<1e-10
        assert local['maximum_alfeld_center_error_m']<1e-12
        assert local['reconstruction_cache_bytes']<=local['reconstruction_cache_cap_bytes']
        if row['tetrahedra']==4992:
            for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==old['identity'][key]
            comparison=force_comparison(row,old)
            assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
            assert not comparison['physical_force_gate_passed']
            equivalence[name]=comparison
            path=Path(receipts[name]['command'][-3])
            with np.load(path,allow_pickle=False) as new,np.load(tightpath.with_name('cube-L4-tight.npz'),allow_pickle=False) as previous:
                differences={key:float(np.max(np.abs(new[key]-previous[key]))) for key in ('velocity_coefficients','pressure_coefficients')}
                assert differences['velocity_coefficients']<1e-8 and differences['pressure_coefficients']<1e-6,differences
                np.testing.assert_array_equal(new['vertices_m'],previous['vertices_m'])
                np.testing.assert_array_equal(new['tetrahedra'],previous['tetrahedra'])
            field_differences[name]=differences
        successful[name]={k:receipts[name][k] for k in ('wall_s','peak_observed_rss_bytes')}
        successful[name].update(iterations=row['iterations'],full_residual=row['final_residual'],tetrahedra=row['tetrahedra'])
    assert {'original-coupled-factor','original-symmetric-ilu','original-storage-readback'}<=successful.keys()
    assert rows['original-storage-readback']['identity']==rows['original-coupled-factor']['identity']
    for filename,count in (('algebra-tests.log',3),('solved-tests.log',1),('preconditioner-tests.log',5)):
        log=(DATA/filename).read_text();assert f'Ran {count} test' in log and '\nOK\n' in log
        assert 'Traceback' not in log and 'FAILED (' not in log
    predecessor=json.loads((ROOT/'build/c3d-graded/checkpoint-audit.json').read_text())
    for p,digest in predecessor['reference_receipts_sha256'].items():assert sha(Path(p))==digest;verify_solver(Path(p))
    for p,digest in predecessor['observer_receipts_sha256'].items():assert sha(Path(p))==digest;verify_observer(Path(p))
    for folder in ('quartic','spatial'):
        prior=json.loads((ROOT/f'build/c3d-{folder}/checkpoint-audit.json').read_text())
        for p,digest in prior['reference_receipts_sha256'].items():assert sha(Path(p))==digest;verify_receipt(Path(p))
    prior=json.loads((ROOT/'build/c3d-equilibrium/checkpoint-audit.json').read_text())
    for p,digest in prior['observer_receipts_sha256'].items():
        path=Path(p);assert sha(path)==digest;receipt=json.loads(path.read_text())
        for artifact,value in receipt['artifact_sha256'].items():assert sha(Path(artifact))==value
        for name,value in receipt['source_sha256'].items():assert sha(path.parent/'source'/name)==value
    prior=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,digest in prior['protected_build_hashes'].items():assert sha(ROOT/p)==digest
    current=successful['original-storage-readback']
    measured=dict(old_wall_s=oldreceipt['wall_s'],old_peak_rss_bytes=oldreceipt['peak_observed_rss_bytes'],
        coupled_speed_ratio=oldreceipt['wall_s']/current['wall_s'],
        coupled_memory_fraction_change=current['peak_observed_rss_bytes']/oldreceipt['peak_observed_rss_bytes']-1)
    assert measured['coupled_memory_fraction_change']>0
    sources={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*')
        if p.is_file() and str(p.relative_to(ROOT)) not in baseline}
    audit=dict(schema='physics_sim_c3d_condensed_audit_v1',status='exact_condensation_verified_coupled_solver_investigation',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        exact_original_equations_and_full_field_verified=True,native_source_unchanged=True,
        predecessor_fields_and_workers_unchanged=True,reference_receipts_sha256=hashes,
        costs=successful,original_force_equivalence=equivalence,original_tight_field_differences=field_differences,
        measured_coupled_control=measured,current_source_sha256=sources,changed_preexisting_files=changed,
        test_count=9,manufactured_solved_case_count=24,manufactured_maxima=manufactured_maxima,rejected_controls=rejected,
        test_log_sha256={name:sha(DATA/name) for name in ('algebra-tests.log','solved-tests.log','preconditioner-tests.log')},
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(ROOT/'build/c3d-graded/checkpoint-audit.json'),
        committed=False,packaged=False,installed=False,
        original_cube_physical_force_gate_passed=False,adopted_memory_improvement=False,
        next_gate='lower-memory coupled inverse must pass original full residual and resource controls before finer force qualification')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({key:audit[key] for key in ('status','costs','measured_coupled_control','predecessor_fields_and_workers_unchanged')}),flush=True)


if __name__=='__main__':main()
