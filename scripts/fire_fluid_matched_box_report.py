#!/usr/bin/env python3
"""Strict terminal analysis of same-operator, prescribed-flow boxed transport.
This does not qualify independent fine-grid momentum or reciprocal wall heating.
"""
import argparse,json
from pathlib import Path
import numpy as np
from coupling.boxed_face_forcing import relative,patches,response,sha

def report(root):
    root=Path(root);identity=json.loads((root/'identity.json').read_text());terminal=json.loads((root/'terminal.json').read_text())
    if terminal['returncode']!=0 or terminal['hard_kill'] or terminal['soft_stop']:
        raise ValueError('complete terminal run required')
    if (identity['driver_grid']!=32 or identity['heat_grids']!=[32,64] or identity['operator'] not in ('mc','minmod')
        or identity['flow_dt_s']!=.0025 or identity['scalar_dt_s']!=.00125 or identity['duration_s']!=3.2 or identity['gate_relative']!=.1):
        raise ValueError('fixed comparison contract required')
    for path,digest in identity['sha256'].items():
        if sha(path)!=digest:raise ValueError('input identity drift')
    if sha(root/'fields.bin')!=terminal['fields_sha256']:raise ValueError('field identity drift')
    rows=[json.loads(line) for line in (root/'trace.jsonl').read_text().splitlines()];native=rows.pop()
    if (not native['terminal'] or not native['complete'] or native['steps']!=1280 or native['time_s']!=3.2 or native['operator']!=identity['operator']
        or not native['control_bitwise_equal'] or native['memory_live_bytes'] or native['rejected_allocations']):raise ValueError('native terminal failed')
    for key,limit in [('max_fine_divergence',1e-8),('max_fine_courant',.4),('max_source_restriction_error_j',1e-14),('coarse_balance_j',1e-9),('fine_balance_j',1e-9)]:
        if not np.isfinite(native[key]) or abs(native[key])>limit:raise ValueError('native closure failed: '+key)
    fields=np.fromfile(root/'fields.bin',dtype='<f8')
    if fields.shape!=(32**3+64**3,) or not np.isfinite(fields).all() or (fields<0).any():raise ValueError('invalid final fields')
    energy=[fields[:32**3].reshape(32,32,32),fields[32**3:].reshape(64,64,64)]
    cases=[]
    for n,e in zip((32,64),energy):
        selected=[r for r in rows if r.get('grid')==n]
        if len(selected)!=641:raise ValueError('complete matched cadence required')
        for i,row in enumerate(selected):
            if abs(row['time_s']-i*.005)>1e-12:raise ValueError('time mismatch')
            for key,count in [('patch_rise_k',64),('bands_j',32),('column_j',32)]:
                a=np.asarray(row[key])
                if a.shape!=(count,) or not np.isfinite(a).all() or (a<0).any():raise ValueError('invalid observation')
                if i==0 and np.count_nonzero(a):raise ValueError('zero initial condition required')
            row['support_mean_rise_k']=float(np.mean(row['patch_rise_k']))
        if not np.allclose(patches(e,n),selected[-1]['patch_rise_k'],rtol=1e-12,atol=0):raise ValueError('independent face readback failed')
        r=n//32
        if np.count_nonzero(e[12*r:20*r,12*r:20*r,12*r:20*r]):raise ValueError('heat in solid')
        b=e.sum(axis=(1,2)).reshape(32,r).sum(axis=1)
        c=e[:,12*r:20*r,12*r:20*r].sum(axis=(1,2)).reshape(32,r).sum(axis=1)
        if not np.allclose(b,selected[-1]['bands_j'],rtol=1e-12,atol=0) or not np.allclose(c,selected[-1]['column_j'],rtol=1e-12,atol=0):raise ValueError('independent band readback failed')
        cases.append(selected)
    if len(rows)!=1282:raise ValueError('unexpected rows')
    coarse=energy[0];fine=energy[1].reshape(32,2,32,2,32,2).sum(axis=(1,3,5))
    hist=[np.asarray([r['patch_rise_k'] for r in case]) for case in cases]
    metrics=dict(local_heat_relative_l1=relative(coarse[:12,8:24,8:24],fine[:12,8:24,8:24]),
                 face_history_relative_l1=relative(*hist),face_final_relative_l1=relative(hist[0][-1],hist[1][-1]))
    responses=[{str(dt):response(case,dt) for dt in (.025,.01,.005)} for case in cases]
    metrics['transfer_demand_relative']=relative([responses[0]['0.005']['gas_transfer_demand_j']],[responses[1]['0.005']['gas_transfer_demand_j']])
    cadence=[relative([r['0.01']['gas_transfer_demand_j']],[r['0.005']['gas_transfer_demand_j']]) for r in responses]
    shape=relative(hist[0][-1]/hist[0][-1].sum(),hist[1][-1]/hist[1][-1].sum()) if all(h[-1].sum()>0 for h in hist) else None
    bands=[]
    for k in range(12):
        a=[r['bands_j'][k] for r in cases[0]];b=[r['bands_j'][k] for r in cases[1]]
        bands.append(dict(z_lower_m=k/32,z_upper_m=(k+1)/32,final_j=[a[-1],b[-1]],history_relative=relative(a,b),final_relative=relative([a[-1]],[b[-1]])))
    timeline=[]
    for i in [1,2,10,20,100,200,400,600,640]:
        a=np.asarray(cases[0][i]['bands_j']);b=np.asarray(cases[1][i]['bands_j'])
        timeline.append(dict(time_s=i*.005,source_band_relative=relative(a[:4],b[:4]),above_source_heat_j=[float(a[4:12].sum()),float(b[4:12].sum())],above_source_relative=relative(a[4:12],b[4:12]),face_mean_rise_k=[float(h[i].mean()) for h in hist]))
    gates={k:v is not None and v<=.1 for k,v in metrics.items()};gates['cadence_01_vs_005']=all(v is not None and v<=.1 for v in cadence)
    return dict(threshold_relative=.1,matched_transport_passed=all(gates.values()),full_coupled_qualification=False,metrics=metrics,gates=gates,cadence_relative=cadence,final_normalized_patch_shape_relative=shape,final_face_mean_rise_k=[float(h[-1].mean()) for h in hist],bands=bands,timeline=timeline,responses=responses,native=native,wall_s=terminal['wall_s'])

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('root',type=Path);args=parser.parse_args()
    print(json.dumps(report(args.root),indent=2,allow_nan=False))
