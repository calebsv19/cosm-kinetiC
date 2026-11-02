#!/usr/bin/env python3
"""Compare continuous, abrupt and ramped native inlet wind through Fire feedback."""
import argparse,os
from pathlib import Path
from coupling.driven_wind_qualification import run_suite
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
    os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(a.output.parent/'native-evidence')
    run_suite((a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker),a.output)
if __name__=='__main__':main()
