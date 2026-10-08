#!/usr/bin/env python3
"""One bounded snapshot observer with frozen source, caps and immutable receipt."""
import argparse
import importlib.util
import re
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-restart/observer-runs'
SOURCES=('cfd_reference3d_signed_force_probe.py','cfd_reference3d_signed_equilibrium.py','cfd_reference3d_force_attribution.py','cfd_reference3d_quartic_equilibrium.py','cfd_reference3d_p4.py','cfd_reference3d_p3.py','cfd_reference3d_quartic_pair.py','cfd_reference3d_chunked.py','cfd_reference3d_preconditioner.py','cfd_reference3d_mesh.py','cfd_reference3d_traction.py')
SOURCES=(*SOURCES,'cfd_run_support.py')


def require(condition,message):
    if not condition:raise ValueError(message)

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


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
    output=directory/(stem+'.json');attribution=directory/(stem+'-signed.npz')
    receipt_path=directory/(stem+'-receipt.json');log_path=directory/(stem+'.log')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,
        '--output',str(output),'--attribution',str(attribution)]
    if receipt_path.exists():
        receipt=support.reference_receipt(receipt_path,directory,command,hashes,expected_paths=(log_path, output, attribution));require(receipt['command']==command,'Reference source/evidence admission failed')
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0 and receipt['stop_reason'] is None and receipt.get('diagnostic_failure') is None
    require(not any(p.exists() for p in (output,log_path,attribution)),'Reference source/evidence admission failed')
    start=time.monotonic();peak=0;reason=None
    child,reason,peak,supervision=support.reference_execute(command,directory,log_path,180,1800*1024**2,
        cwd=ROOT,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='1'))
    progress=support.reference_progress(log_path)
    result=support.factor_json(output,16777216) if output.exists() else {}
    failure=None
    if reason:failure={'kind':'resource_cap' if reason.startswith('reference resource cap:') else 'supervision_hold','detail':reason}
    elif child.returncode!=0:failure={'kind':'observer_error','returncode':child.returncode}
    elif not result.get('diagnostic_accepted'):failure={'kind':'missing_diagnostic_acceptance'}
    receipt=dict(schema='physics_sim_c3d_quartic_equilibrium_receipt_v1',
        runner_sha256=sha(Path(__file__)),progress=progress,diagnostic_failure=failure,
        command=command,source_sha256=hashes,returncode=child.returncode,stop_reason=reason,
        wall_s=time.monotonic()-start,peak_observed_rss_bytes=peak,mesh_cap=50000,
        rss_cap_bytes=1800*1024**2,wall_cap_s=180,input_solver_iteration_cap=3000,
        environment={'OPENBLAS_NUM_THREADS':'1','OMP_NUM_THREADS':'1','PYTHONDONTWRITEBYTECODE':'1'},
        artifact_sha256=support.reference_artifact_hashes(directory,(log_path, output, attribution)))
    receipt['supervision']=supervision
    support.complete_reference_receipt(receipt,directory,(log_path, output, attribution))
    support.publish_reference_receipt(receipt_path,receipt)
    print(json.dumps({'receipt':str(receipt_path),'returncode':child.returncode,
        'wall_s':receipt['wall_s'],'rss_mib':peak/1024**2,'stop_reason':reason}),flush=True)
    return child.returncode==0 and reason is None and failure is None and receipt.get('diagnostic_failure') is None


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


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--name',required=True)
    ap.add_argument('arguments',nargs=argparse.REMAINDER)
    a=ap.parse_args()
    if a.arguments and a.arguments[0]=='--':a.arguments=a.arguments[1:]
    if not a.name.replace('-','').isalnum():raise SystemExit('invalid case name')
    raise SystemExit(0 if run(a.name,a.arguments) else 1)
