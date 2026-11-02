"""Matched wall-normal mean: separate spatial, scalar, receiver and cadence errors."""
import argparse
from decimal import Decimal,localcontext
import hashlib,json,subprocess,time
from pathlib import Path
import numpy as np
from fire_fluid_faint_kernel import arrival


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def read(p):return [json.loads(x) for x in p.read_text().splitlines()]
def relative(a,b):return float(np.abs(np.array(a)-b).sum()/np.abs(b).sum())


def solid(path,tps):
    with localcontext() as c:
        c.prec=80;D=Decimal.from_float;e=[Decimal(0),Decimal(0)];error=[Decimal(0),Decimal(0)];den=error.copy()
        ledger=Decimal(0);qsum=Decimal(0);windows=0
        for line in path.read_text().splitlines():
            f=line.split();tick,n=map(int,f[:2]);q,a,b,*amounts=map(float.fromhex,f[2:]);assert len(amounts)==n
            assert abs(sum(amounts)-q)<=64*np.spacing(q)
            for amount in amounts:
                total=e[0]+e[1]+D(amount);diff=(e[0]-e[1]+D(amount))/(1+Decimal('.08')/tps)
                e=[(total+diff)/2,(total-diff)/2]
            for d,v in enumerate([a,b]):error[d]+=abs(D(v)-e[d]);den[d]+=abs(e[d])
            qsum+=D(q);ledger=max(ledger,abs(D(a)+D(b)-qsum));windows+=1
        ratios=[float(x/y) for x,y in zip(error,den)];assert max(ratios)<1e-10
        return dict(history_error=ratios,windows=windows,ledger_residual_j=float(ledger))


