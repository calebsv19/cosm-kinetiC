"""Fixed-input sharp-front ablation with independent periodic heat-kernel oracle."""
import argparse, hashlib, json, math
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_sharp_transport import relative


def reference(n,t,u,k,points=16):
    h=.25/n;lo=np.arange(n)*h
    if t==0:return np.maximum(0,np.minimum(lo+h,1/16)-np.maximum(lo,1/32))/h
    if k==0:
        return sum(np.maximum(0,np.minimum(lo+h,1/16+u*t+j*.25)-np.maximum(lo,1/32+u*t+j*.25))/h for j in range(-2,3))
    x,w=np.polynomial.legendre.leggauss(points);sample=lo[:,None]+(x+1)*h/2
    width=2*math.sqrt(k/1206*t);erfc=np.vectorize(math.erfc);value=np.zeros_like(sample)
    for j in range(-2,3):
        a=(sample-(1/32+u*t+j*.25))/width;b=(sample-(1/16+u*t+j*.25))/width
        # erfc at the nearer tail avoids subtracting two numbers close to 2.
        value+=.5*np.where(b<=0,erfc(-a)-erfc(-b),erfc(b)-erfc(a))
    return value@(w/2)


def analyze(rows,ref,check):
    terminal=rows[-1];v=np.array([r['profile'] for r in rows[:-1]])
    assert v.shape==ref.shape and np.isfinite(v).all() and v.min()>=0
    assert terminal['terminal'] and terminal['balance_k_m']<1e-12
    n=v.shape[1];metrics={'profile_history':relative(v,ref)};qualification={'profile_history':relative(ref,check)}
    arrivals={}
    times=np.arange(129)*.025
    for p in [0,2,3,4,7]:
        sl=slice(p*n//8,(p+1)*n//8);actual=v[:,sl].mean(1);expected=ref[:,sl].mean(1);fine=check[:,sl].mean(1)
        for label,mask in [('history',np.ones(129,dtype=bool)),('early',times<=1.5)]:
            key=f'patch{p}_{label}';metrics[key]=relative(actual[mask],expected[mask]);qualification[key]=relative(expected[mask],fine[mask])
        def crossing(a,threshold):
            indices=np.flatnonzero(a>=threshold)
            if not len(indices):return None
            j=int(indices[0]);return 0.0 if j==0 else float(times[j-1]+.025*(threshold-a[j-1])/(a[j]-a[j-1]))
        arrivals[str(p)]={str(th):dict(native_s=crossing(actual,th),reference_s=crossing(expected,th)) for th in [1e-4,.01]}
    return dict(native=terminal,physical_bounds=dict(nonnegative=terminal['minimum']>=0,maximum_principle=terminal['maximum']<=1+1e-12,peak_excess_k=max(0,terminal['maximum']-1)),metrics=metrics,reference_quadrature_change=qualification,gates={k:(v<=.1 if v is not None and qualification[k] is not None and qualification[k]<.005 else None) for k,v in metrics.items()},arrival_times=arrivals)


def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve()
    cases=[];refs={}
    # Two representative shear-band speeds plus diffusion alone; same fixed
    # spacing and dt. Half-dt repeats isolate time error without changing source.
    for steps in [10240,20480]:
        for u in [0,.003125,.01875]:
            key=(128,u,.026)
            if key not in refs:
                refs[key]=tuple(np.array([reference(128,t*.025,u,.026,points) for t in range(129)]) for points in [16,32])
                assert abs(refs[key][0].sum(1).max()*.25/128-1/32)<1e-13
            for mode in range(4):
                name=f'case-128-{steps}-{u}-{mode}'
                rows=execute([build/'probe',128,steps,u,mode,.026],out/(name+'.jsonl'),15)
                result=analyze(rows,*refs[key]);result['name']=name;cases.append(result)
                print(json.dumps(dict(name=name,metrics=result['metrics'])),flush=True)
    # Pure advection distinguishes donor behavior from diffusion fallback.
    for mode in range(4):
        u=.01875;ref=np.array([reference(128,t*.025,u,0) for t in range(129)])
        rows=execute([build/'probe',128,10240,u,mode,0],out/f'pure-advection-{mode}.jsonl',15)
        result=analyze(rows,ref,ref);result['name']=f'pure-advection-{mode}';cases.append(result)
    execute([build/'probe-sanitize',128,10240,.01875,3,.026],out/'sanitize.jsonl',30)
    inputs={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),Path(__file__).resolve().parents[1]/'src/tools/physics_sim_front_arrival_probe.c',Path(__file__).resolve().parents[1]/'src/app/cfd_scalar_edge_candidate.h']}
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,reference_threshold=.005,cases=cases,inputs=inputs,mode_names=['retained','value fallback','gradient fallback','both fallbacks'],coupled_trajectory_run=False),indent=2)+'\n')

if __name__=='__main__':main()
