"""Assess a completed edge040 field against strict physical gates and parent."""
import argparse,json,hashlib
from pathlib import Path
from cfd_reference3d_graded_edge_evidence import verify,physical_comparison
from cfd_reference3d_accuracy_graded_evidence import verify as parent_verify
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-graded-edge-field'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def assess(receipt):
    r,row=verify(receipt);L=int(row['length']);out=D/f'L{L}-physical-assessment.json';assert not out.exists()
    parent_a=json.loads((R/'build/c3d-accuracy-graded/physical-assessment.json').read_text())['fields'][f'L{L}'];parent=Path(parent_a['receipt']);assert sha(parent)==parent_a['receipt_sha256'];_,old=parent_verify(parent)
    comparison=physical_comparison(old,row,'same_domain_refinement');previous=physical_comparison(old,old,'same_domain_refinement');ratio=comparison['raw_surface_reaction_relative_mismatch']/previous['raw_surface_reaction_relative_mismatch']
    contract=json.loads((D/'selection-contract.json').read_text());admit=ratio<=contract['max_new_old_raw_mismatch_ratio_for_L8'];result=dict(schema='physics_sim_c3d_graded_edge_physical_assessment_v1',length=L,receipt=str(receipt.resolve()),receipt_sha256=sha(receipt),parent_receipt=str(parent),parent_receipt_sha256=sha(parent),full_residual=row['final_residual'],flux_error=row['flux_error'],maximum_divergence=row['volume_divergence_max_s_inv'],physical_energy_imbalance=row['physical_energy_imbalance'],same_domain_refinement=comparison,previous_raw_surface_reaction_mismatch=previous['raw_surface_reaction_relative_mismatch'],new_old_raw_mismatch_ratio=ratio,selection_contract_sha256=sha(D/'selection-contract.json'),L8_trial_permitted=(L==4 and admit),physical_accuracy_certified=False,persistent_goal_complete=False,native_default_adopted=False,wall_s=r['wall_s'],owned_mib=row['peak_rss_bytes']/2**20,sampled_mib=r['peak_observed_rss_bytes']/2**20)
    if L==8:
        a=json.loads((D/'L4-physical-assessment.json').read_text());assert a['L8_trial_permitted'];_,short=verify(Path(a['receipt']));result['domain_sensitivity']=physical_comparison(short,row,'domain_sensitivity')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('receipt',type=Path);a=p.parse_args();assess(a.receipt)
