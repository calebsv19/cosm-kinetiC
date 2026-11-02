#!/usr/bin/env python3
"""Independent compact-pulse advection/diffusion reference, exact cell support."""
import argparse,json,math
from pathlib import Path
import numpy as np

def primitive(x,half):
    x=np.clip(x,-half,half);k=np.pi/(2*half)
    return .375*x+np.sin(2*k*x)/(4*k)+np.sin(4*k*x)/(32*k)

def average(lo,hi,time,conductivity,half,points=96):
    lo=np.asarray(lo);hi=np.asarray(hi);shift=.05*time;alpha=conductivity/1206
    if time==0 or alpha==0:
        return np.maximum(0,(primitive(hi-.1875-shift,half)-primitive(lo-.1875-shift,half))/(hi-lo))
    x,w=np.polynomial.legendre.leggauss(points);q=.1875+half*x
    initial=np.cos(np.pi*x/2)**4
    width=math.sqrt(4*alpha*time)
    erf=np.vectorize(math.erf)
    mass=.5*(erf((hi[...,None]-q-shift)/width)-erf((lo[...,None]-q-shift)/width))
    return np.sum(mass*(half*w*initial),axis=-1)/(hi-lo)

def report(root):
    rows=[]
    for path in sorted(root.glob('case-*-run.json'),key=lambda p:int(p.name.split('-')[1])):
        receipt=json.loads(path.read_text());assert receipt['returncode']==0
        obs=[json.loads(x) for x in path.with_name(path.name.replace('-run.json','.jsonl')).read_text().splitlines()]
        d=obs.pop();assert d['terminal'] and d['memory_live_bytes']==0 and len(obs)==129
        n=d['side'];k=d['conductivity'];half=d['half_width_m'];profile=np.array(d['profile_k']);z=np.arange(n)/n
        exact=average(z,z+1/n,3.2,k,half)
        reference_delta=np.abs(exact-average(z,z+1/n,3.2,k,half,160)).sum()/exact.sum()
        assert reference_delta<1e-9
        times=np.array([a['time_s'] for a in obs]);assert np.max(np.abs(times-np.arange(129)*.025))<1e-12
        expected=np.array([float(average(11/32,12/32,t,k,half)) for t in times]);actual=np.array([a['slab_rise_k'] for a in obs])
        mass=profile.sum()/n;expected_mass=.75*half
        centroid=float(np.sum(profile*(z+.5/n))/profile.sum())
        l1=float(np.abs(profile-exact).sum()/exact.sum());history=float(np.abs(actual-expected).sum()/expected.sum())
        before=times<((11/32)-(.1875+half))/.05
        rows.append(dict(case=int(path.name.split('-')[1]),side=n,axis=d['axis'],dt_s=d['dt_s'],conductivity=k,half_width_m=half,
            profile_relative_l1=l1,slab_history_relative_l1=history,profile_pass=l1<=.1,slab_history_pass=history<=.1,
            reference_quadrature_relative=reference_delta,mass_relative_error=abs(mass-expected_mass)/expected_mass,
            centroid_error_m=centroid-(.1875+.05*3.2),maximum_prefront_slab_rise_k=float(actual[before].max()) if before.any() else 0,
            final_slab_rise_k=float(actual[-1]),exact_final_slab_rise_k=float(expected[-1]),wall_s=receipt['wall_s']))
    return dict(schema='prescribed_transport_diagnosis/v1',threshold_relative=.1,rows=rows,
        limits=['No buoyancy, obstacle, source feedback or live crossover qualification.','Diffusive reference is the free-space convolution; packet remains far from boundaries.','Prefront value for diffusion cases includes physical diffusion and is not a pure leakage metric.','Thin columns have at most 4096 cells; no large cubic grid.'])
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=report(a.root)
    with a.output.open('x') as f:json.dump(d,f,indent=2);f.write('\n')
    print(json.dumps(d,indent=2))
