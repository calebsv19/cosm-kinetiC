#!/usr/bin/env python3
"""Fixed-spacing, fixed-inlet 2D Stokes outlet-distance sensitivity."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def read(path, n, length):
    lines = path.read_text().splitlines()
    force = dict(part.split('=', 1) for part in next(line for line in lines
                 if line.startswith('n=')).split())
    assert int(force['n']) == n and int(force['uniform']) == 0
    assert 'local_levels=7 lattice_scale=128' in lines
    domain = next((line for line in lines if line.startswith('domain_length_m=')), None)
    if domain is None:
        # Older baseline fixture had a fixed 4x2 domain and no length option.
        assert length == 4 and 'separate_force_two_percent_gate=passed' in lines
    else:
        metadata = dict(part.split('=', 1) for part in domain.split())
        assert float(metadata['domain_length_m']) == length
        assert abs(float(metadata['base_dx_m']) - 2/n) < 1e-12
    values = {key: float(force[key]) for key in ('pressure', 'viscous', 'total')}
    assert all(math.isfinite(v) and v > 0 for v in values.values())
    assert float(force['divergence']) < 1e-8 and float(force['closed_balance']) < 1e-9
    return dict(n=n, length_m=length, force_n=values, evidence=str(path),
                evidence_sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def compare(a, b):
    assert a['n'] == b['n']
    changes = {key: abs(a['force_n'][key] - b['force_n'][key])/abs(b['force_n'][key])
               for key in a['force_n']}
    assert max(changes.values()) < .01
    return dict(n=a['n'], lengths_m=[a['length_m'], b['length_m']], relative_changes=changes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows = []
    for n, length in ((64, 4), (64, 6), (64, 8), (128, 4), (128, 6)):
        name = f'local-n{n}-l7.log' if length == 4 else f'outlet-local-n{n}-L{length}.log'
        rows.append(read(args.root/name, n, length))
    comparisons = [compare(rows[0], rows[1]), compare(rows[1], rows[2]), compare(rows[3], rows[4])]
    assert max(comparisons[1]['relative_changes'].values()) < max(comparisons[0]['relative_changes'].values())
    result = dict(schema='physics_sim_refined_stokes_outlet_sensitivity_v1',
                  scope='Fixed confined steady Stokes rectangle; parabolic inlet and natural vector-Laplacian traction outlet',
                  rows=rows, comparisons=comparisons, maximum_relative_change_limit=.01,
                  qualified=True, transient_outlet_qualified=False, general_cfd_certified=False)
    args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(outlet_distance_sensitivity_qualified=True, comparisons=comparisons)))


if __name__ == '__main__':
    main()
