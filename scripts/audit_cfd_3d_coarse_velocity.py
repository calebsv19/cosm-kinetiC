#!/usr/bin/env python3
"""Audit fixed component sweeps and retain the measured cost rejection."""
import json
from pathlib import Path
from audit_cfd_3d_graded import sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
from audit_cfd_3d_symbolic import diagnostic
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-coarse-velocity'

def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-block-cholesky/checkpoint-audit.json'
    for p,h in json.loads(predecessor.read_text())['reference_receipts_sha256'].items():
        assert sha(Path(p))==h;verify_receipt(Path(p))
    protected=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in protected['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text())
    assert tests['test_count']==5 and sha(DATA/'support-tests-final.log')==tests['log_sha256']
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert sha(ROOT/'build/c3d-cholesky/support/factor.dylib')==tests['library_sha256']
    name='original-L4-coarse-single';path=next((DATA/'runs').glob('*/'+name+'-receipt.json'))
    receipt,row=verify_receipt(path)
    assert receipt['returncode']==0 and row['linear_solve_accepted'] and row['numerically_accepted']
    for p,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/p)==h
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    pc=row['preconditioner'];assert pc['kind']=='balanced_coarse_velocity' and pc['fixed_sweep_count']==1
    assert pc['input_preserved_after_factor'] and pc['shared_input_preserved_after_solve'] and pc['scaling']=='none'
    assert pc['interpolation']['essential_projection_injective'] and pc['interpolation']['vertex_injection_identity_verified']
    assert pc['coarse_dofs']==444 and row['tetrahedra']==4992
    oldname='original-adopted-defaults';oldpath=next((ROOT/'build/c3d-cholesky/runs').glob('*/'+oldname+'-receipt.json'))
    oldreceipt,old=verify_receipt(oldpath)
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==old['identity'][key]
    comparison=force_comparison(row,old)
    assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
    delta=fields(path.with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
    cost=dict(owned_memory_reduction=1-row['peak_rss_bytes']/old['peak_rss_bytes'],total_time_ratio=receipt['wall_s']/oldreceipt['wall_s'],
        owned_peak_rss_bytes=row['peak_rss_bytes'],wall_s=receipt['wall_s'],iterations=row['iterations'],factor_storage_bytes=pc['factor_storage_bytes'],final_residual=row['final_residual'])
    sweep_path=next((ROOT/'build/c3d-block-cholesky/runs').glob('*/original-L4-u-vw-single-receipt.json'))
    sweep_receipt,sweep=verify_receipt(sweep_path)
    cost.update(time_ratio_to_block_sweep=receipt['wall_s']/sweep_receipt['wall_s'],iteration_reduction_to_block_sweep=1-row['iterations']/sweep['iterations'])
    assert cost['time_ratio_to_block_sweep']>1.2 and cost['iteration_reduction_to_block_sweep']<.1
    audit=dict(schema='physics_sim_c3d_coarse_velocity_audit_v1',status='nested_macro_linear_coarse_control_accepted_larger_path_rejected_for_cost',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,larger_path_adopted=False,native_source_unchanged=True,
        reference_receipts_sha256={str(path):sha(path)},equivalence=comparison,field_differences=delta,matched_cost=cost,
        support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),test_count=5,baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),
        changed_preexisting_files=changed,next_gate='nested macro-quadratic velocity Galerkin correction with unchanged balanced SPD formula and full FE acceptance')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
