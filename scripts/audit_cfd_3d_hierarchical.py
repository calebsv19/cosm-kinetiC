#!/usr/bin/env python3
"""Audit the rejected exact coordinate split without promoting a field."""
import json
from pathlib import Path
from audit_cfd_3d_graded import sha
from audit_cfd_3d_spatial import verify_receipt
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-hierarchical'

def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-additive-coarse/checkpoint-audit.json'
    for p,h in json.loads(predecessor.read_text())['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text());assert tests['test_count']==6
    assert sha(DATA/'support-tests.log')==tests['log_sha256']
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert sha(ROOT/'build/c3d-cholesky/support/factor.dylib')==tests['library_sha256']
    path=next((DATA/'runs').glob('*/original-L4-hierarchical-single-receipt.json'));receipt,row=verify_receipt(path)
    for p,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/p)==h
    assert receipt['returncode']==2 and receipt['diagnostic_failure']['kind']=='numerical_acceptance_gate'
    assert row['iterations']==3000 and not row['linear_solve_accepted'] and not row['numerically_accepted']
    assert row['final_residual']['true_residual']>1e-4 and not path.with_name('original-L4-hierarchical-single.npz').exists()
    pc=row['preconditioner'];assert pc['input_preserved_after_factor'] and pc['shared_input_preserved_after_solve']
    assert pc['hierarchy']['entity_block_triangular_pivot_verified'] and pc['scaling']=='none'
    assert pc['factor_storage_bytes']==pc['coarse_factor']['symbolic_factor_storage_bytes']+pc['fine_factor']['symbolic_factor_storage_bytes']
    old=next((ROOT/'build/c3d-cholesky/runs').glob('*/original-adopted-defaults-receipt.json'));_,oldrow=verify_receipt(old)
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==oldrow['identity'][key]
    audit=dict(schema='physics_sim_c3d_hierarchical_audit_v1',status='unit_complement_coordinate_control_rejected',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        candidate_adopted=False,native_source_unchanged=True,reference_receipts_sha256={str(path):sha(path)},full_residual=row['final_residual'],last_coordinate_diagnostics=row['progress'][-1],
        cost=dict(wall_s=receipt['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],factor_storage_bytes=pc['factor_storage_bytes']),test_count=6,
        support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),changed_preexisting_files=changed,
        next_gate='quartic complement that vanishes at cubic nodes, exact congruence and coupled factors; unchanged physical/full FE authority')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
