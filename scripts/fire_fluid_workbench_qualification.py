#!/usr/bin/env python3
"""Retained bounded authoring milestone: feedback control, exact replay and native export."""
import argparse
import json
import os
from pathlib import Path
from coupling.workbench import Workbench, DEFAULTS
from surface_sources.growth_fire_v1 import sealed, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('output', 'growth-root', 'fire-worker', 'fluid-worker', 'clock-worker'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args(); args.output.mkdir(parents=True, exist_ok=False)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(args.output.resolve() / 'native-evidence')
    workers = (args.growth_root, args.fire_worker, args.fluid_worker, args.clock_worker)
    workbench = Workbench(args.output / 'workbench', workers)
    cases = {}
    try:
        for name, settings in [('enabled', DEFAULTS), ('repeat', DEFAULTS), ('disabled', dict(DEFAULTS, feedback=False))]:
            initial = workbench.create(settings)
            for revision in range(initial['windows']): workbench.step(revision, 'qualify-' + str(revision))
            state = workbench.state(); require(state['complete'] and not state['playing'], 'terminal authoring state')
            reopened = Workbench(workbench.root, workers)
            try: require(reopened.state() == state, 'exact fresh-process-model reopen')
            finally: reopened.close()
            (args.output / (name + '-state.json')).write_text(json.dumps(state, indent=2) + '\n')
            exported = workbench.export() if name != 'repeat' else None
            cases[name] = {'run_id': initial['run_id'], 'head': state['frames'][-1], 'export': exported}
            print(name + ' native scene complete', flush=True)
        require(cases['enabled']['head'] == cases['repeat']['head'], 'repeatable full native fields and checkpoint identity')
        a = cases['enabled']['head']; b = cases['disabled']['head']
        require(a['surface']['fields']['heat'] != b['surface']['fields']['heat'], 'wind must change Fire state')
        delta = a['metrics']['energy_j'] - b['metrics']['energy_j']
        require(abs(delta) > 1e-6, 'wind must change integrated source')
        summary = sealed({'schema': 'fire_fluid_usable_workflow_qualification/v1', 'status': 'passed',
                          'settings': DEFAULTS, 'cases': cases, 'enabled_minus_disabled_stored_energy_j': delta,
                          'exact_repeat': True, 'exact_reopen': True,
                          'physical_calibration_qualified': False, 'thermal_return_enabled': False})
        (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(json.dumps({'status': summary['status'], 'energy_difference_j': delta}, indent=2))
    finally: workbench.close()


if __name__ == '__main__': main()
