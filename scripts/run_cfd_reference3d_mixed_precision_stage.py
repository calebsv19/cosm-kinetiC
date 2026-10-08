#!/usr/bin/env python3
"""One bounded reference-method child with frozen source, caps and immutable receipt."""
import argparse
import importlib.util
import re
import platform
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-mixed-precision/stage-runs'
SOURCES=('cfd_reference3d_mixed_precision_stage_probe.py','cfd_reference3d_mixed_workspace.py','cfd_reference3d_flexible.py','cfd_reference3d_factor_catalog.py','cfd_reference3d_factor_metadata.py','cfd_reference3d_sparse_load.py','cfd_reference3d_vector_workspace.py','cfd_reference3d_vector_storage.py','cfd_reference3d_shared_factor.py','cfd_reference3d_allocator_pressure.py','cfd_reference3d_triangle_condensed.py','cfd_reference3d_triangle.py','cfd_reference3d_domain_mesh.py','cfd_reference3d_domain_budget.py','cfd_reference3d_accelerate.py','cfd_reference3d_mixed_storage.c','cfd_reference3d_condensed.py','cfd_reference3d_condensed_pc.py','cfd_reference3d_graded_probe.py','cfd_reference3d_p4.py','cfd_reference3d_quartic_pair.py','cfd_reference3d_quartic_observation.py','cfd_reference3d_graded_mesh.py','cfd_reference3d_quartic_modes.py','cfd_reference3d_chunked.py','cfd_reference3d_preconditioner.py','cfd_fem_reference3d_solenoidal.py','cfd_reference3d_p3.py','cfd_reference3d_stokes_pair.py','cfd_reference3d_mesh.py','cfd_reference3d_traction.py','cfd_reference3d_consistency.py')
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
    library=frozen/'factor.dylib'
    build_command=['/usr/bin/clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-dynamiclib',
        str(frozen/'cfd_reference3d_mixed_storage.c'),'-framework','Accelerate','-o',str(library)]
    build_record=frozen/'factor-build.json'
    if not build_record.exists():
        require(not library.exists(),'Reference source/evidence admission failed')
        build_compile_result=support.compile_probe(build_command,frozen,'build-compile',60,1024**3,cwd=ROOT)
        support.publish_factor_record(build_record,json.dumps(dict(command=build_command,source_sha256=hashes['cfd_reference3d_mixed_storage.c'],
            library_sha256=sha(library),platform=platform.platform(),
            compiler=support.factor_tool(['/usr/bin/clang','--version'],frozen,'compiler-identity',cwd=ROOT).stdout,
            sdk=support.factor_tool(['/usr/bin/xcrun','--show-sdk-path'],frozen,'sdk-identity',cwd=ROOT).stdout.strip()),indent=2)+'\n',build_compile_result)
    factor_build=support.read_factor_record(build_record);require(factor_build['command']==build_command,'Reference source/evidence admission failed')
    require(factor_build['source_sha256']==hashes['cfd_reference3d_mixed_storage.c'],'Reference source/evidence admission failed')
    require(factor_build['library_sha256']==sha(library),'Reference source/evidence admission failed')
    output=directory/(stem+'.json');snapshot=directory/(stem+'.npz')
    receipt_path=directory/(stem+'-receipt.json');log_path=directory/(stem+'.log')
    command=[str(ROOT/'build/cfd-reference-venv/bin/python'),str(frozen/SOURCES[0]),*arguments,'--factor-library',str(library),
        '--snapshot',str(snapshot),'--output',str(output)]
    if receipt_path.exists():
        receipt=support.reference_receipt(receipt_path,directory,command,hashes,expected_paths=(log_path, output, snapshot, library, build_record));require(receipt['command']==command,'Reference source/evidence admission failed')
        print(json.dumps({'retained':str(receipt_path),'returncode':receipt['returncode']}),flush=True)
        return receipt['returncode']==0 and receipt['stop_reason'] is None and receipt.get('diagnostic_failure') is None
    require(not any(p.exists() for p in (output,snapshot,log_path)),'Reference source/evidence admission failed')
    start=time.monotonic();peak=0;reason=None
    child,reason,peak,supervision=support.reference_execute(command,directory,log_path,180,1800*1024**2,
        cwd=ROOT,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1',OPENBLAS_NUM_THREADS='1',OMP_NUM_THREADS='1'))
    progress=support.reference_progress(log_path)
    result=support.factor_json(output,16777216) if output.exists() else {}
    failure=None
    if result.get('resource_phase_rejected'):
        failure={'kind':'resource_cap','detail':'owned phase peak/time cap','phase_record':result['resource_phase_rejected']}
    elif reason:failure={'kind':'resource_cap' if reason.startswith('reference resource cap:') else 'supervision_hold','detail':reason}
    elif child.returncode==2 and result.get('numerical_failure_reasons'):
        failure={'kind':'numerical_acceptance_gate','reasons':result['numerical_failure_reasons'],'iterations':result['iterations'],'final_residual':result['final_residual']}
    elif child.returncode==2 and result.get('numerically_accepted') is False:
        failure={'kind':'diagnostic_iteration_stop' if result['iteration_limit']<3000 else 'linear_iteration_cap',
                 'iterations':result['iterations'],'final_residual':result['final_residual']}
    elif child.returncode!=0:
        final=next((row for row in reversed(progress) if 'iteration' in row),None)
        failure={'kind':'linear_iteration_cap' if final and final['iteration']>=3000 else 'child_error',
                 'last_iteration':final}
    receipt=dict(schema='physics_sim_c3d_mixed_precision_stage_receipt_v1',factor_build=factor_build,
        factor_build_record_sha256=sha(build_record),factor_library_sha256=sha(library),
        runner_sha256=sha(Path(__file__)),progress=progress,diagnostic_failure=failure,
        command=command,source_sha256=hashes,returncode=child.returncode,stop_reason=reason,
        wall_s=time.monotonic()-start,peak_observed_rss_bytes=peak,mesh_cap=50000,
        rss_cap_bytes=1800*1024**2,wall_cap_s=180,linear_iteration_cap=3000,
        environment={'OPENBLAS_NUM_THREADS':'1','OMP_NUM_THREADS':'1','PYTHONDONTWRITEBYTECODE':'1'},
        artifact_sha256=support.reference_artifact_hashes(directory,(log_path, output, snapshot, library, build_record)))
    receipt['supervision']=supervision
    support.complete_reference_receipt(receipt,directory,(log_path, output, snapshot, library, build_record))
    support.publish_reference_receipt(receipt_path,receipt)
    print(json.dumps({'receipt':str(receipt_path),'returncode':child.returncode,
        'wall_s':receipt['wall_s'],'rss_mib':peak/1024**2,'stop_reason':reason}),flush=True)
    return child.returncode==0 and reason is None and receipt.get('diagnostic_failure') is None


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
