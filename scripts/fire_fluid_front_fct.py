"""Bounded update-limited scalar candidate; unchanged references and 10% gate."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from fire_fluid_front_arrival import reference,analyze
from fire_fluid_thermal_transport import execute

def main():
    p=argparse.ArgumentParser()
    for name in ['build','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve()
    identity=json.loads((build/'identity.json').read_text());assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    cases=[];refs={}
    matrix=[(128,steps,u,.026,mode) for steps in [10240,20480] for u in [0,.003125,.01875] for mode in [4,5]]
    matrix += [(128,10240,u,0,mode) for u in [-.01875,.01875] for mode in [4,5]]
    matrix += [(256,10240,u,.026,4) for u in [.003125,.01875]]
    for n,steps,u,k,mode in matrix:
        key=(n,u,k)
        if key not in refs:refs[key]=tuple(np.array([reference(n,t*.025,u,k,q) for t in range(129)]) for q in [16,32])
        name=f'case-{n}-{steps}-{u}-{k}-{mode}'
        rows=execute([build/'probe',n,steps,u,mode,k],out/(name+'.jsonl'),15)
        result=analyze(rows,*refs[key]);result['name']=name
        # Report pointwise zero-reference smearing separately, never as a ratio pass.
        # It is not an additional exact-zero acceptance requirement.
        v=np.array([r['profile'] for r in rows[:-1]]);ref=refs[key][0]
        result['zero_reference_leakage_max_k']=float(v[ref==0].max()) if (ref==0).any() else 0
        assert all(result['physical_bounds'][k] for k in ['nonnegative','maximum_principle'])
        cases.append(result)
        print(json.dumps(dict(name=name,maximum=result['native']['maximum'],metrics=result['metrics'])),flush=True)
    for mode in [4,5]:execute([build/'probe-sanitize',128,10240,.01875,mode,0],out/f'sanitize-{mode}.jsonl',30)
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,reference_threshold=.005,cases=cases,mode_names={'4':'raw fourth-order target with update limiter','5':'retained face-bounded target with update limiter'},wall_integration_admitted=all(v is not False for c in cases for v in c['gates'].values()) and all(c['physical_bounds']['maximum_principle'] and c['physical_bounds']['nonnegative'] for c in cases)),indent=2)+'\n')

if __name__=='__main__':main()
