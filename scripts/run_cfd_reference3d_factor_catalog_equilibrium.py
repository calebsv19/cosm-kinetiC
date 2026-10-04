#!/usr/bin/env python3
"""One bounded snapshot observer with frozen source, caps and immutable receipt."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-factor-catalog/observer-runs'
SOURCES=('cfd_reference3d_cholesky_equilibrium_probe.py','cfd_reference3d_quartic_equilibrium.py','cfd_reference3d_p4.py','cfd_reference3d_p3.py','cfd_reference3d_quartic_pair.py','cfd_reference3d_chunked.py','cfd_reference3d_preconditioner.py','cfd_reference3d_mesh.py','cfd_reference3d_traction.py')


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def run(stem,arguments):
    hashes={name:sha(ROOT/'scripts'/name) for name in SOURCES}
    digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    directory=DATA/digest;directory.mkdir(parents=True,exist_ok=True)
    supervisor_root=DATA.parent/'supervisor-source';supervisor_root.mkdir(exist_ok=True)
    supervisor_digest=sha(Path(__file__));supervisor_path=supervisor_root/(supervisor_digest+'.py')
    if not supervisor_path.exists():shutil.copy2(Path(__file__),supervisor_path)
    assert sha(supervisor_path)==supervisor_digest
    frozen=directory/'source';frozen.mkdir(exist_ok=True)
    for name in SOURCES:
        p=frozen/name
        if not p.exists():shutil.copy2(ROOT/'scripts'/name,p)
        assert sha(p)==hashes[name]
    output=directory/(stem+'.json')
    receipt_path=directory/(stem+'-receipt.json');log_path=directory/(stem+'.log')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,
        '--output',str(output)]
    if receipt_path.exists():
        receipt=json.loads(receipt_path.read_text());assert receipt['command']==command
        for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0 and receipt['stop_reason'] is None
    assert not any(p.exists() for p in (output,log_path))
    start=time.monotonic();peak=0;reason=None
    with log_path.open('x') as log:
        child=subprocess.Popen(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,
            env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='1'))
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
    progress=[]
    for line in log_path.read_text().splitlines():
        try:
            item=json.loads(line)
            if isinstance(item,dict) and ('phase' in item or 'iteration' in item):progress.append(item)
        except json.JSONDecodeError:pass
    result=json.loads(output.read_text()) if output.exists() else {}
    failure=None
    if reason:failure={'kind':'resource_cap','detail':reason}
    elif child.returncode!=0:failure={'kind':'observer_error','returncode':child.returncode}
    elif not result.get('diagnostic_accepted'):failure={'kind':'missing_diagnostic_acceptance'}
    receipt=dict(schema='physics_sim_c3d_quartic_equilibrium_receipt_v1',
        runner_sha256=sha(Path(__file__)),progress=progress,diagnostic_failure=failure,
        command=command,source_sha256=hashes,returncode=child.returncode,stop_reason=reason,
        wall_s=time.monotonic()-start,peak_observed_rss_bytes=peak,mesh_cap=50000,
        rss_cap_bytes=1800*1024**2,wall_cap_s=180,input_solver_iteration_cap=3000,
        environment={'OPENBLAS_NUM_THREADS':'1','OMP_NUM_THREADS':'1','PYTHONDONTWRITEBYTECODE':'1'},
        artifact_sha256={str(p):sha(p) for p in (log_path,output) if p.exists()})
    receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'receipt':str(receipt_path),'returncode':child.returncode,
        'wall_s':receipt['wall_s'],'rss_mib':peak/1024**2,'stop_reason':reason}),flush=True)
    return child.returncode==0 and reason is None and failure is None


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--name',required=True)
    ap.add_argument('arguments',nargs=argparse.REMAINDER)
    a=ap.parse_args()
    if a.arguments and a.arguments[0]=='--':a.arguments=a.arguments[1:]
    if not a.name.replace('-','').isalnum():raise SystemExit('invalid case name')
    raise SystemExit(0 if run(a.name,a.arguments) else 1)
