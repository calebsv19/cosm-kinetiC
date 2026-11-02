"""Reference/evolved same-field face diagnostics with observed FCT activity."""
import argparse,hashlib,json,math
from pathlib import Path
import numpy as np
from fire_fluid_front_arrival import reference
from fire_fluid_thermal_transport import execute
from fire_fluid_thermal_edge import compare


def point_reference(x,t,u,points_images=2):
    width=2*math.sqrt(.026/1206*t);value=np.zeros_like(x);gradient=np.zeros_like(x)
    erfc=np.vectorize(math.erfc)
    for j in range(-points_images,points_images+1):
        a=(x-(1/32+u*t+j*.25))/width;b=(x-(1/16+u*t+j*.25))/width
        value+=.5*np.where(b<=0,erfc(-a)-erfc(-b),erfc(b)-erfc(a))
        gradient+=(np.exp(-a*a)-np.exp(-b*b))/(math.sqrt(math.pi)*width)
    return value,gradient


def main():
    p=argparse.ArgumentParser()
    for name in ['build','traces','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=json.loads((build/'identity.json').read_text());assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n');inputs={};results=[]
    for u in [0,.003125,.01875]:
        trace=a.traces/f'case-128-10240-{u}-0.026-5.jsonl'
        inputs[str(trace.resolve())]=hashlib.sha256(trace.read_bytes()).hexdigest()
        old=[json.loads(l) for l in trace.read_text().splitlines()]
        for t in [.5,1.5,3.2]:
            for n,origin in [(128,'reference'),(256,'reference'),(128,'evolved')]:
                v=reference(n,t,u,.026,32) if origin=='reference' else np.array(old[round(t/.025)]['profile'])
                assert v.min()>=0
                name=f'{origin}-{n}-{u}-{t}';file=out/(name+'.bin');v.astype('float64').tofile(file)
                row=execute([build/'flux-probe',n,u,.026,1/3200,file],out/(name+'.jsonl'),15)[0]
                x=np.arange(n)*.25/n;temp,grad=point_reference(x,t,u)
                adv=u*temp;diff=-.026/1206*grad;total=adv+diff
                check_t,check_g=point_reference(x,t,u,3)
                assert np.array_equal(temp,check_t) and np.array_equal(grad,check_g)
                patches=[]
                for patch in [0,2,3,4,7]:
                    faces=[patch*n//8,((patch+1)*n//8)%n]
                    def metric(native,exact):
                        result=compare(np.asarray(native)[faces],exact[faces]);result['gate_pass']=None if result['relative'] is None else result['relative']<=.1
                        return result
                    rawtotal=u*np.array(row['raw_temperature'])-.026/1206*np.array(row['raw_gradient'])
                    patches.append(dict(patch=patch,faces_m=x[faces].tolist(),raw_temperature=metric(row['raw_temperature'],temp),bounded_temperature=metric(row['bounded_temperature'],temp),raw_gradient=metric(row['raw_gradient'],grad),bounded_gradient=metric(row['bounded_gradient'],grad),advection=metric(row['advection'],adv),diffusion=metric(row['diffusion'],diff),raw_total=metric(rawtotal,total),bounded_total=metric(row['target'],total),corrected_total=metric(row['corrected'],total),reference_total_flux=total[faces].tolist(),native_total_flux=np.array(row['corrected'])[faces].tolist(),limiter_factors=np.array(row['theta'])[faces].tolist(),limiter_change=metric(np.array(row['corrected'])-np.array(row['target'])+total,total)))
                results.append(dict(name=name,origin=origin,n=n,u=u,t=t,limited_faces=row['limited_faces'],parity=row['update_parity_relative'],parity_absolute=row['update_parity_absolute'],patches=patches))
    # Actual limiter activity positive control; no undefined point gradient at
    # the sharp, zero-diffusion reference discontinuity is used as an oracle.
    trace=a.traces/'case-128-10240-0.01875-0-5.jsonl';inputs[str(trace.resolve())]=hashlib.sha256(trace.read_bytes()).hexdigest()
    rows=[json.loads(l) for l in trace.read_text().splitlines()];controls=[]
    for index in [1,128]:
        name=f'advection-control-{index}';file=out/(name+'.bin');np.array(rows[index]['profile'],dtype='float64').tofile(file)
        row=execute([build/'flux-probe',128,.01875,0,1/3200,file],out/(name+'.jsonl'),15)[0];controls.append(dict(time=rows[index]['time_s'],limited_faces=row['limited_faces'],min_factor=min(row['theta']),parity=row['update_parity_relative']))
    # Hook observation cannot alter the arithmetic or full trace output.
    rows=execute([build/'probe',128,10240,.01875,5,0],out/'hook-disabled-parity.jsonl',15)
    assert rows==[json.loads(l) for l in trace.read_text().splitlines()]
    source=out/'reference-256-0.003125-1.5.bin'
    checked=execute([build/'flux-probe-sanitize',256,.003125,.026,1/3200,source],out/'sanitize.jsonl',30)[0]
    expected=json.loads((out/'reference-256-0.003125-1.5.jsonl').read_text());assert checked==expected
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,cases=results,controls=controls,inputs=inputs,hook_disabled_trace_exact=True,sanitizer_exact=True,new_trajectories=0,trajectory_replay_controls=1),indent=2)+'\n')
    print(json.dumps(dict(frozen_probes=len(results),controls=controls)))

if __name__=='__main__':main()
