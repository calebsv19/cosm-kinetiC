"""Strict native cube accuracy screening against verified, unqualified FE fields."""
import argparse
import json
from pathlib import Path
import numpy as np
from run_cfd_native_accuracy_regression import require, save, sha
from cfd_reference3d_directional_evidence import verify

ROOT = Path(__file__).resolve().parents[1]
GATES = dict(complete_momentum=1e-11, divergence=1e-8, flux=1e-9,
             discrete_momentum=1e-9, discrete_energy=1e-9,
             transverse_force=1e-7, physical_momentum=.02, physical_energy=.03,
             separate_force_reference=.05, scalar_reference=.03,
             refinement_force=.01, refinement_scalar=.01, domain_force=.01,
             reference_raw_equilibrium=.01)


def field(path):
    path = path.resolve()
    row = json.loads(path.read_text())
    require(row['status'] == 'completed_native_cube_pressure_comparison', 'Terminal cube field')
    for name, digest in row['artifact_sha256'].items():
        require(sha(path.parent / name) == digest, 'Cube field identity: ' + name)
    contract = json.loads((path.parent / 'contract.json').read_text())
    for name, digest in contract['source_sha256'].items():
        require(sha(ROOT / name) == digest == sha(path.parent / 'source' / name), 'Source drift')
    require(contract['owned_cap_bytes'] == 1024**3 and
            contract['rss_cap_bytes'] == 1536 * 1024**2 and
            contract['fixture_wall_cap_s'] == 600 and
            contract['supervisor_wall_cap_s'] == 630 and
            contract['native_grid_cell_cap'] == 1048576, 'Native resource contract')
    c = row['control']
    require(c['numerical_peak_bytes'] < 1024**3 and c['wall_s'] < 600 and
            row['processes']['field']['peak_sampled_child_rss_bytes'] < 1536 * 1024**2 and
            row['processes']['field']['wall_s'] < 630, 'Native resource admission')
    readback = path.parent / 'readback/receipt.json'
    r = json.loads(readback.read_text())
    require(r['status'] == 'passed_complete_native_cube_readback', 'Complete original field readback')
    for name, digest in r['artifact_sha256'].items():
        require(sha(readback.parent / name) == digest, 'Readback identity')
    rc = json.loads((readback.parent / 'contract.json').read_text())
    require(rc['input_receipt_sha256'] == sha(path) and
            rc['input_binary_sha256'] == sha(path.parent / 'field.bin') and rc['flux_max'] == 1e-9,
            'Strict readback input identity')
    observed = r['control']
    for key in ('pressure_force_n', 'viscous_force_n', 'candidate_pressure_force_n'):
        np.testing.assert_allclose(c[key], observed[key], rtol=0, atol=1e-13)
    tests = dict(
        complete_momentum=observed['original_complete_scaled_momentum_residual'] <= GATES['complete_momentum'],
        divergence=observed['maximum_divergence'] < GATES['divergence'],
        flux=observed['flux_error'] < GATES['flux'],
        discrete_momentum=observed['discrete_momentum_relative_residual'] < GATES['discrete_momentum'],
        discrete_energy=observed['discrete_energy_imbalance'] < GATES['discrete_energy'],
        transverse_force=all(abs(c[k][a]) < GATES['transverse_force'] for k in
                             ('pressure_force_n', 'candidate_pressure_force_n', 'viscous_force_n') for a in (1, 2)))
    require(all(tests.values()), 'Original strict numerical gates')
    return c, observed, dict(receipt=str(path), receipt_sha256=sha(path),
                             readback_receipt_sha256=sha(readback), numerical_tests=tests)


def relative(a, b):
    return float(abs(a / b - 1))


def comparison(c, r, pressure_key):
    p = np.array(c[pressure_key]); v = np.array(c['viscous_force_n'])
    rp = np.array(r['pressure_force_n']); rv = np.array(r['raw_symmetric_viscous_force_n'])
    reaction = np.array(r['reaction_force_n'])
    errors = dict(pressure=float(np.linalg.norm(p-rp)/np.linalg.norm(rp)),
                  viscous=float(np.linalg.norm(v-rv)/np.linalg.norm(rv)),
                  total_raw_surface=float(np.linalg.norm(p+v-rp-rv)/np.linalg.norm(rp+rv)),
                  total_reaction=float(np.linalg.norm(p+v-reaction)/np.linalg.norm(reaction)))
    scalar = dict(inlet_pressure=relative(c['pressure_drop_pa'], r['inlet_pressure_pa']),
                  dissipation=relative(c['physical_dissipation_w'], r['physical_dissipation_w']))
    return dict(force_relative_errors=errors, scalar_relative_errors=scalar,
                force_comparison_screen_passed=all(e <= GATES['separate_force_reference'] for e in errors.values()),
                scalar_comparison_screen_passed=all(e <= GATES['scalar_reference'] for e in scalar.values()),
                reference_physical_accuracy_certified=False, native_physical_accuracy_certified=False)


