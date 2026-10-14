#!/usr/bin/env python3
"""Explicit local source-development Fire/Fluid joint checkpoint operations."""
import argparse
import json
from pathlib import Path
from coupling.framework import Framework
from surface_sources.growth_fire_v1 import strict_load


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',type=Path,required=True);p.add_argument('--store',type=Path,required=True)
    sub=p.add_subparsers(dest='action',required=True);sub.add_parser('init');sub.add_parser('inspect')
    export=sub.add_parser('export');export.add_argument('--output',type=Path,required=True)
    wind=sub.add_parser('observe-velocity');wind.add_argument('--height-m',type=float,required=True)
    step=sub.add_parser('step');step.add_argument('--revision',type=int,required=True);step.add_argument('--operation-id',required=True)
    a=p.parse_args();host=Framework(strict_load(a.config),a.store)
    if a.action=='export':
        from coupling.authoring import export
        print(json.dumps(export(host,a.output),allow_nan=False));return
    if a.action=='observe-velocity':
        from coupling.velocity import observation
        cp=host.inspect();print(json.dumps(observation(cp,host.config,cp['evidence']['window_ticks'][1],0,
            height_m=a.height_m),allow_nan=False));return
    cp=host.initialize() if a.action=='init' else host.inspect() if a.action=='inspect' else host.step(a.revision,a.operation_id)
    print(json.dumps(cp,allow_nan=False))
if __name__=='__main__':main()
