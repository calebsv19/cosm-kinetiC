#!/usr/bin/env python3
"""Run offline coupling using this package's exact workers and adapter bytes."""
import argparse
import json
import os
from pathlib import Path
import sys
from package_runtime import verify, external

ROOT = Path(__file__).resolve().parents[1]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', type=Path)
    p.add_argument('mode', choices=['capabilities', 'periodic', 'open', 'passive'])
    args, rest = p.parse_known_args()
    manifest = verify(ROOT)
    if args.mode == 'capabilities':
        if rest: p.error('unexpected capability arguments')
        print(json.dumps(manifest, sort_keys=True)); return
    if args.runtime is None or not args.runtime.is_absolute():
        p.error('--runtime must be an absolute external directory')
    runtime = external(ROOT, args.runtime)
    os.environ['PHYSICS_SIM_PACKAGE_RUNTIME_ROOT'] = str(runtime)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT'] = str(runtime / 'experiments')
    # Never inherit a developer checkout or alternate binary selection.
    for role, variable in [('passive','PHYSICS_SIM_PASSIVE_WORKER'),('evolving','PHYSICS_SIM_ATMOSPHERE_WORKER'),('open','PHYSICS_SIM_OPEN_ATMOSPHERE_WORKER')]:
        os.environ[variable] = str(ROOT / manifest['workers'][role])
    module = {'periodic':'coupled_atmosphere', 'open':'coupled_open_atmosphere', 'passive':'coupled_passive'}[args.mode]
    # Bind the existing CLI contract without changing its checkpoint format.
    from surface_sources.growth_fire_v1 import strict_load
    index = rest.index('--config') if '--config' in rest else -1
    if index < 0 or index+1 >= len(rest): p.error('--config is required')
    config = strict_load(rest[index+1])
    role = {'periodic':'evolving','open':'open','passive':'passive'}[args.mode]
    selected = ROOT / manifest['workers'][role]
    if Path(config.get('worker','')).absolute() != selected:
        p.error('config worker must equal this package worker: '+str(selected))
    for flag in ('--store',):
        i = rest.index(flag) if flag in rest else -1
        if i < 0 or i+1 >= len(rest):p.error(flag+' is required')
        external(ROOT, rest[i+1])
    sys.argv = [module, *rest]
    __import__(module).main()

if __name__ == '__main__':
    try: main()
    except (ValueError, OSError, KeyError) as error: raise SystemExit(str(error))
