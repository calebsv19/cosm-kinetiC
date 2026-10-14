#!/usr/bin/env python3
"""Read completed full-grid qualification without solver execution."""
import argparse,json
from pathlib import Path
from coupling.resolution_report import report
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();result=report(a.root,json.loads((a.root/'source-restriction.json').read_text()))
with a.output.open('x') as out:out.write(json.dumps(result,indent=2)+'\n')
print(result['status'])
