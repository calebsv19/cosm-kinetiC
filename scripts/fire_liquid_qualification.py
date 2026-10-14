#!/usr/bin/env python3
"""Qualify native liquid energetics against independent published properties.

Prescribed total pool heat feedback is NOT a Fluid return measurement. No fit.
Constant room-temperature Cp, replenishment temperature and active depth are
explicit reduced-model assumptions, not measured transient boundary conditions.
"""
import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
from coupling.methanol_reference import SPECS, table


def qualify(growth_root, worker, reference):
    path = Path(growth_root) / 'scripts/fire_source/liquid.py'
    spec = importlib.util.spec_from_file_location('growth_liquid_adapter', path)
    liquid = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(liquid)
    reference = Path(reference)
    provenance = json.loads((reference / 'provenance.json').read_text())
    hashes = {row['file']: row['sha256'] for row in provenance['files']}
    name = 'Methanol_30_cm_HRR_Sung_2024.csv'
    if hashlib.sha256((reference / name).read_bytes()).hexdigest() != hashes[name]:
        raise ValueError('published reference bytes changed')
    published = table(reference / name, *SPECS[name])[0]
    area = math.pi * .301**2 / 4
    # Independent properties from WebBook and TN2162r1; not HRR/MLR fitting.
    cp, latent = 81.13 / .03204, 35210 / .03204
    config = dict(area_m2=area, mass_kg=area * .005 * 791.01,
        cp_j_kg_k=cp, latent_j_kg=latent, boiling_k=337.85,
        feed_k=293.15, minimum_k=273.15, combustion_j_kg=19.9e6)
    power = .082 * 18400
    flux = dict(radiation_w_m2=.055 * 18400 / area,
                convection_w_m2=(.082 - .055) * 18400 / area, loss_w_m2=0.)
    initial = liquid.initial(config, worker, config['boiling_k'], tick_seconds=(1, 1))
    state = initial
    receipts = []
    for tick in range(10, 601, 10):
        state, receipt = liquid.advance(config, worker, state, flux, tick, (1, 1))
        receipts.append(receipt)
        if tick == 300:
            recovered = json.loads(json.dumps(state))
    replay = recovered
    replay_receipts = []
    for tick in range(310, 601, 10):
        replay, r = liquid.advance(config, worker, replay, flux, tick, (1, 1))
        replay_receipts.append(r)
    if replay != state or replay_receipts != receipts[30:]:
        raise ValueError('exact checkpoint recovery failed')
    single, _ = liquid.advance(config, worker, initial, flux, 600, (1, 1))
    cadence_difference = abs(single['supplied_mass_kg'] - state['supplied_mass_kg'])
    if cadence_difference > 1e-12:
        raise ValueError('constant-forcing cadence check failed')
    rate = state['supplied_mass_kg'] / 600
    measured = .013 * area
    specific = latent + cp * (config['boiling_k'] - config['feed_k'])
    # Synthetic heat/cool pulse: not claimed as observed pool warm-up.
    cold = liquid.initial(config, worker, config['feed_k'], tick_seconds=(1, 1))
    hot, heating = liquid.advance(config, worker, cold, flux, 20, (1, 1))
    cooled, cooling = liquid.advance(config, worker, hot,
        dict(radiation_w_m2=0., convection_w_m2=-1000., loss_w_m2=0.), 40, (1, 1))
    properties = {
        'report': 'https://doi.org/10.6028/NIST.TN.2162r1',
        'webbook': 'https://webbook.nist.gov/cgi/cbook.cgi?Name=methanol&cTC=on&cTG=on&cTP=on',
        'report_table_2': {'diameter_m': .301, 'mass_flux_kg_m2_s': .013,
            'mass_flux_reported_uncertainty_kg_m2_s': .0004, 'ideal_hrr_w': 18400},
        'report_table_10': {'total_feedback_fraction': .082, 'reported_uncertainty': .020,
            'radiative_feedback_fraction': .055, 'radiative_reported_uncertainty': .012,
            'note': 'Reported feedback from cited prior experiments; not synchronized return-channel data.'},
        'report_table_12': {'liquid_surface_temperature_k': 338.15,
            'thermal_drift_k': 1., 'note': 'Reported spread reflects warming over about one hour, not a confidence interval.'},
        'report_table_a1': {'density_kg_m3_at_293_15k': 791.01,
            'boiling_temperature_k': 337.85, 'lower_combustion_j_kg': 19.9e6, 'molecular_mass_kg_mol': .03204},
        'webbook_heat_capacity': {'value_j_mol_k': 81.13, 'temperature_k': 298.15,
            'reference': 'Carlson and Westrum 1971', 'assumption': 'Frozen Cp through heating to boiling; not temperature-resolved qualification.'},
        'webbook_latent_heat': {'value_j_mol': 35210, 'temperature_k': 337.7,
            'reference': 'Majer and Svoboda 1985'}}
    assessment = {'schema': 'fire_liquid_material_qualification/v1',
        'material': 'methanol', 'parameters_fitted': False,
        'surface_numerics_qualified': True, 'coupled_thermal_feedback_qualified': False,
        'physical_calibration_qualified': False, 'config_si': config,
        'properties_and_measurements': properties,
        'reference_commit': provenance['repository_commit'], 'reference_sha256': hashes[name],
        'macfp_steady_mass_loss_kg_s': published[2], 'macfp_steady_hrr_w': published[1],
        'nist_report_mass_loss_kg_s': measured, 'predicted_no_loss_mass_loss_kg_s': rate,
        'relative_mass_loss_error': rate / measured - 1,
        'potential_complete_combustion_power_w': rate * config['combustion_j_kg'],
        'prescribed_absorbed_power_w': power,
        'feedback_uncertainty_only_mass_rate_range_kg_s': [(.082-.020)*18400/specific, (.082+.020)*18400/specific],
        'measured_mass_rate_reported_range_kg_s': [(.013-.0004)*area, (.013+.0004)*area],
        'unmodeled_loss_implied_by_measured_rate_w': power - measured * specific,
        'loss_diagnostic_used_as_input': False,
        'temperature_offset_from_measured_surface_k': config['boiling_k'] - 338.15,
        'temperature_is_imposed_boiling_property': True,
        'cadence_mass_difference_kg': cadence_difference, 'exact_restart_passed': True,
        'max_energy_residual_j': max(abs(r['budget_si']['residual_j']) for r in receipts),
        'assumptions': ['Uniform well-mixed circular pool; no spatial liquid gradients.',
            'Unlimited constant-mass external replenishment at assumed 293.15 K.',
            'Active depth 5 mm is synthetic; steady mass rate is independent of this depth.',
            'No sub-boiling evaporation, dryout, radiation transport, or combustion kinetics.',
            'Total/radiative feedback fractions are prescribed; existing Fluid return carries wind only.',
            'No-loss mean overpredicts measured mass rate; losses and independent thermal boundary data remain needed.',
            'Hot-gas observations remain outside current constant-property Fluid regime.']}
    return liquid.seal(assessment), {'initial': initial, 'final': state, 'receipts': receipts,
        'synthetic_heat_cool': {'heated': hot, 'cooled': cooled, 'heating': heating, 'cooling': cooling}}

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root', 'worker', 'reference', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    a.output.mkdir(parents=True, exist_ok=True)
    if any(a.output.iterdir()): raise ValueError('fresh qualification output required')
    assessment, evidence = qualify(a.growth_root, a.worker, a.reference)
    (a.output / 'assessment.json').write_text(json.dumps(assessment, indent=2) + '\n')
    (a.output / 'native-evidence.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps({k: assessment[k] for k in ('surface_numerics_qualified',
        'predicted_no_loss_mass_loss_kg_s', 'nist_report_mass_loss_kg_s',
        'relative_mass_loss_error', 'max_energy_residual_j', 'physical_calibration_qualified')}, indent=2))
