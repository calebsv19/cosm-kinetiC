#!/usr/bin/env python3
"""Fixed symmetric-reference controls with frozen sources and own-child caps."""
import hashlib
import json
import shutil
import subprocess
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-obstacle/refinement-v2'
SOURCES=('cfd_fem_reference3d_scalar.py','cfd_reference3d_mesh.py','cfd_reference3d_traction.py')


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def run(stem,arguments):
    output=DATA/(stem+'.json');receipt_path=DATA/(stem+'-receipt.json')
    if receipt_path.exists():
        receipt=json.loads(receipt_path.read_text())
        if receipt['returncode']==0:
            assert sha(output)==receipt['output_sha256']
            if 'pressure_sha256' in receipt:assert sha(DATA/(stem+'-pressure.npz'))==receipt['pressure_sha256']
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0
    hashes={name:sha(ROOT/'scripts'/name) for name in SOURCES}
    digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    frozen=DATA/'reference-source'/digest;frozen.mkdir(parents=True,exist_ok=True)
    for name in SOURCES:
        path=frozen/name
        if not path.exists():shutil.copy2(ROOT/'scripts'/name,path)
        assert sha(path)==hashes[name]
    pressure=DATA/(stem+'-pressure.npz')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,
             '--pressure-output',str(pressure),'--output',str(output)]
    started=time.monotonic();peak=0;reason=None
    with (DATA/(stem+'.log')).open('w') as log:
        child=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
        try:
            while child.poll() is None:
                text=subprocess.run(['/bin/ps','-o','rss=','-p',str(child.pid)],capture_output=True,text=True).stdout.strip()
                peak=max(peak,int(text or 0)*1024)
                if peak>1800*1024**2 or time.monotonic()-started>180:
                    reason='reference RSS/time cap';child.terminate();break
                time.sleep(.2)
        finally:
            if child.poll() is None:
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:child.kill();child.wait()
    receipt={'command':command,'returncode':child.returncode,'stop_reason':reason,
        'wall_s':time.monotonic()-started,'peak_observed_rss_bytes':peak,'source_sha256':hashes,
        'mesh_cap':50000,'rss_cap_bytes':1800*1024**2,'wall_cap_s':180}
    if child.returncode==0:
        receipt['output_sha256']=sha(output);receipt['pressure_sha256']=sha(pressure)
    receipt['log_sha256']=sha(DATA/(stem+'.log'))
    receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt),flush=True)
    return child.returncode==0


def main():
    DATA.mkdir(parents=True,exist_ok=True)
    for length in (8,4):
        geometry=['--length',str(length),'--center',str(length/2),'--mirror-mesh']
        run(f'L{length}-edge1',geometry+['--graded','8','--gaps','4','--long','6','--edge-passes','1'])
        for count in (12,14,16,18):
            run(f'L{length}-body{count}',geometry+['--graded',str(count),'--gaps','3','--long','3'])
        run(f'L{length}-body16-long4',geometry+['--graded','16','--gaps','3','--long','4'])
        # body18 empty would be 82944 tets; rejected by the exact same cap.
        run(f'L{length}-empty18',geometry+['--graded','18','--gaps','3','--long','3','--empty'])
        run(f'L{length}-empty16',geometry+['--graded','16','--gaps','3','--long','3','--empty'])
        run(f'L{length}-empty14',geometry+['--graded','14','--gaps','3','--long','3','--empty'])

if __name__=='__main__':main()
