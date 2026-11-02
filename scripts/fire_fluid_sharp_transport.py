#!/usr/bin/env python3
"""Sharp-source prescribed-flow benchmark; native solver, independent kernel.
T_t + u T_z = alpha T_zz + 1_[a,b], initially zero; u=.05, a=.125, b=.25.
Free-space reference: source remains remote from endpoints during 3.2 s.
"""
import argparse,hashlib,json,math,subprocess,time
from pathlib import Path
import numpy as np
U=.05;A=.125;B=.25;GATE=.1

def reference(lo,hi,t,conductivity,points=128):
    lo=np.asarray(lo);hi=np.asarray(hi)
    if t==0:return np.zeros(np.broadcast(lo,hi).shape)
    if conductivity==0:
        def primitive(x):
            return (np.maximum(x-A,0)**2-np.maximum(x-A-U*t,0)**2-np.maximum(x-B,0)**2+np.maximum(x-B-U*t,0)**2)/(2*U)
        value=(primitive(hi)-primitive(lo))/(hi-lo)
        return np.where((hi<=A)|(lo>=B+U*t),0,np.maximum(value,0))
    x,w=np.polynomial.legendre.leggauss(points);q=(x+1)/2;age=t*q*q;weights=w*t*q
    width=2*np.sqrt(conductivity/1206*age)
    erfc=np.vectorize(math.erfc)
    def correction(d):
        d=np.abs(d)
        return width/np.sqrt(np.pi)*np.exp(-(d/width)**2)-d*erfc(d/width)
    low=lo[...,None];high=hi[...,None];a=A+U*age;b=B+U*age
    overlap=np.maximum(0,np.minimum(high,b)-np.maximum(low,a))
    integral=overlap+.5*(correction(high-a)-correction(low-a)-correction(high-b)+correction(low-b))
    return np.maximum(0,(integral/(high-low))@weights)

def relative(a,b):
    a=np.asarray(a);b=np.asarray(b);den=float(np.abs(b).sum())
    return float(np.abs(a-b).sum()/den) if den else None

def analyze(lines):
    native=lines[-1];rows=lines[:-1];n=native['n'];L=native['length_m'];k=native['conductivity']
    if not all(math.isfinite(native[key]) for key in ('heat_balance_j','velocity_error','time_s','dt_s')):raise ValueError('nonfinite native metadata')
    if (not native['terminal'] or not native['scalar_rk2'] or native['time_s']!=3.2 or native['steps'] not in (2560,5120)
        or native['memory_live_bytes'] or native['rejected_allocations'] or abs(native['heat_balance_j'])>1e-8 or native['velocity_error']>1e-12 or len(rows)!=129):raise ValueError('native closure')
    times=np.array([r['time_s'] for r in rows]);assert np.max(np.abs(times-np.arange(129)*.025))<1e-12
    profile=np.asarray(native['profile_k']);z=np.arange(n)*L/n
    if profile.shape!=(n,) or not np.isfinite(profile).all() or (profile<0).any():raise ValueError('invalid profile')
    exact=reference(z,z+L/n,3.2,k);check=reference(z,z+L/n,3.2,k,256)
    actual=[np.array([r[key] for r in rows]) for key in ('near_k','tail_k')]
    expected=[np.array([float(reference(lo,hi,t,k)) for t in times]) for lo,hi in [(11/32,12/32),(14/32,15/32)]]
    checked=[np.array([float(reference(lo,hi,t,k,256)) for t in times]) for lo,hi in [(11/32,12/32),(14/32,15/32)]]
    deltas=[relative(exact,check)]+[relative(a,b) for a,b in zip(expected,checked)]
    if any(x is not None and x>1e-6 for x in deltas):raise ValueError(('reference quadrature',deltas))
    for (lo,hi),v in zip([(11/32,12/32),(14/32,15/32)],actual):
        sampled=float(np.sum(profile*np.maximum(0,np.minimum(z+L/n,hi)-np.maximum(z,lo)))/(hi-lo))
        if abs(sampled-v[-1])>1e-12:raise ValueError('independent slab sampling')
    # Early near-slab response is evaluated before the non-diffusive front.
    pre=times<=1.5
    metrics=dict(profile=relative(profile,exact),near_history=relative(actual[0],expected[0]),tail_history=relative(actual[1],expected[1]),prefront_near_history=relative(actual[0][pre],expected[0][pre]))
    gates={key:v is not None and v<=GATE for key,v in metrics.items()}
    # For exactly zero physical reference, retain absolute leakage; no ratio or waiver.
    return dict(n=n,length_m=L,cell_width_m=L/n,dt_s=native['dt_s'],conductivity=k,reconstruction=native['reconstruction'],metrics=metrics,gates=gates,all_gates_pass=all(gates.values()),reference_quadrature_deltas=deltas,
        final_near_k=[float(actual[0][-1]),float(expected[0][-1])],final_tail_k=[float(actual[1][-1]),float(expected[1][-1])],max_prefront_near_k=[float(actual[0][pre].max()),float(expected[0][pre].max())],native=native)

