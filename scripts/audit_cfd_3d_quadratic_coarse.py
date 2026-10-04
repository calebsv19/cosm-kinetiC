#!/usr/bin/env python3
"""Audit exact coupled two-block controls and full larger-run cost rejection."""
import json
from pathlib import Path
from audit_cfd_3d_graded import sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
from audit_cfd_3d_symbolic import diagnostic
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-quadratic-coarse'

def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-coarse-velocity/checkpoint-audit.json'
    for p,h in json.loads(predecessor.read_text())['reference_receipts_sha256'].items():
        assert sha(Path(p))==h;verify_receipt(Path(p))
    protected=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in protected['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text())
    assert tests['test_count']==5 and sha(DATA/'support-tests-final.log')==tests['log_sha256']
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert sha(ROOT/'build/c3d-cholesky/support/factor.dylib')==tests['library_sha256']
    rows={};receipts={};costs={};hashes={};equivalence={};differences={}
    oldname='original-adopted-defaults';oldpath=next((ROOT/'build/c3d-cholesky/runs').glob('*/'+oldname+'-receipt.json'))
    oldreceipt,old=verify_receipt(oldpath)
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        name=path.name.removesuffix('-receipt.json');receipt,row=verify_receipt(path)
        rows[name]=row;receipts[name]=receipt;hashes[str(path)]=sha(path)
        for p,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/p)==h
        assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','diagnostic_failure')}
        if receipt['returncode']!=0:continue
        assert row['linear_solve_accepted'] and row['numerically_accepted'] and not row['physical_accuracy_certified']
        pc=row['preconditioner'];assert pc['kind']=='balanced_coarse_velocity' and pc['interpolation']['coarse_degree']==2
        assert pc['interpolation']['essential_projection_injective'] and pc['interpolation']['essential_boundary_vanishing_verified']
        assert pc['input_preserved_after_factor'] and pc['shared_input_preserved_after_solve'] and pc['scaling']=='none'
        assert pc['factor_storage_bytes']==pc['coarse_factor']['symbolic_factor_storage_bytes']+pc['local_sweep']['factor_storage_bytes']
        costs[name].update(owned_peak_rss_bytes=row['peak_rss_bytes'],iterations=row['iterations'],tetrahedra=row['tetrahedra'],factor_storage_bytes=pc['factor_storage_bytes'],final_residual=row['final_residual'],phase_timings=row['timings'])
        if name.startswith('original-'):
            for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==old['identity'][key]
            comp=force_comparison(row,old)
            assert max([*comp['component_relative_changes'].values(),*comp['scalar_relative_changes'].values()])<1e-7
            equivalence[name]=comp;differences[name]=fields(path.with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
    assert set(rows)=={'original-L4-quadratic-single','L4-body6-base-quadratic-single'}
    assert set(equivalence)=={'original-L4-quadratic-single'}
    base=receipts['L4-body6-base-quadratic-single'];assert base['returncode']!=0
    previous=next((ROOT/'build/c3d-shared-factor/runs').glob('*/L4-body6-base-shared-receipt.json'));_,previous_row=verify_receipt(previous)
    identity=rows['L4-body6-base-quadratic-single']['identity'] if rows['L4-body6-base-quadratic-single'] else next(p['identity'] for p in base['progress'] if p.get('phase')=='assembled')
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert identity[key]==previous_row['identity'][key]
    audit=dict(schema='physics_sim_c3d_quadratic_coarse_audit_v1',status='quadratic_coarse_original_accepted_refined_time_cap',persistent_goal_complete=False,stage_1_complete=False,
        physical_accuracy_certified=False,larger_path_adopted=False,native_source_unchanged=True,reference_receipts_sha256=hashes,costs=costs,equivalence=equivalence,field_differences=differences,
        support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),test_count=5,baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),changed_preexisting_files=changed,
        next_gate='nested macro-cubic Galerkin coarse inverse with fixed component or two-block sweeps and full FE authority')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
