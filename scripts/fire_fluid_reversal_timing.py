"""Fixed-grid 20-ms scalar/exchange timing separation, no live application IO.
The nine native profiles are diagnostic controls, never new five-second defaults.
"""
import argparse,gzip,hashlib,json,subprocess,time
from decimal import Decimal,localcontext
from pathlib import Path
import numpy as np

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def rows(path):return [json.loads(s) for s in path.read_text().splitlines()]
def field(data,key):return np.asarray([r[key] for r in data])
def compare(a,b):
    error=abs(a-b);den=abs(b)
    rel=np.divide(error,den,out=np.where(error==0,0.,np.inf),where=den!=0)
    history=np.divide(np.sum(error,axis=0),np.sum(den,axis=0),out=np.where(np.sum(error,axis=0)==0,0.,np.inf),where=np.sum(den,axis=0)!=0)
    return dict(history=float(np.max(history)),final=float(np.max(rel[-1])),point=float(np.max(rel)),point_failures=int(np.sum(rel>.1)))
def packet_check(path):
    with localcontext() as ctx:
        ctx.prec=70;e0=e1=Decimal(1000);worst=Decimal(0);count=0;last=0;total=Decimal(0)
        for line in path.read_text().splitlines():
            p=line.split();tick,m=int(p[0]),int(p[1]);q=Decimal.from_float(float.fromhex(p[2]));assert tick-last==8*m and q<0
            values=Decimal(0)
            for i in range(m):
                amount,n0,n1=(Decimal.from_float(float.fromhex(v)) for v in p[3+i*3:6+i*3]);values+=amount;total+=amount
                s=e0+e1+amount;d=(e0-e1+amount)/(1+Decimal('.08')/6400);e0,e1=(s+d)/2,(s-d)/2
                worst=max(worst,abs(n0-e0),abs(n1-e1));count+=1
            assert abs(values-q)<=Decimal.from_float(32*np.finfo(float).eps*abs(float(q)))
            last=tick
        assert count==128 and last==1024
        return dict(microsteps=count,max_energy_error_j=float(worst),signed_total_j=float(total))
def run(root,command,name):
    start=time.monotonic()
    with (root/'runs'/f'{name}.jsonl').open('x') as out,(root/'runs'/f'{name}.log').open('x') as err:result=subprocess.run(command,stdout=out,stderr=err,timeout=30)
    write(root/'runs'/f'{name}.receipt.json',dict(command=list(map(str,command)),returncode=result.returncode,seconds=time.monotonic()-start))
    assert result.returncode==0,name

