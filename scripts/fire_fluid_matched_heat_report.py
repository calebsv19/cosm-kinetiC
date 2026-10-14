"""Inspect one terminal matched-flow diagnostic and its accepted references."""
import argparse
import json
from pathlib import Path
from coupling.matched_heat_report import report

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','control','coupled-fine','output'):
        p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    result=report(a.root,a.control,a.coupled_fine)
    with a.output.open('x') as f:
        json.dump(result,f,indent=2,allow_nan=False);f.write('\n')
    print(result['status'])
