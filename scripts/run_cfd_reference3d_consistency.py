#!/usr/bin/env python3
"""Four predeclared reference controls, frozen sources and retained child caps."""
import hashlib
import importlib.util
import re
import json
import os
import shutil
import subprocess
import time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-initial-improvements/traction-v1'
SOURCES=('cfd_fem_reference3d_scalar.py','cfd_reference3d_mesh.py','cfd_reference3d_traction.py','cfd_reference3d_consistency.py')
SOURCES=(*SOURCES,'cfd_run_support.py')


def require(condition,message):
    if not condition:raise ValueError(message)

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def _run_owned(stem,arguments):
    if not isinstance(stem,str) or not re.fullmatch('[A-Za-z0-9][A-Za-z0-9_-]{0,95}',stem):
        raise ValueError('Invalid reference case name')
    bootstrap_path=Path(__file__).with_name('cfd_run_support.py')
    if (any(part.is_symlink() for part in (bootstrap_path,*bootstrap_path.parents))
            or not bootstrap_path.is_file() or bootstrap_path.stat().st_size>1048576):
        raise ValueError('Reference bootstrap helper unadmitted')
    bootstrap_spec=importlib.util.spec_from_file_location('reference_bootstrap',bootstrap_path)
    bootstrap=importlib.util.module_from_spec(bootstrap_spec);bootstrap_spec.loader.exec_module(bootstrap)
    hashes,digest,directory,frozen=bootstrap.reference_prepare(ROOT,DATA,SOURCES,Path(__file__))
    helper_path=frozen/'cfd_run_support.py'
    if helper_path.is_symlink() or sha(helper_path)!=hashes['cfd_run_support.py']:
        raise ValueError('Frozen reference supervisor changed')
    spec=importlib.util.spec_from_file_location('frozen_reference_supervision',helper_path)
    support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
    output=directory/(stem+'.json');receipt_path=directory/(stem+'-receipt.json')
    velocity=directory/(stem+'-velocity.npz');pressure=directory/(stem+'-pressure.npz')
    log_path=directory/(stem+'.log')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,
             '--consistency-diagnostics','--velocity-output',str(velocity),'--pressure-output',str(pressure),'--output',str(output)]
    if receipt_path.exists():
        receipt=support.reference_receipt(receipt_path,directory,command,hashes,expected_paths=(log_path, output, pressure, velocity));require(receipt['command']==command,'Reference source/evidence admission failed')
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0 and receipt.get('stop_reason') is None and receipt.get('diagnostic_failure') is None
    require(not any(p.exists() for p in (output,velocity,pressure,log_path)),'Reference source/evidence admission failed')
    start=time.monotonic();peak=0;reason=None
    child,reason,peak,supervision=support.reference_execute(command,directory,log_path,180,1800*1024**2,
        cwd=ROOT,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
    receipt={'command':command,'source_sha256':hashes,'returncode':child.returncode,'stop_reason':reason,
             'wall_s':time.monotonic()-start,'peak_observed_rss_bytes':peak,
             'mesh_cap':50000,'rss_cap_bytes':1800*1024**2,'wall_cap_s':180,
             'artifact_sha256':support.reference_artifact_hashes(directory,(log_path, output, pressure, velocity))}
    receipt['supervision']=supervision
    support.complete_reference_receipt(receipt,directory,(log_path, output, pressure, velocity))
    support.publish_reference_receipt(receipt_path,receipt)
    print(json.dumps({'receipt':str(receipt_path),'returncode':child.returncode,'wall_s':receipt['wall_s'],'rss_mib':peak/1024**2}),flush=True)
    return child.returncode==0 and reason is None and receipt.get('diagnostic_failure') is None


def main():
    for length,body in ((4,12),(8,8)):
        arguments=['--length',str(length),'--center',str(length/2),'--mirror-mesh',
                   '--graded',str(body),'--body-counts',f'{body},12,12','--gaps','4','--long','6','--physical-long-grid']
        for split in (False,True):
            stem=f'L{length}-'+('split' if split else 'unsplit')
            if not run(stem,arguments+(['--split-first-normal'] if split else [])):
                raise SystemExit(1)

def run(stem,arguments):
    if not isinstance(stem,str) or not re.fullmatch('[A-Za-z0-9][A-Za-z0-9_-]{0,95}',stem):
        raise ValueError('Invalid reference case name')
    bootstrap_path=Path(__file__).with_name('cfd_run_support.py')
    if (any(part.is_symlink() for part in (bootstrap_path,*bootstrap_path.parents))
            or not bootstrap_path.is_file() or bootstrap_path.stat().st_size>1048576):
        raise ValueError('Reference bootstrap helper unadmitted')
    spec=importlib.util.spec_from_file_location('reference_owner_bootstrap',bootstrap_path)
    support=importlib.util.module_from_spec(spec);spec.loader.exec_module(support)
    with support.reference_ownership(ROOT,DATA):
        return _run_owned(stem,arguments)


if __name__=='__main__':main()