def main():
    p=argparse.ArgumentParser();p.add_argument('evidence',type=Path);p.add_argument('--audit-only',action='store_true');a=p.parse_args()
    root=a.evidence.resolve();repo=Path(__file__).resolve().parents[1];prior=root.parent/'fire-fluid-warm-solid-20261020'
    identity=json.loads((root/'build/identity.json' if (root/'build').exists() else root/'identity.json').read_text())
    assert all(sha(Path(p))==h for p,h in identity['inputs'].items())
    preserved=json.loads((prior/'preserved.json').read_text());assert all(sha(Path(p))==h for p,h in preserved.items())
    if not a.audit_only:
        (root/'runs').mkdir()
        # Rebuild the exact retained short independent CN reference, no changes.
        ref=prior/'front-check/reference.c';assert sha(ref)==json.loads((prior/'front-check/identity.json').read_text())['derived_sha256']
        command=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off',str(ref),'-lm','-o',str(root/'build/front-reference')]
        subprocess.run(command,check=True,timeout=30);write(root/'reference-build.json',dict(command=command,source=str(ref),sha256=sha(ref),binary_sha256=sha(root/'build/front-reference')))
        for n,tps in [(256,102400),(256,204800),(4096,204800),(8192,204800),(8192,409600)]:run(root,[root/'build/front-reference','2',str(n),str(tps)],f'reference-{n}-{tps}')
        for factor in [1,2,4]:
            for exchange in [5,2,1]:
                for binary in ['probe','probe-sanitize']:
                    name=f's{factor}-e{exchange}-{binary}'
                    run(root,[root/'build'/binary,'2',root/'runs'/f'{name}.packets',str(factor),str(exchange)],name)
    fine=rows(root/'runs/reference-8192-409600.jsonl');same=rows(root/'runs/reference-256-204800.jsonl')
    qualification={}
    for label,left,right in [('space','reference-4096-204800','reference-8192-204800'),('fine_time','reference-8192-204800','reference-8192-409600'),('same_grid_time','reference-256-102400','reference-256-204800')]:
        aa,bb=rows(root/'runs'/f'{left}.jsonl'),rows(root/'runs'/f'{right}.jsonl')
        tests={k:compare(field(aa,k)-field(aa,k)[0],field(bb,k)-field(bb,k)[0]) for k in ['profile_k','q_j','gas_mean_k','solid0_k','solid1_k']}
        assert all(max(v['history'],v['final'],v['point'])<=.005 for v in tests.values()),(label,tests)
        qualification[label]=tests
    report=dict(gate=.1,reference_gate=.005,normal_cells=256,horizon_s=.02,reference_qualification=qualification,cases={})
    report['same_grid_physical_errors']={k:compare(field(same,k)-field(same,k)[0],field(fine,k)-field(fine,k)[0]) for k in ['profile_k','q_j','gas_mean_k','solid0_k','solid1_k']}
    for factor in [1,2,4]:
        for exchange in [5,2,1]:
            tag=f's{factor}-e{exchange}';rr=rows(root/'runs'/f'{tag}-probe.jsonl');native=rr[:-1];term=rr[-1]
            assert len(native)==3 and [r['time_s'] for r in native]==[0,.01,.02]
            assert (root/'runs'/f'{tag}-probe.jsonl').read_bytes()==(root/'runs'/f'{tag}-probe-sanitize.jsonl').read_bytes()
            assert (root/'runs'/f'{tag}-probe.packets').read_bytes()==(root/'runs'/f'{tag}-probe-sanitize.packets').read_bytes()
            assert term['normal_cells']==256 and term['scalar_steps']==1024*factor and term['receiver_steps']==128 and term['refusals']==3 and term['memory_live_bytes']==0 and term['velocity_error']==0
            assert np.isfinite(field(native,'profile_k')).all() and np.min(field(native,'profile_k'))>=0 and np.max(field(native,'profile_k'))<=21+1e-12
            assert all(abs(r['source_j']-4.7109375*r['time_s'])<1e-13 for r in native)
            assert term['worst_balance_j']/(2000+4.7109375)<=1e-8
            checks={};matched={}
            for k in ['profile_k','q_j','gas_mean_k','solid0_k','solid1_k']:
                # Initial solid20K cannot mask its signed response error.
                n=field(native,k)-field(native,k)[0];f=field(fine,k)-field(fine,k)[0];s=field(same,k)-field(same,k)[0]
                checks[k]=compare(n,f);matched[k]=compare(n,s)
            checks['rate_w']=compare(np.diff(field(native,'q_j'))/.01,np.diff(field(fine,'q_j'))/.01)
            n=field(native,'profile_k');f=field(fine,'profile_k');relative=np.divide(abs(n-f),abs(f),out=np.zeros_like(f),where=f!=0)
            i,k=map(int,np.unravel_index(np.argmax(relative),relative.shape))
            audit=packet_check(root/'runs'/f'{tag}-probe.packets')
            report['cases'][tag]=dict(scalar_dt_s=1/(51200*factor),exchange_nominal_s=exchange/6400,errors=checks,vs_same_grid=matched,point_pass=bool(all(v['point']<=.1 for v in checks.values())),history_final_pass=bool(all(v['history']<=.1 and v['final']<=.1 for v in checks.values())),worst=dict(time_s=i/100,cell=k,z_m=(k+.5)*.0625/256,native_k=float(n[i,k]),reference_k=float(f[i,k]),relative_error=float(relative[i,k])),native=term,packets=audit)
    # Validate exact old observations and each accepted packet in their prefix.
    baseline=rows(root/'runs/s1-e5-probe.jsonl')[:-1]
    old_manifest=json.loads((prior/'data-manifest.json').read_text())
    old_data={}
    for name in ['runs/reverse-probe.jsonl','runs/reverse-probe.packets']:
        data=gzip.decompress((prior/old_manifest[name]['compressed']).read_bytes());assert hashlib.sha256(data).hexdigest()==old_manifest[name]['sha256'];old_data[name]=data
    oldrows=[json.loads(s) for s in old_data['runs/reverse-probe.jsonl'].decode().splitlines()[:3]];assert baseline==oldrows
    packets=b''.join(line for line in old_data['runs/reverse-probe.packets'].splitlines(keepends=True) if int(line.split()[0])<=1024)
    assert packets==(root/'runs/s1-e5-probe.packets').read_bytes()
    report['old_prefix_exact']=True
    # Retain the fixed baseline sample separately from each run's maximum.
    # The maximum moves from cell6 to cell7 as exchange cadence changes.
    report['tracked_sample']=dict(time_s=.01,cell=6,z_m=6.5*.0625/256,fine_k=fine[1]['profile_k'][6],same_grid_cn_k=same[1]['profile_k'][6],values={})
    for tag in report['cases']:
        native=rows(root/'runs'/f'{tag}-probe.jsonl')[1]['profile_k'][6];reference=same[1]['profile_k'][6]
        report['tracked_sample']['values'][tag]=dict(k=native,error_vs_fine=native/fine[1]['profile_k'][6]-1,error_vs_same_grid=native/reference-1)
    report['investigation_checks_pass']=True
    report['all_cases_physical_point_pass']=all(c['point_pass'] for c in report['cases'].values())
    write(root/'report.json',report)
    # Simple explicit table for readback; diagnostic failures do not crash runner.
    for tag,c in report['cases'].items():print(tag,'worst%',100*c['worst']['relative_error'],'same-grid%',100*c['vs_same_grid']['profile_k']['point'],'pointsfailed',c['errors']['profile_k']['point_failures'])
    print('Exact old prefix, qualified references, sanitizer/packet parity and integrity checks PASS. Physical verdicts are recorded separately.')
    return 0
if __name__=='__main__':raise SystemExit(main())
