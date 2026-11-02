#!/usr/bin/env python3
"""Run only the bounded fixed-input gust reference after independent acceptance."""
import argparse,json,os,sys,traceback
from pathlib import Path
P=Path(__file__).resolve().parents[1];C=P.parents[1];sys.path[:0]=[str(P/'scripts'),str(C/'bin')]
import verification_retention
from coupling.accuracy_gust_study import run,write
from coupling.source_replay import validate

def record_owned_failure(output,exc):
    output=Path(output).resolve()
    try:owner=json.loads((output/'process.json').read_text())
    except (OSError,ValueError):return False
    if (owner.get('pid')!=os.getpid() or owner.get('root')!=str(output) or
        owner.get('script')!=str(P/'scripts/coupling/accuracy_gust_study.py')):return False
    write(output/'error.json',{'error':str(exc),'traceback':traceback.format_exc(),'preserved':True,'action':'Inspect exact attempt; do not blindly retry or reuse case roots'})
    return True

def main():
    p=argparse.ArgumentParser();p.add_argument('--workers',type=Path,required=True);p.add_argument('--pulse',type=Path,required=True);p.add_argument('--references',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--budget-root',type=Path,required=True);a=p.parse_args()
    if a.output.exists() or a.output.is_symlink():raise ValueError("fixed-input gust requires a fresh output root; existing content preserved")
    require=json.loads(a.references.read_text());assert require['status']=='passed','Independent references must pass before gust campaign'
    w=json.loads(a.workers.read_text());workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')]);pulse=validate(json.loads(a.pulse.read_text()))
    import hashlib
    for v in w['binaries'].values():assert hashlib.sha256(Path(v['path']).read_bytes()).hexdigest()==v['sha256']
    try:run(a.output,workers,pulse,a.budget_root)
    except BaseException as exc:
        record_owned_failure(a.output,exc)
        raise
if __name__=='__main__':main()
