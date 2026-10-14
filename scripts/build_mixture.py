#!/usr/bin/env python3
"""Contained source-only evolving-mixture build; fresh output, no cleanup."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['src/app/cfd_hotgas_caloric.c','src/app/cfd_mixture_thermo.c',
'src/app/cfd_mixture_flux3d.c','src/app/cfd_mixture3d.c','src/app/cfd_cartesian3d.c',
'src/app/cfd_sparse_mg.c','src/app/cfd_memory.c']
HEADERS=['include/app/cfd_hotgas_caloric.h','include/app/cfd_mixture_thermo.h',
'include/app/cfd_mixture3d.h','include/app/cfd_cartesian3d.h',
'include/app/cfd_sparse_mg.h','include/app/cfd_memory.h']
def build(output,sanitized=False,harness=False):
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=False)
    entry='tests/cfd_mixture3d_test.c' if harness else 'src/tools/physics_sim_mixture_worker.c'
    pins={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in SOURCES+HEADERS+[entry]}
    binary=output/('native_test' if harness else 'physics_sim_mixture_worker')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off']
    flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if sanitized else ['-O2']
    command=['clang',*flags,'-Iinclude',*SOURCES,entry,'-lm','-o',str(binary)]
    result=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
    (output/'compiler.log').write_text(result.stdout+result.stderr)
    if pins!={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in pins}:raise ValueError('build source drift')
    receipt=dict(command=command,source_sha256=pins,returncode=result.returncode,
        compiler=subprocess.check_output(['clang','--version'],text=True))
    if result.returncode==0:receipt['binary_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
    (output/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
    if result.returncode:raise RuntimeError(result.stderr)
    return binary
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sanitized',action='store_true');p.add_argument('--harness',action='store_true')
    a=p.parse_args();print(build(a.output,a.sanitized,a.harness))
