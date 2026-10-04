"""Compare a fine directional cube field with the immutable retained parent."""
import argparse
import json
from pathlib import Path
from cfd_reference3d_directional_fine_evidence import verify, physical_comparison
from cfd_reference3d_directional_evidence import verify as parent_verify
from run_cfd_native_accuracy_regression import save, sha, require

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT/'build/c3d-directional-fine-field'


def assess(receipt):
    r, row = verify(receipt)
    length = int(row['length']); out = DEST/f'L{length}-physical-assessment.json'
    contract_path = DEST/'selection-contract.json'
    contract = json.loads(contract_path.read_text())
    parent_record = contract['parent_fields']['L'+str(length)]
    parent = Path(parent_record['receipt'])
    require(sha(parent)==parent_record['receipt_sha256'], 'Retained parent identity')
    _, previous = parent_verify(parent)
    comparison = physical_comparison(previous, row, 'same_domain_refinement')
    original = physical_comparison(previous, previous, 'same_domain_refinement')
    ratio = comparison['raw_surface_reaction_relative_mismatch']/original['raw_surface_reaction_relative_mismatch']
    admit = ratio <= contract['max_new_old_raw_mismatch_ratio_for_L8'] and comparison['force_change_passed'] and comparison['scalar_refinement_passed']
    result = dict(schema='physics_sim_c3d_directional_fine_physical_assessment_v1',
        length=length, receipt=str(receipt.resolve()), receipt_sha256=sha(receipt),
        parent_receipt=str(parent),parent_receipt_sha256=sha(parent),
        full_residual=row['final_residual'],flux_error=row['flux_error'],
        maximum_divergence=row['volume_divergence_max_s_inv'],physical_energy_imbalance=row['physical_energy_imbalance'],
        same_domain_refinement=comparison,previous_raw_surface_reaction_mismatch=original['raw_surface_reaction_relative_mismatch'],
        new_old_raw_mismatch_ratio=ratio,selection_contract_sha256=sha(contract_path),
        L8_trial_permitted=length==4 and admit,physical_accuracy_certified=False,
        persistent_goal_complete=False,native_default_adopted=False,wall_s=r['wall_s'],
        owned_mib=row['peak_rss_bytes']/2**20,sampled_mib=r['peak_observed_rss_bytes']/2**20)
    if length==8:
        short=json.loads((DEST/'L4-physical-assessment.json').read_text())
        require(short['L8_trial_permitted'], 'Prospective L8 admission')
        _, field=verify(Path(short['receipt']))
        result['domain_sensitivity']=physical_comparison(field,row,'domain_sensitivity')
    save(out,result)
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('receipt',type=Path)
    assess(parser.parse_args().receipt)
