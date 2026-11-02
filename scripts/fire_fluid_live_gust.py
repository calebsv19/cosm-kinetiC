#!/usr/bin/env python3
"""One bounded live32/64cubed gust or box-wake comparison; local source workflow."""
import argparse
from pathlib import Path
import sys
from passive_atmosphere import ROOT
from surface_sources.growth_fire_v1 import require
from coupling.live_gust import run_case

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('growth-root','fire-worker','fluid-worker','clock-worker','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--grid',type=int,choices=(32,64),required=True)
    p.add_argument('--box',action='store_true')
    p.add_argument('--native-packet-steps',type=int,choices=range(1,21),default=None)
    p.add_argument('--journal-codec',choices=('lossless_zlib/v1','lossless_f64_zlib/v1'),default='lossless_f64_zlib/v1')
    a=p.parse_args()
    workspace=ROOT.parent.parent if ROOT.parent.name=='_worktrees' else ROOT.parent
    helper=workspace/'bin/verification_retention.py'
    require(helper.is_file(),'live comparison needs the CodeWork task-output consumer-check helper before native admission')
    sys.path.insert(0,str(helper.parent))
    workers=(a.growth_root,a.fire_worker,a.fluid_worker,a.clock_worker)
    run_case(workers,a.output,a.grid,a.box,journal_codec=a.journal_codec,native_packet_steps=a.native_packet_steps or (5 if a.box and a.grid==64 else 20))

if __name__=='__main__':main()
