#!/usr/bin/env python3
"""Fresh, contained CP2C source-only build; never cleans existing output."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
SOURCES=['src/app/cfd_lowmach_room.c','src/app/cfd_lowmach_room_geometry.c',
         'src/app/cfd_lowmach_room_pressure.c','src/app/cfd_memory.c']
HEADERS=['include/app/cfd_lowmach_room.h','src/app/cfd_lowmach_room_internal.h',
         'include/app/cfd_memory.h','src/tools/physics_sim_lowmach_room_io.h']

def build(output,sanitized=False):
    output=Path(output).resolve();output.mkdir(parents=True,exist_ok=False)
    entries=['src/tools/physics_sim_lowmach_room.c','tests/cfd_lowmach_room_test.c']
    pins={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in SOURCES+HEADERS+entries}
    flags=['-std=c11','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off']
    flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if sanitized else ['-O2']
    commands=[];binaries={}
    for entry,name in zip(entries,('room_worker','native_test')):
        cmd=['clang',*flags,'-Iinclude',*SOURCES,entry,'-lm','-o',str(output/name)]
        run=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
        (output/(name+'-compiler.log')).write_text(run.stdout+run.stderr)
        commands.append(dict(command=cmd,returncode=run.returncode))
        if run.returncode:raise RuntimeError(run.stderr)
        binaries[name]=hashlib.sha256((output/name).read_bytes()).hexdigest()
    if pins!={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in pins}:raise RuntimeError('source drift')
    (output/'build.json').write_text(json.dumps(dict(commands=commands,source_sha256=pins,binaries=binaries,
        compiler=subprocess.check_output(['clang','--version'],text=True)),indent=2)+'\n')
    return output

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sanitized',action='store_true');a=p.parse_args();print(build(a.output,a.sanitized))
