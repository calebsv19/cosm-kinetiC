#!/usr/bin/env python3
"""Audit exact nodal complement preservation, actual full target miss and cost rejection."""
import json,hashlib
from pathlib import Path
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-nodal-hierarchy'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    prior=json.loads((DATA/'predecessor.json').read_text());assert sha(Path(prior['audit']))==prior['sha256']
    for p,h in prior['stage_receipts_sha256'].items():
        path=Path(p);assert sha(path)==h
        for q,d in json.loads(path.read_text())['artifact_sha256'].items():assert sha(Path(q))==d
    unitpath=ROOT/'build/c3d-hierarchical/checkpoint-audit.json';assert sha(unitpath)==prior['unit_hierarchy_audit_sha256']
    unit=json.loads(unitpath.read_text())
    for p,h in unit['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['test_count']==6
    log=DATA/'support-tests-02.log';assert sha(log)==tests['log_sha256'] and 'Ran 6 tests' in log.read_text() and '\nOK\n' in log.read_text()
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert sha(ROOT/'build/c3d-cholesky/support/factor.dylib')==tests['library_sha256']
    assert sha(ROOT/'build/c3d-hierarchical/support-test-receipt.json')==tests['unit_support_receipt_sha256']
    paths=list((DATA/'runs').glob('*/*-receipt.json'));assert len(paths)==1
    path=paths[0];receipt,row=verify_receipt(path)
    for p,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/p)==h
    assert receipt['returncode']==0 and row['numerically_accepted'] and row['linear_solve_accepted'] and not row['physical_accuracy_certified']
    assert row['target']==1e-10 and row['final_residual']['true_residual']>row['target']
    pc=row['preconditioner'];assert pc['kind']=='nodal_hierarchy_cholesky' and pc['fixed_sweep_count']==1 and pc['scaling']=='none'
    assert pc['input_preserved_after_factor'] and pc['shared_input_preserved_after_solve']
    h=pc['hierarchy'];assert h['entity_block_triangular_pivot_verified'] and h['exact_triangular_coordinate_change_verified'] and h['nodal_left_inverse_verified'] and h['nodal_complement_vanishing_verified']
    assert h['maximum_left_inverse_error']<1e-11 and h['maximum_nodal_complement_error']<1e-11
    assert pc['factor_storage_bytes']==pc['coarse_factor']['symbolic_factor_storage_bytes']+pc['fine_factor']['symbolic_factor_storage_bytes']
    build=receipt['factor_build'];assert build['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
    assert sha(path.parent/'source/factor.dylib')==receipt['factor_library_sha256']==build['library_sha256']
    assert sha(path.parent/'source/factor-build.json')==receipt['factor_build_record_sha256'] and json.loads((path.parent/'source/factor-build.json').read_text())==build
    oldpath=next((ROOT/'build/c3d-factor-catalog/runs').glob('*/original-L4-catalog-receipt.json'));oldreceipt,old=verify_receipt(oldpath)
    for k in ('mesh_sha256','free_dofs_sha256','rhs_sha256','stored_block_triangle_sha256'):assert row['identity'][k]==old['identity'][k]
    comparison=force_comparison(row,old);assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
    differences=fields(path.with_name('original-L4-nodal-single.npz'),oldpath.with_name('original-L4-catalog.npz'))
    time_ratio=receipt['wall_s']/oldreceipt['wall_s'];memory_ratio=row['peak_rss_bytes']/old['peak_rss_bytes'];assert time_ratio>4 and memory_ratio>1.4
    sources=('scripts/audit_cfd_3d_nodal_hierarchy.py','docs/cfd_3d_nodal_hierarchy_execution_goal.md','docs/cfd_3d_nodal_hierarchy_checkpoint.md','docs/cfd_3d_mixed_precision_goal.md')
    audit=dict(schema='physics_sim_c3d_nodal_hierarchy_audit_v1',status='equivalent_original_field_target_miss_cost_rejected',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,candidate_adopted=False,larger_mesh_extension_withheld=True,requested_full_residual_target_met=False,numerically_accepted_under_existing_gates=True,
        reference_receipts_sha256={str(path):sha(path)},full_residual=row['final_residual'],final_coordinate_diagnostics=row['final_retained_residual'],last_progress=row['progress'][-1],force_equivalence=comparison,field_maximum_absolute_differences=differences,
        cost=dict(iterations=row['iterations'],wall_s=receipt['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],sampled_peak_rss_bytes=receipt['peak_observed_rss_bytes'],factor_storage_bytes=pc['factor_storage_bytes'],exact_anchor_time_ratio=time_ratio,exact_anchor_memory_ratio=memory_ratio),
        hierarchy=h,test_count=6,support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=prior['sha256'],unit_hierarchy_audit_sha256=prior['unit_hierarchy_audit_sha256'],changed_preexisting_files=changed,source_sha256={f:sha(ROOT/f) for f in sources},native_source_unchanged=True,committed=False,packaged=False,installed=False,next_gate='bounded lower-precision coupled factor with original float64 operator/full residual and flexible outer iteration')
    output=DATA/'checkpoint-audit.json';assert not output.exists();output.write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','full_residual','cost','field_maximum_absolute_differences')},indent=2))
if __name__=='__main__':main()
