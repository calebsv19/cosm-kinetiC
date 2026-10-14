#!/usr/bin/env python3
import argparse,json
from pathlib import Path
from coupling.flow_spatial_report import report
p=argparse.ArgumentParser(description='Fixed-scalar independent 64/128 flow comparison')
for name in ('root','prior64','output'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();result=report(a.root,a.prior64)
with a.output.open('x') as f:f.write(json.dumps(result,indent=2)+'\n')
print(result['status'])
