#!/usr/bin/env python3
"""Assess matched S3 report files without advancing simulations or changing evidence."""
import argparse
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
sys.path.insert(0,str(Path(__file__).resolve().parent/'agent_session'))
from wake_qualification import refinement_screen

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--kind',choices=['spatial','temporal','outlet'],required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--tolerance',type=float,default=.02)
    p.add_argument('reports',type=Path,nargs='+')
    a=p.parse_args()
    reports=[json.loads(path.read_text()) for path in a.reports]
    result=refinement_screen(reports,a.kind,a.tolerance)
    result['reports']=[str(path.resolve()) for path in a.reports]
    with a.output.open('x') as f: json.dump(result,f,indent=2,allow_nan=False);f.write('\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__': main()
