#!/usr/bin/env python3
"""Qualify one inert stationary box in the native coupled warm plume."""
import argparse
import json
import math
import os
from pathlib import Path
from coupling.box_qualification import coupled, frozen, spatial, write
from surface_sources.growth_fire_v1 import require, sealed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root', 'fire-worker', 'fluid-worker', 'clock-worker', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args(); root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(root / 'native-evidence')
    workers = (args.growth_root, args.fire_worker, args.fluid_worker, args.clock_worker)
    hosts = {}; reports = {}; rows = {}
    for name, box in (('no-box', False), ('box', True)):
        directory = root / name; directory.mkdir()
        hosts[name], rows[name], reports[name] = coupled(workers, directory, box)
        print(name + ' live coupled/recovery controls complete', flush=True)
    frames = [row['source']['frame'] for row in rows['no-box'][1:]]
    write(root / 'matched-source-frames.json', frames)
    schedule = hosts['no-box'].plan(rows['no-box'])['schedule']
    refinement = {}; frozen_reports = {}
    for name, box in (('no-box', False), ('box', True)):
        results = []; checks = []
        for grid in (8, 16, 32):
            directory = root / ('matched-' + name + '-' + str(grid)); directory.mkdir()
            result, report = frozen(workers, directory, frames, schedule, grid, box)
            results.append(result); checks.append(report)
            print(name + ' matched grid ' + str(grid) + ' complete', flush=True)
        refinement[name] = spatial(results, checks); frozen_reports[name] = checks
    delta = {name: [b - a for a, b in zip(reports['no-box']['checks']['wind_probes'][name]['velocity_m_s'],
                                          reports['box']['checks']['wind_probes'][name]['velocity_m_s'])]
             for name in reports['box']['checks']['wind_probes']}
    changed = math.sqrt(math.fsum(v * v for values in delta.values() for v in values))
    require(changed > 1e-6, 'box must change native wind probes')
    # Preset gates: exact exclusion/budgets/recovery, decreasing common-partition
    # scalar differences and fixed-world probe differences over the declared meshes.
    passed = all(v['scalar_differences_decrease'] and v['probe_differences_decrease'] for v in refinement.values())
    summary = sealed({'schema': 'fire_fluid_inert_box_qualification/v1', 'status': 'passed' if passed else 'refinement_unqualified',
                      'duration_s': 3.2, 'initial_flow': 'both controls from rest', 'source_nodes': [8, 8],
                      'injection_depth_m': .125, 'fluid_dt_s': .005, 'exchange_s': .1,
                      'coupled': reports, 'matched_source_refinement': refinement, 'matched_cases': frozen_reports,
                      'box_minus_no_box_probe_velocity_m_s': delta, 'probe_change_l2_m_s': changed,
                      'wind_return_enabled': True, 'thermal_return_enabled': False,
                      'box_burnable': False, 'box_thermal_boundary': 'insulated',
                      'physical_calibration_qualified': False, 'asymptotic_order_qualified': False})
    write(root / 'summary.json', summary)
    print(json.dumps({'status': summary['status'], 'refinement': refinement, 'probe_change_l2_m_s': changed}, indent=2), flush=True)
    require(passed, 'declared refinement gates remain unqualified; preserve report')


if __name__ == '__main__': main()
