#!/usr/bin/env python3
import argparse,json
from pathlib import Path
from coupling.matched_original_report import report
p=argparse.ArgumentParser(description='Original-source matched-velocity heat attribution')
for name in ('root','coupled','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();result=report(a.root,a.coupled)
with a.output.open('x') as f:f.write(json.dumps(result,indent=2)+'\n')
print(result['status'])
