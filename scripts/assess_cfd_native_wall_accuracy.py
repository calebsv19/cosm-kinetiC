"""Verify an immutable wall-case receipt and report physical accuracy separately.

Read-only source-checkout assessment. Exit 0 means this known-answer case meets
its field/force targets; exit 2 means it does not. Neither qualifies general CFD.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def assess(path):
    receipt=json.loads(path.read_text());directory=path.parent
    if not receipt['status'].startswith('completed'):raise ValueError('Receipt is not terminal successful investigation')
    for q,h in receipt['artifact_sha256'].items():
        p=(directory/q).resolve()
        if not p.is_relative_to(directory.resolve()) or sha(p)!=h:raise ValueError('Artifact identity failed: '+q)
    contract=json.loads((directory/'contract.json').read_text())
    source=contract['source_sha256']
    for q,h in source.items():
        if sha(directory/'source'/q)!=h:raise ValueError('Frozen source identity failed: '+q)
    row=receipt.get('control') or receipt['controls'][str(max(map(int,receipt['controls'])))]
    exact=np.asarray(receipt['independent_forcing']['analytic_viscous_force_n']);scale=float(np.linalg.norm(exact))
    if not np.all(np.isfinite(exact)) or scale<=0:raise ValueError('Invalid exact force')
    np.testing.assert_allclose(row['analytic_viscous_force_n'],exact,rtol=0,atol=1e-14)
    process=receipt['processes']['n'+str(row['n'])]
    owned_cap=contract['owned_cap_bytes'];rss_cap=contract.get('rss_cap_bytes',contract.get('supervisor_rss_cap_bytes'))
    wall_cap=contract['fixture_wall_cap_s']
    velocity_target=contract.get('velocity_error_target',contract.get('fine_velocity_error_target'))
    pressure_target=contract.get('pressure_error_target',contract.get('fine_pressure_error_target'))
    values=[row[q] for q in ('velocity_relative_error','pressure_relative_error','momentum_residual','maximum_divergence')]
    finite=all(math.isfinite(v) for v in values)
    numerical=finite and row['momentum_residual']<1e-11 and row['maximum_divergence']<1e-8
    resources=row['peak_owned_bytes']<owned_cap and process['peak_sampled_child_rss_bytes']<=rss_cap and row['wall_s']<wall_cap
    error=float(np.linalg.norm(np.asarray(row['solved_pressure_force_n'])+np.asarray(row['solved_viscous_force_n'])-exact)/scale)
    field=finite and row['velocity_relative_error']<=velocity_target and row['pressure_relative_error']<=pressure_target
    passed=numerical and resources and field and error<=.01
    result=dict(status='known_answer_wall_accuracy_passed' if passed else 'known_answer_wall_accuracy_targets_not_met',
        receipt=str(path.resolve()),receipt_sha256=sha(path),assessor_sha256=sha(Path(__file__)),n=row['n'],
        numerical_gates_passed=numerical,resource_gates_passed=resources,field_targets_passed=field,
        velocity_relative_error=row['velocity_relative_error'],pressure_relative_error=row['pressure_relative_error'],
        current_total_force_relative_error=error,known_answer_total_force_error_target=.01,
        velocity_error_target=velocity_target,pressure_error_target=pressure_target,
        current_native_source_matches_packet=all((ROOT/q).exists() and sha(ROOT/q)==h for q,h in source.items() if q.startswith(('src/','include/'))),
        original_pressure_driven_cube_qualification=False,native_default_adopted=False,persistent_goal_complete=False,
        scope='independently forced nonzero cube-wall shear case; zero exact pressure load and known viscous load; no gauge or force correction')
    if 'solved_candidate_pressure_force_n' in row:
        candidate=float(np.linalg.norm(np.asarray(row['solved_candidate_pressure_force_n'])+np.asarray(row['solved_viscous_force_n'])-exact)/scale)
        result['candidate_pressure_total_force_relative_error']=candidate
        result['candidate_is_diagnostic_only']=True
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('receipt',type=Path);args=p.parse_args()
    try:result=assess(args.receipt)
    except Exception as error:result=dict(status='receipt_verification_failed',failure=str(error),original_pressure_driven_cube_qualification=False)
    print(json.dumps(result,indent=2,allow_nan=False))
    return 0 if result['status']=='known_answer_wall_accuracy_passed' else 2
if __name__=='__main__':raise SystemExit(main())
