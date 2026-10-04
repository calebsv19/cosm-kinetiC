#!/usr/bin/env python3
"""One bounded reference-method child with frozen source, caps and immutable receipt."""
import argparse
import platform
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-size-selected/runs'
SOURCES=('cfd_reference3d_selected_probe.py', 'cfd_reference3d_pressure_coarse.py', 'cfd_reference3d_exact_prediction.py', 'cfd_reference3d_encoded_operator.py', 'cfd_reference3d_encoded_workspace.py', 'cfd_reference3d_workspace_retirement.py', 'cfd_reference3d_size_selected.py', 'cfd_reference3d_residency_owners.py', 'cfd_reference3d_requested_target.py', 'cfd_reference3d_retained_margin.py', 'cfd_reference3d_fused_observation.py', 'cfd_reference3d_mixed_workspace.py', 'cfd_reference3d_flexible.py', 'cfd_reference3d_factor_catalog.py', 'cfd_reference3d_factor_metadata.py', 'cfd_reference3d_sparse_load.py', 'cfd_reference3d_vector_workspace.py', 'cfd_reference3d_vector_storage.py', 'cfd_reference3d_shared_factor.py', 'cfd_reference3d_allocator_pressure.py', 'cfd_reference3d_triangle_condensed.py', 'cfd_reference3d_triangle.py', 'cfd_reference3d_domain_mesh.py', 'cfd_reference3d_domain_budget.py', 'cfd_reference3d_accelerate.py', 'cfd_reference3d_encoded_storage.c', 'cfd_reference3d_condensed.py', 'cfd_reference3d_condensed_pc.py', 'cfd_reference3d_graded_probe.py', 'cfd_reference3d_p4.py', 'cfd_reference3d_quartic_pair.py', 'cfd_reference3d_quartic_observation.py', 'cfd_reference3d_graded_mesh.py', 'cfd_reference3d_quartic_modes.py', 'cfd_reference3d_chunked.py', 'cfd_reference3d_preconditioner.py', 'cfd_fem_reference3d_solenoidal.py', 'cfd_reference3d_p3.py', 'cfd_reference3d_stokes_pair.py', 'cfd_reference3d_mesh.py', 'cfd_reference3d_traction.py', 'cfd_reference3d_consistency.py')


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
    library=frozen/'factor.dylib'
    build_command=['/usr/bin/clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-dynamiclib',
        str(frozen/'cfd_reference3d_encoded_storage.c'),'-framework','Accelerate','-o',str(library)]
    build_record=frozen/'factor-build.json'
    if not build_record.exists():
        assert not library.exists()
        subprocess.run(build_command,check=True,cwd=ROOT)
        build_record.write_text(json.dumps(dict(command=build_command,source_sha256=hashes['cfd_reference3d_encoded_storage.c'],
            library_sha256=sha(library),platform=platform.platform(),
            compiler=subprocess.run(['/usr/bin/clang','--version'],capture_output=True,text=True,check=True).stdout,
            sdk=subprocess.run(['/usr/bin/xcrun','--show-sdk-path'],capture_output=True,text=True,check=True).stdout.strip()),indent=2)+'\n')
    factor_build=json.loads(build_record.read_text());assert factor_build['command']==build_command
    assert factor_build['source_sha256']==hashes['cfd_reference3d_encoded_storage.c']
    assert factor_build['library_sha256']==sha(library)
    output=directory/(stem+'.json');snapshot=directory/(stem+'.npz')
    receipt_path=directory/(stem+'-receipt.json');log_path=directory/(stem+'.log')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,'--factor-library',str(library),
        '--snapshot',str(snapshot),'--output',str(output)]
    if receipt_path.exists():
        receipt=json.loads(receipt_path.read_text());assert receipt['command']==command
        for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0 and receipt['stop_reason'] is None
    assert not any(p.exists() for p in (output,snapshot,log_path))
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
    if result.get('resource_phase_rejected'):
        failure={'kind':'resource_cap','detail':'owned phase peak/time cap','phase_record':result['resource_phase_rejected']}
    elif reason:failure={'kind':'resource_cap','detail':reason}
    elif child.returncode==2 and result.get('numerical_failure_reasons'):
        failure={'kind':'numerical_acceptance_gate','reasons':result['numerical_failure_reasons'],'iterations':result['iterations'],'final_residual':result['final_residual']}
    elif child.returncode==2 and result.get('numerically_accepted') is False:
        failure={'kind':'diagnostic_iteration_stop' if result['iteration_limit']<3000 else 'linear_iteration_cap',
                 'iterations':result['iterations'],'final_residual':result['final_residual']}
    elif child.returncode!=0:
        final=next((row for row in reversed(progress) if 'iteration' in row),None)
        failure={'kind':'linear_iteration_cap' if final and final['iteration']>=3000 else 'child_error',
                 'last_iteration':final}
    receipt=dict(schema='physics_sim_c3d_mixed_precision_receipt_v1',factor_build=factor_build,
        factor_build_record_sha256=sha(build_record),factor_library_sha256=sha(library),
        runner_sha256=sha(Path(__file__)),progress=progress,diagnostic_failure=failure,
        command=command,source_sha256=hashes,returncode=child.returncode,stop_reason=reason,
        wall_s=time.monotonic()-start,peak_observed_rss_bytes=peak,mesh_cap=50000,
        rss_cap_bytes=1800*1024**2,wall_cap_s=180,linear_iteration_cap=3000,
        environment={'OPENBLAS_NUM_THREADS':'1','OMP_NUM_THREADS':'1','PYTHONDONTWRITEBYTECODE':'1'},
        artifact_sha256={str(p):sha(p) for p in (log_path,output,snapshot,library,build_record) if p.exists()})
    receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps({'receipt':str(receipt_path),'returncode':child.returncode,
        'wall_s':receipt['wall_s'],'rss_mib':peak/1024**2,'stop_reason':reason}),flush=True)
    return child.returncode==0 and reason is None


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--name',required=True)
    ap.add_argument('arguments',nargs=argparse.REMAINDER)
    a=ap.parse_args()
    if a.arguments and a.arguments[0]=='--':a.arguments=a.arguments[1:]
    if not a.name.replace('-','').isalnum():raise SystemExit('invalid case name')
    raise SystemExit(0 if run(a.name,a.arguments) else 1)
