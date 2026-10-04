#!/usr/bin/env python3
"""Qualify the fixed confined Stokes rectangle fixture, not general CFD."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def fields(line):
    return dict(part.split('=', 1) for part in line.split() if '=' in part)


def read(path):
    lines = path.read_text().splitlines()
    domain = fields(next(line for line in lines if line.startswith('domain_length_m=')))
    solve = fields(next(line for line in lines if line.startswith('fixed_stokes_obstacle_physical_gate=')))
    assert float(domain['domain_length_m']) == 4, 'reference domain mismatch'
    residual = float(solve['relative_linear_residual'])
    assert math.isfinite(residual) and 0 <= residual <= 1e-11, 'unqualified linear solve'
    mesh = fields(next(line for line in lines if line.startswith('local_levels=')))
    force = fields(next(line for line in lines if line.startswith('n=')))
    energy = fields(next(line for line in lines if line.startswith('energy_strain=')))
    assert int(mesh['local_levels']) == 7 and int(force['uniform']) == 0
    assert abs(float(domain['base_dx_m']) - 2/int(force['n'])) < 1e-12
    values = {key: float(force[key]) for key in
              ('pressure', 'viscous', 'total', 'divergence', 'closed_balance', 'lift')}
    assert all(math.isfinite(value) for value in values.values())
    pressure_ref, viscous_ref = .003633153361585, .002347529940743
    errors = {key: abs(values[key] / reference - 1) for key, reference in
              [('pressure', pressure_ref), ('viscous', viscous_ref),
               ('total', pressure_ref + viscous_ref)]}
    power, strain = float(energy['energy_boundary']), float(energy['energy_strain'])
    assert math.isfinite(power) and math.isfinite(strain) and power > 0 and strain > 0
    imbalance = abs(power - strain) / power
    assert values['divergence'] < 1e-8 and values['closed_balance'] < 1e-9
    assert abs(values['lift']) < 1e-9
    return dict(n=int(force['n']), cells=int(force['cells']), values=values,
                linear_relative_residual=residual, domain_length_m=4,
                reference_relative_errors=errors, physical_energy_imbalance=imbalance,
                physically_qualified=max(errors.values()) <= .02 and imbalance <= .02,
                evidence=str(path), evidence_sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--logs', type=Path, nargs=3, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows = sorted((read(path) for path in args.logs), key=lambda row: row['n'])
    assert rows[1]['n'] == 2 * rows[0]['n'] and rows[2]['n'] == 2 * rows[1]['n']
    changes = [{key: abs(b['values'][key] - a['values'][key]) / abs(b['values'][key])
                for key in ('pressure', 'viscous', 'total')} for a, b in zip(rows, rows[1:])]
    assert rows[1]['physically_qualified'] and rows[2]['physically_qualified']
    assert max(changes[-1].values()) < .01
    assert all(changes[-1][key] < changes[0][key] for key in changes[-1])
    result = dict(schema='physics_sim_local_stokes_rectangle_qualification_v1',
                  scope='2D steady confined Stokes rectangle, fixed natural-traction outlet',
                  domain_m=[4, 2, .5], obstacle_bounds_m=[1.5, .75, 2.5, 1.25],
                  density_kg_m3=1, dynamic_viscosity_pa_s=.1, mean_inlet_m_s=.002,
                  rows=rows, consecutive_force_changes=changes,
                  component_force_and_energy_qualified=True,
                  general_cfd_certified=False, outlet_sensitivity_qualified=False,
                  matched_accuracy_cost_qualified=False)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(component_force_and_energy_qualified=True,
                          last_force_changes=changes[-1])))


if __name__ == '__main__':
    main()
