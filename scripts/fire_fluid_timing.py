"""Run the finite serial timing comparison in fresh, contained case roots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time
import traceback
from coupling.live_gust import write
from coupling.timing_cases import CASES,run_case


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('workers','coarse-pulse','tick-pulse','output','budget-root'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--resume-replay-prefix',type=Path,default=None)
    parser.add_argument('--case',choices=CASES,default=None)
    a=parser.parse_args()
    selected_cases=CASES if a.case is None else (a.case,)
    root=Path(__file__).resolve().parents[1]
    workspace=root.parent.parent if root.parent.name=='_worktrees' else root.parent
    helper=workspace/'bin/verification_retention.py'
    if not helper.is_file():
        raise ValueError('timing comparison requires CodeWork consumer-check helper before native admission')
    sys.path.insert(0,str(helper.parent))
    import verification_retention
    assert Path(verification_retention.__file__).resolve()==helper
    a.output.mkdir(parents=True,exist_ok=False)
    w=json.loads(a.workers.read_text())
    workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')])
    for key in ('fire','fluid','clock'):
        p=Path(w['binaries'][key]['path'])
        assert hashlib.sha256(p.read_bytes()).hexdigest()==w['binaries'][key]['sha256']
    coarse=json.loads(a.coarse_pulse.read_text());tick=json.loads(a.tick_pulse.read_text())
    write(a.output/'process.json',{'pid':os.getpid(),'script':str(Path(__file__).resolve()),
                                 'started':time.time(),'cases':selected_cases})
    def progress(name,done,total,phase):
        write(a.output/'progress.json',{'status':'running','case':name,'completed_windows':done,
                                       'windows':total,'phase':phase})
        if phase=='published':print(name,done,total,flush=True)
    results={}
    for name in selected_cases:
        try:
            results[name]=run_case(workers,a.output/name,name,coarse,tick,
                                   budget_root=a.budget_root,progress=progress,
                                   replay_prefix=a.resume_replay_prefix if name=='host100' else None)
        except Exception as error:
            results[name]={'status':'failed-preserved','error':str(error),
                           'traceback':traceback.format_exc()}
            write(a.output/'results.json',results)
            write(a.output/'error.json',{'case':name,**results[name]})
            raise
        write(a.output/'results.json',results)
    write(a.output/'terminal.json',{'status':'numerical-complete-pending-audit','cases':results})
    write(a.output/'progress.json',{'status':'terminal-pending-audit','completed':len(results)})


if __name__=='__main__':main()
