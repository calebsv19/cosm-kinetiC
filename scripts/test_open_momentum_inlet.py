#!/usr/bin/env python3
"""Contained native inlet and ordinary open-atmosphere regression; never packages."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--mode',choices=['strict','headless','sanitize'],default='strict');a=p.parse_args();r=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
names=['cfd_open_atmosphere3d','cfd_open_atmosphere3d_box','cfd_open_atmosphere3d_box_preconditioner','cfd_open_atmosphere3d_lateral','cfd_open_atmosphere3d_wind','cfd_open_atmosphere3d_weather','cfd_open_atmosphere3d_vent','cfd_open_atmosphere3d_vent_pressure','cfd_open_atmosphere3d_vent_balance','cfd_passive3d','cfd_cartesian3d','cfd_sparse_mg','cfd_memory']
sources=[r/f'src/app/{x}.c' for x in names];flags=['-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-ffp-contract=off','-I'+str(r/'include')]+{'strict':['-O0','-g'],'headless':['-O2'],'sanitize':['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']}[a.mode]
commands=[]
for name,source in [('inlet','cfd_open_momentum_inlet_test.c'),('ordinary','cfd_open_atmosphere3d_test.c')]:
 cmd=['/usr/bin/clang',*flags,*map(str,sources),str(r/'tests'/source),'-lm','-o',str(out/name)];commands.append(cmd)
 with (out/(name+'-build.log')).open('w') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True)
 with (out/(name+'-run.log')).open('w') as log:subprocess.run([str(out/name)],stdout=log,stderr=subprocess.STDOUT,check=True)
inputs=sources+[r/'tests/cfd_open_momentum_inlet_test.c',r/'tests/cfd_open_atmosphere3d_test.c',Path(__file__)]+list((r/'include/app').glob('*.h'))+list((r/'src/app').glob('cfd_open_atmosphere3d*.inc'))
(out/'identity.json').write_text(json.dumps({'mode':a.mode,'commands':commands,'inputs':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs}},indent=2)+'\n');print('PASS inlet and ordinary native regression',a.mode)
