#!/usr/bin/env python3
"""Four predeclared reference controls, frozen sources and retained child caps."""
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-initial-improvements/traction-v1'
SOURCES=('cfd_fem_reference3d_scalar.py','cfd_reference3d_mesh.py','cfd_reference3d_traction.py','cfd_reference3d_consistency.py')


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def run(stem,arguments):
    hashes={name:sha(ROOT/'scripts'/name) for name in SOURCES}
    digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    directory=DATA/digest;directory.mkdir(parents=True,exist_ok=True)
    frozen=directory/'source';frozen.mkdir(exist_ok=True)
    for name in SOURCES:
        p=frozen/name
        if not p.exists():shutil.copy2(ROOT/'scripts'/name,p)
        assert sha(p)==hashes[name]
    output=directory/(stem+'.json');receipt_path=directory/(stem+'-receipt.json')
    velocity=directory/(stem+'-velocity.npz');pressure=directory/(stem+'-pressure.npz')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,
             '--consistency-diagnostics','--velocity-output',str(velocity),'--pressure-output',str(pressure),'--output',str(output)]
    if receipt_path.exists():
        receipt=json.loads(receipt_path.read_text());assert receipt['command']==command
        for p,h in receipt.get('artifact_sha256',{}).items():assert sha(Path(p))==h
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0
    log_path=directory/(stem+'.log');assert not any(p.exists() for p in (output,velocity,pressure,log_path))
    start=time.monotonic();peak=0;reason=None
    with log_path.open('x') as log:
        child=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
                               env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
        try:
            while child.poll() is None:
                text=subprocess.run(['/bin/ps','-o','rss=','-p',str(child.pid)],capture_output=True,text=True).stdout.strip()
                peak=max(peak,int(text or 0)*1024)
                if peak>1800*1024**2 or time.monotonic()-start>180:
                    reason='reference RSS/time cap';child.terminate();break
                time.sleep(.2)
        finally:
            if child.poll() is None:
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:child.kill();child.wait()
    receipt={'command':command,'source_sha256':hashes,'returncode':child.returncode,'stop_reason':reason,
             'wall_s':time.monotonic()-start,'peak_observed_rss_bytes':peak,
             'mesh_cap':50000,'rss_cap_bytes':1800*1024**2,'wall_cap_s':180,
             'artifact_sha256':{str(p):sha(p) for p in (log_path,output,pressure,velocity) if p.exists()}}
    receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'receipt':str(receipt_path),'returncode':child.returncode,'wall_s':receipt['wall_s'],'rss_mib':peak/1024**2}),flush=True)
    return child.returncode==0 and reason is None


def main():
    for length,body in ((4,12),(8,8)):
        arguments=['--length',str(length),'--center',str(length/2),'--mirror-mesh',
                   '--graded',str(body),'--body-counts',f'{body},12,12','--gaps','4','--long','6','--physical-long-grid']
        for split in (False,True):
            stem=f'L{length}-'+('split' if split else 'unsplit')
            if not run(stem,arguments+(['--split-first-normal'] if split else [])):
                raise SystemExit(1)

if __name__=='__main__':main()
