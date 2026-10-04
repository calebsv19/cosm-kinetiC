"""Seal the local cube rejection, native regression and analytic traction proof."""
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess

from cfd_reference3d_graded_local_evidence import verify

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT/'build/c3d-local-continuation'


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()


def load(path):
    return json.loads(path.read_text())


def main():
    output = DEST/'closeout-verification.json'
    assert not output.exists()
    preceding = load(DEST/'predecessor.json')
    assert sha(ROOT/'build/c3d-graded-stress/closeout-verification.json') == preceding['closeout_sha256']
    for name, expected in preceding['checkpoint_sha256'].items():
        assert sha(ROOT/'build'/name/'checkpoint-audit.json') == expected
    for name, expected in preceding['native_hashes'].items():
        assert sha(ROOT/name) == expected
    prepared = load(ROOT/'build/c3d-graded-local-field/prepared-source.json')
    for name, expected in prepared['source_sha256'].items():
        assert sha(ROOT/name) == expected == sha(ROOT/'build/c3d-graded-local-field/prepared-frozen'/name)
    transforms = ROOT/'build/c3d-graded-local-field/transforms.json'
    assert sha(transforms) == prepared['transforms_sha256']
    for transform in load(transforms):
        parent, current = ROOT/transform['parent'], ROOT/transform['output']
        assert sha(parent) == transform['parent_sha256']
        assert sha(current) == transform['output_sha256']
        text = parent.read_text()
        for before, after in transform['literal_replacements']:
            assert before in text
            text = text.replace(before, after)
        assert text == current.read_text()
    assessment_path = ROOT/'build/c3d-graded-local-field/L4-physical-assessment.json'
    assessment = load(assessment_path)
    receipt_path = Path(assessment['receipt'])
    receipt, field = verify(receipt_path)
    assert sha(receipt_path) == assessment['receipt_sha256']
    assert field['final_residual']['true_residual'] < 1e-10
    assert field['flux_error'] < 1e-8 and field['volume_divergence_max_s_inv'] < 1e-8
    assert field['physical_energy_imbalance'] < .03
    assert not assessment['L8_trial_permitted'] and assessment['new_old_raw_mismatch_ratio'] > .9
    assert not assessment['physical_accuracy_certified']
    local_root = ROOT/'build/c3d-graded-local-field'
    assert not list(local_root.rglob('L8*-receipt.json'))
    assert sha(local_root/'selection-contract.json') == prepared['selection_contract_sha256'] == assessment['selection_contract_sha256']
    regression_path = ROOT/'build/c3d-native-accuracy-regression/runs/624d131df03daf549819804cb5cf73e2d12ccda90b05dcfdfa5e469b2b838dc2/accuracy-first-02/receipt.json'
    regression = load(regression_path)
    contract = load(regression_path.parent/'contract.json')
    assert regression['status'] == 'passed_smooth_native_accuracy_regression'
    assert regression['independent_integral_samples'] == 84 and regression['maximum_independent_integral_error'] < 2e-8
    for name, expected in contract['source_sha256'].items():
        assert sha(ROOT/name) == expected == sha(regression_path.parent/'source'/name)
    for name, expected in regression['artifact_sha256'].items():
        assert sha(regression_path.parent/name) == expected
    assert set(regression['controls']) == {'8', '16', '32', '64'}
    for n, row in regression['controls'].items():
        assert row['momentum_residual'] < 1e-11 and row['maximum_divergence'] < 1e-8
    assert regression['controls']['64']['velocity_relative_error'] < .01
    assert regression['controls']['64']['pressure_relative_error'] < .003
    traction_path = ROOT/'build/c3d-cube-traction-polynomial/checkpoint-audit.json'
    calibration = load(traction_path)
    assert calibration['status'] == 'passed_actual_mesh_polynomial_traction_calibration'
    assert len(calibration['controls']) == 8 and calibration['maximum_face_load_error_n'] < 1e-9
    for name, expected in calibration['source_sha256'].items():
        assert sha(ROOT/name) == expected
        if name.startswith('scripts/'):
            assert sha(traction_path.parent/'source'/name) == expected
    baseline = load(DEST/'baseline.json')
    changes = [name for name, expected in baseline.items() if not (ROOT/name).exists() or sha(ROOT/name) != expected]
    assert set(changes) <= {'docs/current_truth.md', 'docs/README.md'}, changes
    documents = ['docs/current_truth.md', 'docs/README.md', 'docs/cfd_3d_local_accuracy_checkpoint.md']
    for name in documents:
        text = (ROOT/name).read_text()
        if name != 'docs/README.md':
            for target in re.findall(r'\]\(([^)]+)\)', text):
                if not target.startswith(('http:', 'https:', '#')):
                    assert ((ROOT/name).parent/target.split('#')[0]).exists(), (name, target)
    for name in ('scripts/run_cfd_native_accuracy_regression.py',
                 'scripts/check_cfd_reference3d_cube_traction_polynomial.py',
                 'scripts/audit_cfd_local_accuracy_continuation.py'):
        ast.parse((ROOT/name).read_text(), filename=name)
    result = dict(status='LOCAL CUBE REJECTED; NATIVE REGRESSION AND TRACTION CALIBRATION VERIFIED',
                  source_changes_preexisting=changes,
                  local_assessment_sha256=sha(assessment_path), local_receipt_sha256=sha(receipt_path),
                  local_true_residual=field['final_residual']['true_residual'],
                  local_raw_force_mismatch=assessment['same_domain_refinement']['raw_surface_reaction_relative_mismatch'],
                  L8_trial_permitted=False, regression_receipt_sha256=sha(regression_path),
                  native_known_answers_repeated=4, independent_forcing_checks=84,
                  traction_checkpoint_sha256=sha(traction_path), traction_controls=8,
                  maximum_traction_load_error_n=calibration['maximum_face_load_error_n'],
                  predecessor_sha256=sha(DEST/'predecessor.json'),
                  audit_source_sha256=sha(Path(__file__)),
                  document_sha256={name: sha(ROOT/name) for name in documents},
                  native_hashes=preceding['native_hashes'],
                  native_operator_changed=False, native_default_adopted=False,
                  physical_cube_force_qualification=False, persistent_goal_complete=False,
                  doc_sync=dict(program='physics_sim', diff_window='HEAD~1..HEAD',
                                source_repo=str(ROOT), current_truth_updated=True,
                                docs_index_updated=True, future_intent_inspected_preserved=True,
                                private_log_appended=True, bounded_memdb_query_limit=3,
                                memory_write_skipped='No explicit user authorization',
                                package_install_proof=False),
                  scope='one completed local cube flow, four repeated native smooth fields, prescribed polynomial traction on actual meshes; no general CFD qualification')
    output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(status=result['status'], sha256=sha(output), preserved_preexisting=True)))


if __name__ == '__main__':
    main()
