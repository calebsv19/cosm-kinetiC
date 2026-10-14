"""Read terminal temporal runs without executing the simulation."""
import argparse
import json
from pathlib import Path
from coupling.flow_time_report import report

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','spatial','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();result=report(a.root,a.spatial)
    with a.output.open('x') as f:json.dump(result,f,indent=2,allow_nan=False);f.write('\n')
    print(result['status'])
