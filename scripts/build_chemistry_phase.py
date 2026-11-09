"""Contained RC7 phase build: requires fresh output; never cleans existing trees."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FILES=['include/physics/chemistry/phase.h','src/physics/chemistry/phase.c','src/physics/chemistry/phase_transport.c','include/physics/chemistry/thermal.h','src/physics/chemistry/thermal.c','src/physics/chemistry/thermal_transport.c','src/physics/chemistry/thermal_flow.c','include/physics/chemistry/reactor.h','include/physics/chemistry/transport.h','src/physics/chemistry/reactor.c','src/physics/chemistry/transport.c','src/physics/chemistry/flow.c','src/app/cfd_cartesian3d.c','src/app/cfd_sparse_mg.c','src/app/cfd_memory.c','include/physics/chemistry/flow.h','include/app/cfd_cartesian3d.h','include/app/cfd_sparse_mg.h','include/app/cfd_memory.h']
def build(out,sanitize=False,water=False):
 files=FILES + (['include/physics/chemistry/water.h','src/physics/chemistry/water.c'] if water else [])
 out=Path(out).resolve();out.mkdir(parents=True,exist_ok=False)
 pins={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files}
 cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-Iinclude','-shared','-fPIC',*[f for f in files if f.endswith('.c')],'-lm','-o',str(out/'phase.dylib')]
 cmd+=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all'] if sanitize else ['-O2']
 p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True);(out/'compiler.log').write_text(p.stdout+p.stderr)
 assert pins=={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in files},'source drift'
 (out/'build.json').write_text(json.dumps(dict(command=cmd,pins=pins,returncode=p.returncode),indent=2));p.check_returncode()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--sanitize',action='store_true');p.add_argument('--water',action='store_true');a=p.parse_args();build(a.output,a.sanitize,a.water)
