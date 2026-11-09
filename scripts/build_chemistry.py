"""Build isolated local reactor. New output directory required; no cleanup."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
FILES=['include/physics/chemistry/reactor.h','src/physics/chemistry/reactor.c']
def build(out,sanitize=False):
 out=Path(out).resolve();out.mkdir(parents=True,exist_ok=False)
 pins={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in FILES}
 cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fno-fast-math','-ffp-contract=off','-Iinclude','-shared','-fPIC','src/physics/chemistry/reactor.c','-lm','-o',str(out/'reactor.dylib')]
 cmd+=['-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all'] if sanitize else ['-O2']
 p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
 (out/'compiler.log').write_text(p.stdout+p.stderr)
 assert pins=={f:hashlib.sha256((ROOT/f).read_bytes()).hexdigest() for f in FILES},'source drift'
 (out/'build.json').write_text(json.dumps(dict(command=cmd,pins=pins,returncode=p.returncode),indent=2))
 p.check_returncode()
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();build(a.output,a.sanitize)
