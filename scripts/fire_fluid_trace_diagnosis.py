#!/usr/bin/env python3
"""Read-only timing/shape diagnosis of hash-bound retained boxed observations.
A fitted delay is a diagnostic, never an acceptance transformation.
"""
import argparse,json
from pathlib import Path
import numpy as np
from coupling.boxed_face_forcing import sha,relative,load

def crossing(t,y,level):
    indices=np.flatnonzero(y>=level)
    if not len(indices):return None
    i=int(indices[0])
    if i==0 or y[i-1]<=0:return float(t[i])
    f=(np.log(level)-np.log(y[i-1]))/(np.log(y[i])-np.log(y[i-1]))
    return float(t[i-1]+f*(t[i]-t[i-1]))

def diagnose(root):
    coarse=load(root,32)['rows']
    fine_path=root/'box-64/trace.jsonl'
    fine=[json.loads(s) for s in fine_path.read_text().splitlines()]
    assert not any(r.get('terminal') for r in fine)
    assert json.loads((root/'box-64/terminal.json').read_text())['returncode']==124
    a=np.array([r['patch_rise_k'] for r in coarse]);b=np.array([r['patch_rise_k'] for r in fine]);t=np.array([r['time_s'] for r in coarse]);u=np.array([r['time_s'] for r in fine])
    assert b.shape==(128,64) and np.isfinite(b).all() and (b>=0).all()
    assert np.max(np.abs(u-np.arange(128)*.025))<1e-12
    x=a.mean(axis=1);y=b.mean(axis=1)
    thresholds=[]
    for v in (1e-12,1e-10,1e-8,1e-6):
        ca=crossing(t,x,v);cb=crossing(u,y,v)
        thresholds.append(dict(rise_k=v,coarse_time_s=ca,fine_time_s=cb,delay_s=cb-ca))
    mask=u>=2.5
    fits=[]
    for delay in np.linspace(0,1.25,1251):
        shifted=np.array([np.interp(u[mask]-delay,t,a[:,j]) for j in range(64)]).T
        fits.append(relative(shifted,b[mask]))
    k=int(np.argmin(fits))
    shapes=[]
    for time in (2.5,2.75,3.,3.175):
        i=round(time/.025);aa=a[i];bb=b[i]
        shapes.append(dict(time_s=time,coarse_mean_rise_k=float(x[i]),fine_mean_rise_k=float(y[i]),
            amplitude_ratio=float(x[i]/y[i]),normalized_shape_l1=relative(aa/aa.sum(),bb/bb.sum())))
    return dict(schema='boxed_trace_diagnosis/v1',inputs={str(p):sha(p) for p in (root/'box-32/trace.jsonl',fine_path)},threshold_relative=.1,
        original_prefix_history_relative_l1=relative(a[:len(b)][1:],b[1:]),crossings=thresholds,shape_samples=shapes,
        diagnostic_best_delay_s=k*.001,diagnostic_delayed_history_relative_l1=fits[k],fit_window_s=[2.5,3.175],
        fit_passes_10_percent=fits[k]<=.1,forcing_qualified=False,qualification_unchanged=True,
        limits=['Incomplete 64 run: no terminal field or native closure.','Fitted delay and amplitude normalization are explanatory only; neither changes acceptance.','Threshold levels are diagnostic observations, not arrival calibration or noise-floor waivers.'])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    result=diagnose(args.root.resolve())
    with args.output.open('x') as f:json.dump(result,f,indent=2);f.write('\n')
    print(json.dumps(result,indent=2))
