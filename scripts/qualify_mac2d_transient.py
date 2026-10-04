#!/usr/bin/env python3
"""Nested-grid self-convergence. Finest numerical field is NOT exact truth."""
import argparse, hashlib, json, math, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',default='build/s3-mac2d-transient');a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
binary=Path('build/cfd_mac2d_transient')
def run(n,steps,amplitude=.01):
    raw=subprocess.check_output([str(binary),str(n),str(steps),str(amplitude)],text=True)
    (out/f'n{n}-steps{steps}-a{amplitude}.txt').write_text(raw)
    rows=raw.splitlines();h=rows[0].split();x=list(map(float,rows[1:]));nn=n*n
    return dict(amplitude=amplitude,n=n,steps=steps,dt=float(h[1]),max_divergence=float(h[2]),max_momentum_residual=float(h[3]),substeps=int(h[4]),u=x[:nn],v=x[nn:2*nn+n],p=x[2*nn+n:])
def differences(c,f):
    n=c['n'];m=f['n'];r=m//n;assert m%n==0
    # Face-aligned restriction averages tangential fine samples; pressure
    # averages each fine cell block. This restriction is second-order.
    u=[sum(f['u'][(j*r+b)*m+i*r] for b in range(r))/r for j in range(n) for i in range(n)]
    v=[sum(f['v'][(j*r)*m+i*r+b] for b in range(r))/r for j in range(n+1) for i in range(n)]
    pressure=[sum(f['p'][(j*r+b)*m+i*r+d] for b in range(r) for d in range(r))/(r*r) for j in range(n) for i in range(n)]
    vel=math.sqrt((sum((x-y)**2 for x,y in zip(c['u'],u))+sum((x-y)**2 for x,y in zip(c['v'],v)))/(n*n))
    pe=math.sqrt(sum((x-y)**2 for x,y in zip(c['p'],pressure))/(n*n))
    return {'velocity_l2_m_s':vel,'pressure_l2_pa':pe}
def sequence(runs):
    diffs=[differences(c,f) for c,f in zip(runs,runs[1:])]
    orders=[{k:math.log(x[k]/y[k],2) for k in x} for x,y in zip(diffs,diffs[1:])]
    return {'runs':[{k:v for k,v in r.items() if k not in ('u','v','p')} for r in runs],'successive_differences':diffs,'observed_orders':orders}
space=[run(n,400) for n in (8,16,32,64)]
time=[run(32,steps) for steps in (50,100,200,400)]
report={'schema':'physics_sim_mac2d_transient_v1','scope':'nonlinear unforced no-slip vortex, T=.1 s; numerical self-convergence, not analytical/experimental acceptance','binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'spatial':sequence(space),'temporal':sequence(time),'temporal_contamination_at_n32':differences(time[-2],time[-1])}
# Gate refinement direction and first-order temporal consistency, not a
# retrospectively selected absolute physical accuracy target.
report['low_amplitude_spatial']=sequence([run(n,400,.001) for n in (8,16,32,64)])
report['fine_grid_time_check']=differences(space[-1],run(64,800))
report['passed']=all(x[k]>0 for x in report['spatial']['observed_orders'] for k in x) and all(.7<x[k]<1.3 for x in report['temporal']['observed_orders'] for k in x) and all(r['substeps']==r['steps'] for r in space+time)
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
raise SystemExit(0 if report['passed'] else 1)
