#!/usr/bin/env python3
"""Run one admitted scalar-only candidate; no default activation or old-root reuse."""
import argparse,hashlib,json,os,sys,traceback
from pathlib import Path
P=Path(__file__).resolve().parents[1];C=P.parents[1];sys.path[:0]=[str(P/'scripts'),str(C/'bin')]
import verification_retention
from coupling.scalar_candidate_study import run
from coupling.accuracy_gust_study import write
from coupling.source_replay import validate

def main():
    p=argparse.ArgumentParser();p.add_argument('--admission',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--resume-initialized',action='store_true');a=p.parse_args()
    if a.output.is_symlink() or (a.output.exists() and not a.resume_initialized):raise ValueError('scalar candidate requires a fresh output root; existing content preserved')
    gate=json.loads(a.admission.read_text());assert gate['status']=='admitted'
    for path,digest in gate['inputs'].items():assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==digest,'candidate admission input drift'
    if a.resume_initialized:
        recovery=gate['resume_initialized'];assert a.output.resolve()==Path(recovery['root']);assert hashlib.sha256((a.output/'candidate64/replay.sqlite3').read_bytes()).hexdigest()==recovery['journal_sha256']
        owner=json.loads((a.output/'process.json').read_text());assert owner['pid']==recovery['previous_pid']
        try:os.kill(owner['pid'],0)
        except ProcessLookupError:pass
        else:raise ValueError('prior producer PID still exists; reconcile before resuming')
    workers=json.loads(Path(gate['workers']).read_text());w=(workers['growth_root'],*[workers['binaries'][k]['path'] for k in ('fire','fluid','clock')]);pulse=validate(json.loads(Path(gate['pulse']).read_text()))
    try:run(a.output,w,pulse,gate['baseline'],gate['budget_root'],resume_initialized=a.resume_initialized)
    except BaseException as exc:
        marker=a.output/'process.json'
        if marker.is_file():
            owner=json.loads(marker.read_text())
            if owner.get('pid')==os.getpid() and owner.get('root')==str(a.output.resolve()) and owner.get('script')==str(P/'scripts/coupling/scalar_candidate_study.py'):
                write(a.output/'error.json',{'error':str(exc),'traceback':traceback.format_exc(),'preserved':True,'action':'Inspect exact cause; do not retry or reuse case root'})
        raise
if __name__=='__main__':main()