def point_flux(z,t,conductivity,points=256):
    x,w=np.polynomial.legendre.leggauss(points);q=(x+1)/2;age=t*q*q;weights=w*t*q
    alpha=conductivity/1206;width=2*np.sqrt(alpha*age);erfc=np.vectorize(math.erfc)
    left=(z-A-U*age)/width;right=(z-B-U*age)/width
    temperature=float((.5*(erfc(right)-erfc(left)))@weights)
    gradient=float(((np.exp(-left**2)-np.exp(-right**2))/(np.sqrt(np.pi)*width))@weights)
    return U*temperature-alpha*gradient

def run_flux(probe,out):
    out.mkdir(parents=True,exist_ok=False);rows=[];exact=point_flux(.4375,3.2,.026)
    for length,n in [(1,32),(1,64),(1,128),(1,256),(.5,256)]:
        z=np.arange(n)*length/n;v=reference(z,z+length/n,3.2,.026)
        path=out/f'profile-{length}-{n}.bin';v.astype('<f8').tofile(path)
        for mode in ['minmod','mc']:
            p=subprocess.run([str(probe),str(n),str(length),str(path),mode],capture_output=True,text=True,timeout=15)
            if p.returncode:raise RuntimeError(p.stderr)
            d=json.loads(p.stdout)
            if d['memory_live_bytes'] or d['rejected_allocations']:raise ValueError('native flux allocation closure')
            d['exact_flux_k_m_s']=exact;d['signed_relative_error']=d['flux_k_m_s']/exact-1;rows.append(d)
    result=dict(exact_profile_time_s=3.2,measurement_plane_m=.4375,probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),rows=rows)
    (out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))

def run(probe,out,case="matrix"):
    if case not in ("matrix","finest-half-step"):
        raise ValueError("unknown selected case")
    out.mkdir(parents=True,exist_ok=False)
    cases=[(n,1,2560,k,'mc') for k in (0,.026) for n in (32,64,128,256)]
    cases += [(n,1,5120,.026,'mc') for n in (64,128,256)]
    cases += [(n,1,2560,.026,'minmod') for n in (32,64,128,256)]
    cases += [(n,.5,2560,.026,'mc') for n in (128,256)]
    cases += [(256,.5,5120,.026,'mc')]
    if case == 'finest-half-step':
        cases=[(1024,.5,5120,.026,'mc')]
    wall_limit=180 if case == 'finest-half-step' else 45
    (out/'identity.json').write_text(json.dumps(dict(probe=str(probe),sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),selected_case=case,cases=cases,threshold_relative=GATE,scalar_integrator='ssprk2',source_interval_m=[A,B],velocity_m_s=U,wall_limit_per_case_s=wall_limit),indent=2)+'\n')
    report=[]
    for i,c in enumerate(cases):
        start=time.monotonic();command=[str(probe),*map(str,c)]
        try:p=subprocess.run(command,capture_output=True,text=True,timeout=wall_limit)
        except subprocess.TimeoutExpired as error:
            for label,value in [('jsonl',error.stdout),('log',error.stderr)]:
                (out/f'case-{i}.{label}').write_text(value.decode() if isinstance(value,bytes) else value or '')
            (out/f'case-{i}-receipt.json').write_text(json.dumps(dict(command=command,returncode=124,complete=False,wall_s=time.monotonic()-start))+'\n')
            raise
        (out/f'case-{i}-receipt.json').write_text(json.dumps(dict(command=command,returncode=p.returncode,complete=p.returncode==0,wall_s=time.monotonic()-start))+'\n')
        (out/f'case-{i}.jsonl').write_text(p.stdout);(out/f'case-{i}.log').write_text(p.stderr)
        if p.returncode:raise RuntimeError((i,p.returncode,p.stderr))
        row=analyze([json.loads(l) for l in p.stdout.splitlines()]);native=row['native']
        if (native['n'],native['length_m'],native['steps'],native['conductivity'],native['reconstruction'])!=c:raise ValueError('case identity mismatch')
        row['case']=i;row['wall_s']=time.monotonic()-start;report.append(row)
        print(json.dumps({k:v for k,v in row.items() if k!='native'}),flush=True)
    (out/'report.json').write_text(json.dumps(dict(threshold_relative=GATE,cases=report,full_box_qualified=False),indent=2)+'\n')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--probe',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    selection=p.add_mutually_exclusive_group()
    selection.add_argument('--flux-check',action='store_true')
    selection.add_argument('--case',choices=['matrix','finest-half-step'],default='matrix',
        help='Run the original matrix or only the finest column at half timestep (180 s cap).')
    a=p.parse_args()
    if a.flux_check:run_flux(a.probe.resolve(),a.output.resolve())
    else:run(a.probe.resolve(),a.output.resolve(),a.case)