def changes(coarse, fine, scalar):
    force = {k: relative(fine[k][0], coarse[k][0]) for k in
             ('pressure_force_n', 'candidate_pressure_force_n', 'viscous_force_n')}
    force['current_total'] = relative(fine['pressure_force_n'][0]+fine['viscous_force_n'][0],
                                     coarse['pressure_force_n'][0]+coarse['viscous_force_n'][0])
    force['candidate_total'] = relative(fine['candidate_pressure_force_n'][0]+fine['viscous_force_n'][0],
                                       coarse['candidate_pressure_force_n'][0]+coarse['viscous_force_n'][0])
    scalars = {k: relative(fine[k], coarse[k]) for k in ('pressure_drop_pa', 'physical_dissipation_w')}
    return dict(force_relative_changes=force, scalar_relative_changes=scalars,
                force_change_screen_passed=all(v <= .01 for v in force.values()),
                scalar_change_screen_passed=all(v <= .01 for v in scalars.values()) if scalar else None,
                scalars_are_length_dependent=not scalar)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('receipts', type=Path, nargs='+')
    args = parser.parse_args()
    references = {}; identity = {}
    for path in sorted((ROOT / 'build/c3d-directional-field/runs').glob('*/*receipt.json')):
        _, r = verify(path)
        length = int(r['length']); references[length] = r
        raw = float(np.linalg.norm(np.array(r['pressure_force_n'])+r['raw_symmetric_viscous_force_n']-
                                   r['reaction_force_n']) / np.linalg.norm(r['reaction_force_n']))
        identity['L'+str(length)] = dict(receipt=str(path), receipt_sha256=sha(path),
                                       raw_surface_reaction_mismatch=raw,
                                       raw_equilibrium_passed=raw <= GATES['reference_raw_equilibrium'],
                                       physical_accuracy_certified=False)
    require(set(references) == {4, 8}, 'Complete independent reference pair')
    rows = {}; controls = {}
    for path in args.receipts:
        c, observed, record = field(path)
        key = f"L{c['length']}-n{c['n']}"
        require(key not in rows, 'Distinct physical grid cases')
        controls[key] = c
        current = comparison(c, references[c['length']], 'pressure_force_n')
        candidate = comparison(c, references[c['length']], 'candidate_pressure_force_n')
        record.update(control=c, full_readback_control=observed, current=current, candidate=candidate,
                      physical_momentum_passed=observed['current_streamwise_momentum_closure'] <= .02,
                      candidate_streamwise_momentum_passed=observed['candidate_streamwise_momentum_closure'] <= .02,
                      physical_energy_passed=observed['physical_energy_imbalance'] <= .03,
                      candidate_pressure_error_improves=candidate['force_relative_errors']['pressure'] < current['force_relative_errors']['pressure'],
                      candidate_streamwise_closure_improves=observed['candidate_streamwise_momentum_closure'] < observed['current_streamwise_momentum_closure'],
                      all_native_comparison_screens_passed=current['force_comparison_screen_passed'] and
                      current['scalar_comparison_screen_passed'] and observed['current_streamwise_momentum_closure'] <= .02 and
                      observed['physical_energy_imbalance'] <= .03)
        rows[key] = record
    refinements = {}
    for length in (4, 8):
        cases = sorted((c for c in controls.values() if c['length'] == length), key=lambda c:c['n'])
        for a, b in zip(cases[:-1], cases[1:]):
            refinements[f"L{length}-n{a['n']}-to-n{b['n']}"] = changes(a, b, True)
    domains = {}
    for n in sorted({c['n'] for c in controls.values()}):
        if f'L4-n{n}' in controls and f'L8-n{n}' in controls:
            domains['n'+str(n)] = changes(controls[f'L4-n{n}'], controls[f'L8-n{n}'], False)
    result = dict(schema='physics_sim_native_cube_directional_reference_assessment_v1',
                  status='completed_strict_native_cube_physical_screen', gates=GATES,
                  reference_fields=identity, native_fields=rows, same_domain_refinement=refinements,
                  domain_sensitivity=domains, assessor_sha256=sha(Path(__file__)),
                  reference_verifier_sha256=sha(ROOT/'scripts/cfd_reference3d_directional_evidence.py'),
                  pressure_candidate_retained_as_optional_diagnostic=all(r['candidate_pressure_error_improves'] and
                      r['candidate_streamwise_closure_improves'] for r in rows.values()),
                  reference_raw_equilibrium_passed=all(r['raw_equilibrium_passed'] for r in identity.values()),
                  physical_accuracy_certified=False, native_default_adopted=False, persistent_goal_complete=False,
                  interpretation='Screens against an unqualified reference do not certify native physical forces; current viscous observer retained; candidate closure is streamwise only.')
    save(args.output.resolve(), result)
    print(json.dumps(dict(status=result['status'], reference_raw_equilibrium_passed=result['reference_raw_equilibrium_passed'],
        native_screens={k:r['all_native_comparison_screens_passed'] for k,r in rows.items()},
        refinements=refinements, domains=domains, output=str(args.output.resolve()))))

if __name__ == '__main__':
    main()
