#!/usr/bin/env python3
"""Compare completed source-transport probes without re-running a solver."""
import argparse,json
from coupling.accuracy_report import report
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,required=True);p.add_argument('--prior',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();r=report(a.root,a.prior)
with a.output.open('x') as out:out.write(json.dumps(r,indent=2)+'\n')
print(r['status'])
