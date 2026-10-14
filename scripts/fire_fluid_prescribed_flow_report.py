#!/usr/bin/env python3
import argparse,json
from pathlib import Path
from coupling.prescribed_flow_report import report
p=argparse.ArgumentParser(description='Original-source spatial response under shared coarse thermal forcing')
for name in ('root','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();result=report(a.root)
with a.output.open('x') as f:f.write(json.dumps(result,indent=2)+'\n')
print(result['status'])
