"""Impermeable planar wall: matched normal resolution, contact, and time checks."""
import argparse,json,hashlib
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute,metrics
from fire_fluid_sharp_transport import relative

def insulated_reference(lo,hi,t):
    # Independent cosine-series solution for insulated finite-domain diffusion
    # with constant unit source on [L/4,L/2]. No native/reference-C solver calls.
    L=.0625;alpha=.026/1206
    k=np.arange(1,16385)*np.pi/L
    source=2/L*(np.sin(k*L/2)-np.sin(k*L/4))/k
    mean=(np.sin(k*hi)-np.sin(k*lo))/(k*(hi-lo))
    return .25*t+float(np.sum(source*mean*(-np.expm1(-alpha*k*k*t))/(alpha*k*k)))

def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=json.loads((build/'identity.json').read_text());assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items()),'source drift'
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    refs={}
    for name,n,steps,h in [('coarse',1024,10240,10),('fine',2048,10240,10),('time',2048,20480,10),('zero',2048,20480,0)]:
        refs[name]=execute([build/'reference',n,steps,h],out/f'reference-{name}.jsonl',30)
    spatial=metrics(refs['coarse'],refs['fine']);temporal=metrics(refs['fine'],refs['time'])
    assert all(v<.001 for v in list(spatial.values())+list(temporal.values())),(spatial,temporal)
    zero={}
    for k,lo,hi in [('near_k',0,1/128),('tail_k',1/128,1/64)]:
        exact=[insulated_reference(lo,hi,i*.025) for i in range(129)]
        zero[k]=relative([x[k] for x in refs['zero']],exact)
        if k=='near_k':zero['early_near']=relative([x[k] for x in refs['zero'][:61]],exact[:61])
    assert all(v<.001 for v in zero.values()),zero
    assert all(x['q_j']==x['solid0_k']==x['solid1_k']==0 for x in refs['zero'])
    (out/'reference-checks.json').write_text(json.dumps(dict(spatial=spatial,temporal=temporal,insulated_cosine=zero),indent=2)+'\n')
    results=[];histories=[]
    for n,window in [(128,10),(256,10),(256,5)]:
        lines=execute([build/'probe',n,window,5120,10],out/f'case-{n}-{window}.jsonl',90);native=lines.pop()
        assert native['terminal'] and native['steps']==5120 and native['memory_live_bytes']==0 and native['velocity_error']==0 and native['refusal_checks']==4
        assert [x['tick'] for x in lines]==list(range(0,5121,40))
        assert all(np.isfinite(list(x.values())).all() for x in lines)
        assert max(abs(x['gas_balance_j']) for x in lines)<1e-8
        assert max(abs(x['solid_balance_j']) for x in lines)<1e-9
        m=metrics(lines,refs['time']);row=dict(n=n,spacing_m=.0625/n,window_s=window/1600,metrics=m,gates={k:v<=.1 for k,v in m.items()},native=native)
        results.append(row);histories.append(lines);print(json.dumps(row),flush=True)
        (out/'report.json').write_text(json.dumps(dict(threshold_relative=.1,cases=results,complete=len(results)==3,full_box_qualified=False,normal_diffusion_only=True),indent=2)+'\n')
    (out/'refinement.json').write_text(json.dumps(dict(spatial_at_fixed_scalar_dt=metrics(histories[0],histories[1]),window_at_fixed_grid=metrics(histories[1],histories[2])),indent=2)+'\n')
if __name__=='__main__':main()
