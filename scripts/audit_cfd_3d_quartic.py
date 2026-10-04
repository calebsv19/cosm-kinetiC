#!/usr/bin/env python3
"""Audit higher-order support, immutable controls and original force/resource gates."""
import hashlib
import json
import math
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt, force_comparison
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'build/c3d-quartic'


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def main():
    baseline = json.loads((DATA / 'baseline.json').read_text())
    changed = [p for p, digest in baseline.items() if sha(ROOT / p) != digest]
    assert set(changed) <= {'docs/current_truth.md', 'docs/README.md', 'make/rules-tools.mk'}, changed
    records, receipts, hashes = {}, {}, {}
    for path in sorted((DATA / 'runs').glob('*/*-receipt.json')):
        receipt, row = verify_receipt(path)
        name = path.name.removesuffix('-receipt.json')
        assert name not in records
        records[name], receipts[name] = row, receipt
        hashes[str(path)] = sha(path)
        if row is not None:
            assert (row['velocity_degree'], row['pressure_degree']) == (4, 3)
            assert row['verified_volume_product_degree'] == 6
            assert row['matrix_free'] and row['pressure_preconditioner']['kind'] == 'exact_DG_cubic_mass'
            with np.load(path.with_name(name + '.npz'), allow_pickle=False) as field:
                assert int(field['velocity_degree']) == 4 and int(field['pressure_degree']) == 3
    required = {'empty-L4', 'empty-L8', 'cube-L4', 'cube-L4-tight', 'cube-L4-normal',
                'cube-L4-body4', 'cube-L4-body4-normal', 'cube-L4-body4-normal-chunk128'}
    assert required <= records.keys()
    calibration = {}
    series = sum(1 / (n*n*m*m*((n*math.pi/2)**2 + (m*math.pi/2)**2))
                 for n in range(1, 256, 2) for m in range(1, 256, 2))
    for length in (4, 8):
        row = records[f'empty-L{length}']
        pressure = length * .008 / (64 * 4 * series / (.1 * math.pi**4))
        calibration[str(length)] = {key: abs(row[key]/value - 1) for key, value in
            (('inlet_pressure_pa', pressure), ('physical_dissipation_w', pressure * .008))}
        assert max(calibration[str(length)].values()) < .0001
    base, tight = records['cube-L4'], records['cube-L4-tight']
    assert base['tetrahedra'] == 4992 and base['identity'] == tight['identity']
    old = next((ROOT / 'build/c3d-preconditioner/runs').glob('*/cube-factor.json'))
    assert base['identity']['mesh_sha256'] == json.loads(old.read_text())['identity']['mesh_sha256']
    tighter = force_comparison(tight, base)
    assert max([*tighter['component_relative_changes'].values(),
                *tighter['scalar_relative_changes'].values()]) < 1e-7
    assert tight['target'] == 1e-12 and tight['final_residual']['true_residual'] < base['final_residual']['true_residual']
    refinement = {name: force_comparison(records[name], base)
                  for name in ('cube-L4-normal', 'cube-L4-body4')}
    assert records['cube-L4-normal']['tetrahedra'] == 6720
    assert records['cube-L4-body4']['tetrahedra'] == 10752
    assert all(not row['physical_force_gate_passed'] for row in refinement.values())
    failed = receipts['cube-L4-body4-normal']
    assert records['cube-L4-body4-normal'] is None and failed['diagnostic_failure']['kind'] == 'resource_cap'
    smaller = records['cube-L4-body4-normal-chunk128']
    if smaller is not None:
        assert smaller['tetrahedra'] == 13824 and smaller['chunk_size'] == 128
        refinement['body4_normal_chunk128'] = force_comparison(smaller, records['cube-L4-body4'])
    else:
        assert receipts['cube-L4-body4-normal-chunk128']['diagnostic_failure']['kind'] == 'resource_cap'
    log = (DATA / 'focused-tests.log').read_text()
    assert 'Ran 7 tests' in log and 'Ran 1 test' in log and log.count('\nOK\n') == 2
    assert 'Traceback' not in log and 'FAILED (' not in log
    solved = next(json.loads(line) for line in log.splitlines() if line.startswith('{"schema": "c3d_quartic_solved_controls_v1"'))
    assert len(solved['cases']) == 24
    maxima = {key: max(row[key] for row in solved['cases']) for key in
              ('velocity_error', 'pressure_error', 'pressure_traction_error', 'viscous_traction_error',
               'true_residual', 'wall_divergence_max', 'volume_divergence_max')}
    assert max(maxima.values()) < 1e-9
    rejected = DATA / 'rejected-intorder6-source'
    for name, digest in json.loads((rejected / 'manifest.json').read_text()).items():
        assert sha(rejected / name) == digest
    # Verify prior observer artifacts and all solver receipts without rewriting prior audits.
    equilibrium = json.loads((ROOT / 'build/c3d-equilibrium/checkpoint-audit.json').read_text())
    for p, digest in equilibrium['observer_receipts_sha256'].items():
        path = Path(p)
        assert sha(path) == digest
        receipt = json.loads(path.read_text())
        for artifact, value in receipt['artifact_sha256'].items(): assert sha(Path(artifact)) == value
        for name, value in receipt['source_sha256'].items(): assert sha(path.parent / 'source' / name) == value
        input_path = Path(json.loads(path.with_name(path.name.removesuffix('-receipt.json') + '.json').read_text())['input_receipt'])
        verify_receipt(input_path)
    spatial = json.loads((ROOT / 'build/c3d-spatial/checkpoint-audit.json').read_text())
    for p, digest in spatial['reference_receipts_sha256'].items():
        assert sha(Path(p)) == digest
        verify_receipt(Path(p))
    method = json.loads((ROOT / 'build/c3d-reference-method/completion-audit.json').read_text())
    for p, digest in method['protected_build_hashes'].items(): assert sha(ROOT / p) == digest
    costs = {name: {key: receipt[key] for key in ('wall_s', 'peak_observed_rss_bytes', 'diagnostic_failure')}
             for name, receipt in receipts.items()}
    sources = {str(p.relative_to(ROOT)): sha(p) for folder in ('scripts', 'tests', 'docs')
               for p in (ROOT / folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline}
    audit = dict(schema='physics_sim_c3d_quartic_audit_v1', status='quartic_support_verified_force_reference_unqualified',
        persistent_goal_complete=False, stage_1_complete=False, physical_accuracy_certified=False,
        native_source_unchanged=True, predecessor_fields_and_workers_unchanged=True,
        reference_receipts_sha256=hashes, costs=costs, empty_fourier_calibration=calibration,
        manufactured_maximum_errors=maxima, refinement=refinement, tighter_residual_force_stability=tighter,
        requested_tighter_target_attained=tight['final_residual']['true_residual'] <= tight['target'],
        current_source_sha256=sources, changed_preexisting_files=changed,
        baseline_sha256=sha(DATA / 'baseline.json'), test_log_sha256=sha(DATA / 'focused-tests.log'),
        rejected_underintegration_manifest_sha256=sha(rejected / 'manifest.json'),
        rejected_natural_order6_source_sha256=sha(DATA / 'rejected-natural-order6-test-source.py'),
        rejected_natural_order6_log_sha256=sha(DATA / 'rejected-natural-order6-solved-tests.log'),
        predecessor_audit_sha256=sha(ROOT / 'build/c3d-equilibrium/checkpoint-audit.json'),
        committed=False, packaged=False, installed=False,
        next_gate='grade and condition the reference mesh using stress-defect attribution; test memory reduction before a larger quartic force pair; preserve raw traction and original gates')
    (DATA / 'checkpoint-audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    print(json.dumps({k: audit[k] for k in ('status', 'refinement', 'empty_fourier_calibration',
        'requested_tighter_target_attained', 'predecessor_fields_and_workers_unchanged')}), flush=True)


if __name__ == '__main__': main()
