#!/usr/bin/env python3
"""Run one contained native wind refinement or aligned exchange comparison."""
import argparse
import json
import os
from fractions import Fraction
from pathlib import Path
from coupling.wind_sensitivity import refined, cadence
from coupling.driven_wind_qualification import frames, observations
from fire_fluid_regression import qualify


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base-config', type=Path, required=True)
    p.add_argument('--grid', type=int, choices=(8,16,32))
    p.add_argument('--exchange', type=Fraction)
    p.add_argument('--duration', type=Fraction)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if (a.exchange is None) != (a.duration is None):
        p.error('--exchange and --duration must be supplied together')
    c = json.loads(a.base_config.read_text())
    if a.grid is not None:
        c = refined(c, a.grid)
    if a.exchange is not None:
        c = cadence(c, a.exchange, a.duration)
    # Refuse a used root; do not adopt or overwrite retained evidence.
    if a.output.exists():
        p.error('output must be a fresh task-owned directory')
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(a.output.parent / (a.output.name+'-native-evidence'))
    result = qualify(c, a.output)
    (a.output/'observations.json').write_text(json.dumps(observations(c,frames(a.output)),indent=2)+'\n')
    print(json.dumps(result,indent=2))

if __name__ == '__main__':
    main()
