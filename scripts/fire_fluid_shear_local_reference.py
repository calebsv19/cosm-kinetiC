"""Bounded modal local-wall reference check, fixed .5s only; no native reruns."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return [json.loads(x) for x in p.read_text().splitlines()]
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser()
    for key in ['build','output','previous']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve();build.mkdir(parents=True,exist_ok=False)
    source=Path(__file__).resolve().parents[1]/'src/tools/physics_sim_thermal_columns_reference.c'
    flags=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off','-DPHYSICS_SIM_COLUMNS_SHORT_REFERENCE=1']
    commands=[]
    for name,extra in [('reference',[]),('reference-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
        cmd=flags+extra+[str(source),'-lm','-o',str(build/name)];subprocess.run(cmd,check=True);commands.append(cmd)
    digest=sha(source)
    for name,n,modes,steps,exe in [('control',512,128,1600,'reference'),('control-sanitize',512,128,1600,'reference-sanitize'),
        ('normal1024',1024,128,1600,'reference'),('normal2048',2048,128,1600,'reference'),
        ('time',2048,128,3200,'reference'),('modes64',2048,64,3200,'reference')]:
        start=time.monotonic();command=list(map(str,[build/exe,n,modes,steps,10]));path=out/(name+'.jsonl')
        with path.open('x') as f,path.with_suffix('.log').open('x') as err:
            v=subprocess.run(command,stdout=f,stderr=err,timeout=240)
        write(path.with_suffix('.receipt.json'),dict(command=command,wall_s=time.monotonic()-start,returncode=v.returncode,cap_s=240))
        assert v.returncode==0 and sha(source)==digest
        if name=='control':assert read(path)==read(a.previous/'reference-time.jsonl')[:21]
        if name=='control-sanitize':assert path.read_bytes()==(out/'control.jsonl').read_bytes()
        print(name,'reference complete',flush=True)
    write(out/'identity.json',dict(inputs={str(source):digest,str(Path(__file__).resolve()):sha(Path(__file__))},commands=commands,binaries={x:sha(build/x) for x in ['reference','reference-sanitize']}))
    write(out/'native-sha256.json',{p.name:sha(p) for p in out.iterdir() if p.suffix in ['.jsonl','.log']})


if __name__=='__main__':main()
