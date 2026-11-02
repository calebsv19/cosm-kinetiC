#!/usr/bin/env python3
"""Continuum and exact-in-time grid references for sharp-source diffusion.
T_t=alpha*T_zz+1_[0,a], insulated endpoints, T(z,0)=0; averages, not point samples.
Heat-kernel integration supplies the continuum reference; discrete cosine
eigenmodes supply the exact-in-time grid reference.
"""
import argparse,hashlib,json,subprocess,time,math
from pathlib import Path
import numpy as np
ALPHA=.026/1206

def continuum(n,length,edge,t,points=128):
    # Reflect the source at the insulated lower boundary. The nearest omitted
    # upper image is >60 diffusion lengths away in every admitted case.
    x,w=np.polynomial.legendre.leggauss(points);u=(x+1)/2;weights=w*t*u
    width=2*np.sqrt(ALPHA*t)*u
    z=np.arange(n)[:,None]*length/n;h=length/n
    erf=np.vectorize(math.erf)
    def primitive(x):
        return x*erf(x/width)+width/np.sqrt(np.pi)*np.exp(-(x/width)**2)
    avg=.5*(primitive(z+h+edge)-primitive(z+edge)-primitive(z+h-edge)+primitive(z-edge))/h
    return avg@weights

def semidiscrete(n,length,edge,t):
    m=np.arange(n);basis=np.cos(np.pi*(np.arange(n)[:,None]+.5)*m/n)
    source=(np.arange(n)*length/n<edge).astype(float)
    coeff=2/n*(basis.T@source);coeff[0]/=2
    rate=4*ALPHA*np.sin(m*np.pi/(2*n))**2/(length/n)**2
    gain=np.full(n,t);gain[1:]=-np.expm1(-rate[1:]*t)/rate[1:]
    return basis@(coeff*gain)

def analyze(d):
    n=d['n'];L=d['length_m'];a=d['edge_m'];t=d['time_s'];v=np.array(d['profile_k']);start=round(a*n/L)
    exact=continuum(n,L,a,t);check=continuum(n,L,a,t,256);grid=semidiscrete(n,L,a,t)
    def err(x,y):return float(np.abs(x[start:]-y[start:]).sum()/np.abs(y[start:]).sum())
    ref_delta=err(exact,check)
    if ref_delta>1e-6 or not np.isfinite(v).all() or (v<0).any() or d['memory_live_bytes'] or d['rejected_allocations'] or abs(d['heat_balance_j'])>1e-8:raise ValueError('reference/native closure')
    return dict(n=n,length_m=L,time_s=t,dt_s=d['dt_s'],steps=d['steps'],cell_width_m=L/n,
        outside_profile_error=err(v,exact),time_only_error=err(v,grid),space_only_error=err(grid,exact),
        reference_quadrature_delta=ref_delta,outside_heat_j=1206*L/n*float(v[start:].sum()),
        reference_outside_heat_j=1206*L/n*float(exact[start:].sum()),gate_pass=err(v,exact)<=.1)

def run(probe,out,rk2=False):
    out.mkdir(parents=True,exist_ok=False)
    # Fixed time refinement at 32/64; fixed spatial sweep; resolved early zoom.
    cases=[(n,1,steps,.0025) for n in (32,64) for steps in (2,8,32,128)]
    cases += [(n,1,2560,3.2) for n in (32,64,128,256)]
    cases += [(256,1,5120,3.2)]
    cases += [(n,.0625,128,.0025) for n in (32,64,128,256)]
    cases += [(256,.0625,steps,.0025) for steps in (8,32,512)]
    receipt=dict(probe=str(probe),probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),threshold_relative=.1,cases=cases,scalar_integrator='ssprk2' if rk2 else 'euler')
    (out/'identity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    rows=[]
    for i,c in enumerate(cases):
        start=time.monotonic();p=subprocess.run([str(probe),*map(str,c)]+(["rk2"] if rk2 else []),capture_output=True,text=True,timeout=45)
        (out/f'case-{i}.json').write_text(p.stdout);(out/f'case-{i}.log').write_text(p.stderr)
        if p.returncode:raise RuntimeError((i,p.returncode,p.stderr))
        d=json.loads(p.stdout);r=analyze(d);r['wall_s']=time.monotonic()-start;rows.append(r)
    result=dict(threshold_relative=.1,rows=rows,limits=['Pure diffusion with a steady sharp source, zero velocity, insulated endpoints.','Exact spatially discrete reference separates temporal from spatial error; the errors are not additive.','Zoom tests resolve the initial source edge locally; they do not qualify the full boxed flow.'])
    (out/'report.json').write_text(json.dumps(result,indent=2)+'\n')
    for row in rows:print(json.dumps(row),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--rk2',action='store_true');a=p.parse_args();run(a.probe.resolve(),a.output.resolve(),a.rk2)
