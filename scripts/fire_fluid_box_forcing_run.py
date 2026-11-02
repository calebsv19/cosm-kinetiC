#!/usr/bin/env python3
"""Bounded local box probe supervisor; retain accepted state on a soft timeout.
Fields are diagnostic snapshots, not resumable joint-host checkpoints.
"""
import argparse,hashlib,json,subprocess,time
from pathlib import Path

def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def run(probe,source,out,n,seconds,mc):
    if n not in (32,64) or not 1<=seconds<=1200:raise ValueError('bounded grid/wall budget required')
    if source.stat().st_size!=32*2*n**3*8:raise ValueError('source carrier size mismatch')
    out.mkdir(parents=True,exist_ok=False)
    command=[str(probe),str(n),str(source),str(out/'fields.bin'),'2']+(['mc'] if mc else [])
    identity=dict(command=command,source_sha256=sha(source),probe_sha256=sha(probe),grid=n,flow_dt_s=.0025,scalar_dt_s=.00125,duration_s=3.2,
        support_lower_m=[.375,.375,.34375],support_upper_m=[.625,.625,.375],heat_gate_relative=.1,scalar_reconstruction='mc' if mc else 'minmod',soft_wall_limit_s=seconds)
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n');start=time.monotonic();soft=False;hard=False
    with (out/'trace.jsonl').open('x') as stdout,(out/'native.log').open('x') as stderr:
        process=subprocess.Popen(command,stdout=stdout,stderr=stderr)
        try:code=process.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            soft=True;process.terminate()
            try:code=process.wait(timeout=30)
            except subprocess.TimeoutExpired:hard=True;process.kill();code=process.wait()
    terminal=dict(returncode=code,wall_s=time.monotonic()-start,soft_stop_requested=soft,hard_kill_required=hard,complete=code==0)
    if (out/'fields.bin').exists():terminal['fields_sha256']=sha(out/'fields.bin')
    (out/'terminal.json').write_text(json.dumps(terminal,indent=2)+'\n')
    return terminal

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--grid',type=int,choices=(32,64),required=True);p.add_argument('--seconds',type=int,required=True);p.add_argument('--mc',action='store_true');a=p.parse_args()
    result=run(a.probe.resolve(),a.source.resolve(),a.output.resolve(),a.grid,a.seconds,a.mc);print(json.dumps(result));raise SystemExit(result['returncode'])