def main():
    p=argparse.ArgumentParser()
    for key in ['build','output','previous']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--reuse-native',action='store_true');a=p.parse_args()
    root=Path(__file__).resolve().parents[1];out=a.output.resolve();out.mkdir(parents=True,exist_ok=a.reuse_native);build=a.build.resolve()
    if a.reuse_native:identity=json.loads((out/'identity.json').read_text())
    else:
        identity=json.loads((build/'identity.json').read_text());write(out/'identity.json',identity)
    assert identity['core_version']=='0.3.3'
    assert all(sha(Path(k))==v for k,v in identity['inputs'].items())
    def run(exe,args,path):
        if a.reuse_native:return read(path)
        command=list(map(str,[exe,*args]));start=time.monotonic()
        with path.open('x') as f,path.with_suffix('.log').open('x') as err:
            v=subprocess.run(command,stdout=f,stderr=err,timeout=120)
        write(path.with_suffix('.receipt.json'),dict(command=command,returncode=v.returncode,wall_s=time.monotonic()-start))
        assert v.returncode==0,path
        return read(path)
    if not a.reuse_native:
        for name in ['columns-test','face-test','support-test','slab-test']:
            v=subprocess.run([build/name],text=True,capture_output=True,check=True,timeout=30);(out/(name+'.log')).write_text(v.stdout+v.stderr)
        cmd=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off',str(root/'src/tools/physics_sim_faint_reference.c'),'-lm','-o',str(build/'mean-reference')]
        subprocess.run(cmd,check=True);write(out/'reference-build.json',dict(command=cmd,source_sha256=sha(root/'src/tools/physics_sim_faint_reference.c'),binary_sha256=sha(build/'mean-reference')))
    previous=read(a.previous/'runs/repaired-full.jsonl')[:-1]
    prior_identity=json.loads((a.previous/'runs/repaired-identity.json').read_text())
    fixed={k:v for k,v in identity['inputs'].items() if k in prior_identity['inputs'] and Path(k).name!='build_thermal_transport.py'}
    assert all(v==prior_identity['inputs'][k] for k,v in fixed.items()),'unchanged helper/owner drift'
    ref=[arrival(i*.025,256) for i in range(21)];low=np.array([r['lower_q_j'] for r in ref]);high=np.array([r['upper_q_j'] for r in ref])
    def wall_error(actual):
        x=np.array(actual);lo=float(np.maximum(np.maximum(low-x,x-high),0).sum()/high.sum());hi=float(np.maximum(abs(x-low),abs(x-high)).sum()/low.sum())
        return dict(error_interval=[lo,hi],gate=True if hi<=.1 else False if lo>.1 else None)
    rows={};cases={}
    specifications=[('n128',128,2,160,1600),('n128-half-scalar',128,1,160,1600),
        ('n256',256,2,160,1600),('n256-half-scalar',256,1,160,1600),
        ('n256-half-receiver',256,1,160,3200),('n256-half-exchange',256,1,80,3200),
        ('n256-quarter-receiver',256,1,80,6400),('n256-quarter-exchange',256,1,40,6400)]
    for name,n,dt,window,tps in specifications:
        strict=run(build/'probe',[n,dt,window,tps,25600,out/(name+'.packets')],out/(name+'.jsonl'))
        san=run(build/'probe-sanitize',[n,dt,window,tps,25600,out/(name+'-sanitize.packets')],out/(name+'-sanitize.jsonl'))
        assert strict==san and (out/(name+'.packets')).read_bytes()==(out/(name+'-sanitize.packets')).read_bytes()
        terminal=strict.pop();assert terminal['memory_live_bytes']==0 and terminal['worst_balance_j']<1e-10
        assert terminal['minimum_k']>=0 and terminal['maximum_k']<=.05+1e-10
        assert [r['tick'] for r in strict]==list(range(0,25601,1280))
        assert abs(strict[-1]['source_j']-.05888671875)<1e-10
        if name=='n128':
            parity=relative([r['q_j'] for r in strict],[r['q_j'] for r in previous]);assert parity<1e-8
        q=[r['q_j'] for r in strict]
        cases[name]=dict(normal=n,scalar_s=dt/51200,exchange_s=window/51200,receiver_s=1/tps,
            **wall_error(q),solid_given_q=solid(out/(name+'.packets'),tps),terminal=terminal,
            points={str(i*.025):dict(native_q_j=q[i],physical_interval_j=[low[i],high[i]],relative_excess_lower=max(low[i]-q[i],q[i]-high[i],0)/high[i]) for i in [1,4,16,20]})
        rows[name]=strict;print(name,cases[name]['error_interval'],flush=True)
    pairs=[('space-base','n128','n256'),('space-half-scalar','n128-half-scalar','n256-half-scalar'),
        ('scalar128','n128','n128-half-scalar'),('scalar256','n256','n256-half-scalar'),
        ('receiver-half','n256-half-scalar','n256-half-receiver'),('exchange-half','n256-half-receiver','n256-half-exchange'),
        ('receiver-quarter','n256-half-exchange','n256-quarter-receiver'),('exchange-quarter','n256-quarter-receiver','n256-quarter-exchange')]
    changes={name:relative([r['q_j'] for r in rows[lo]],[r['q_j'] for r in rows[hi]]) for name,lo,hi in pairs}
    same_normal={};cn={}
    for name,n,steps in [('cn128',128,64000),('cn256',256,64000),('cn256-half-time',256,128000)]:
        v=run(build/'mean-reference',[n,1,steps,.5,0,.25,-3,0,steps//20],out/(name+'.jsonl'));cn[name]=v
        same_normal[name]=wall_error([r['q_j'] for r in v])
    cn_time=relative([r['q_j'] for r in cn['cn256']],[r['q_j'] for r in cn['cn256-half-time']]);assert cn_time<.005
    native_same={name:relative([r['q_j'] for r in rows[name]],[r['q_j'] for r in cn[ref]]) for name,ref in [('n128','cn128'),('n256','cn256-half-time'),('n256-quarter-exchange','cn256-half-time')]}
    point_attribution={}
    for i in [1,4,16,20]:
        fine=cn['cn256-half-time'][i]['q_j'];coarse=cn['cn256'][i]['q_j']
        point_attribution[str(i*.025)]=dict(cn256_q_j=fine,time_refinement=abs(coarse/fine-1),
            physical_interval_j=[low[i],high[i]],spatial_relative_excess_lower=max(low[i]-fine,fine-high[i],0)/high[i])
    source_paths=[Path(__file__),root/'scripts/fire_fluid_faint_kernel.py',root/'src/tools/physics_sim_faint_reference.c',a.previous/'runs/repaired-full.jsonl']
    inputs={str(p.resolve()):sha(p) for p in source_paths}
    result=dict(gate=.1,interval_s=[0,.5],physical_reference='positive Robin kernel plus feedback/top-image bounds',baseline_full_shear_parity=parity,
        cases=cases,isolated_changes=changes,independent_same_normal=same_normal,cn_time_change=cn_time,native_vs_same_normal=native_same,point_attribution=point_attribution,
        unchanged_owner_and_helpers=fixed,feedback=False,full_shear_requalified=False,inputs=inputs)
    write(out/'report.json',result)
    if a.reuse_native:
        assert all(sha(out/k)==v for k,v in json.loads((out/'native-sha256.json').read_text()).items())
    else:write(out/'native-sha256.json',{p.name:sha(p) for p in out.iterdir() if p.suffix in ['.jsonl','.packets','.log']})


if __name__=='__main__':main()
