"""Bounded prescribed-flow/solid qualification; exact support, separate ledgers."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path
import numpy as np
from fire_fluid_sharp_transport import reference,relative

def execute(command,path,cap):
    start=time.monotonic()
    with path.open('x') as output,path.with_suffix('.log').open('x') as log:
        try:
            p=subprocess.run(list(map(str,command)),stdout=output,stderr=log,timeout=cap)
            code=p.returncode
        except subprocess.TimeoutExpired:
            code=124
        path.with_suffix('.receipt.json').write_text(json.dumps(dict(command=list(map(str,command)),returncode=code,wall_s=time.monotonic()-start,cap_s=cap))+'\n')
    if code:raise RuntimeError(f'{path.name}: native failure/timeout {code}')
    return [json.loads(x) for x in path.read_text().splitlines()]

def metrics(rows,ref):
    if len(rows)!=129 or len(ref)!=129:raise ValueError('incomplete history')
    values={k:relative([x[k] for x in rows],[x[k] for x in ref]) for k in ['near_k','tail_k','solid0_k','solid1_k','q_j']}
    values['early_near']=relative([x['near_k'] for x in rows[:61]],[x['near_k'] for x in ref[:61]])
    return values

def main():
    p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items()),'source drift'
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    # Independent discretization: both spatial and temporal errors must be small
    # relative to the unchanged response gate. No native solver calls in oracle.
    refs={}
    for name,n,steps,h in [('coarse',8192,10240,10),('fine',16384,10240,10),('time',16384,20480,10),('zero',16384,20480,0),('final',32768,40960,10)]:
        refs[name]=execute([build/'reference',n,steps,h],out/f'reference-{name}.jsonl',60 if name=='final' else 30)
    spatial=metrics(refs['coarse'],refs['fine']);temporal=metrics(refs['fine'],refs['time'])
    assert all(v is not None and v<.01 for v in list(spatial.values())+list(temporal.values())),(spatial,temporal)
    exact=[dict(near_k=float(reference(11/32,12/32,i*.025,.026)),tail_k=float(reference(14/32,15/32,i*.025,.026))) for i in range(129)]
    zero={k:relative([x[k] for x in refs['zero']],[x[k] for x in exact]) for k in ['near_k','tail_k']}
    zero['early_near']=relative([x['near_k'] for x in refs['zero'][:61]],[x['near_k'] for x in exact[:61]])
    assert all(v<.01 for v in zero.values()),zero
    assert all(x['q_j']==x['solid0_k']==x['solid1_k']==0 for x in refs['zero'])
    final_delta=metrics(refs['time'],refs['final'])
    assert all(v<.001 for v in final_delta.values()),final_delta
    (out/'reference-checks.json').write_text(json.dumps(dict(spatial=spatial,temporal=temporal,final_refinement=final_delta,zero_exchange_kernel=zero),indent=2)+'\n')
    results=[]
    for window in [40,20]:
        lines=execute([build/'probe',1024,window,5120,10],out/f'window-{window}.jsonl',300)
        native=lines.pop();assert native['terminal'] and native['steps']==5120 and native['memory_live_bytes']==0 and native['velocity_error']<1e-12 and native['refusal_checks']==4
        assert [r['tick'] for r in lines]==list(range(0,5121,40))
        assert all(np.isfinite(list(r.values())).all() for r in lines)
        assert max(abs(r['gas_balance_j']) for r in lines)<1e-8
        assert max(abs(r['solid_balance_j']) for r in lines)<1e-9
        m=metrics(lines,refs['final']);row=dict(window_ticks=window,window_s=window/1600,metrics=m,gates={k:v<=.1 for k,v in m.items()},native=native)
        results.append(row);print(json.dumps(row),flush=True)
        (out/'report.json').write_text(json.dumps(dict(threshold_relative=.1,cases=results,complete=len(results)==2,full_box_qualified=False),indent=2)+'\n')
    (out/'window-refinement.json').write_text(json.dumps(metrics([json.loads(x) for x in (out/'window-40.jsonl').read_text().splitlines()][:-1],[json.loads(x) for x in (out/'window-20.jsonl').read_text().splitlines()][:-1]),indent=2)+'\n')
if __name__=='__main__':main()
