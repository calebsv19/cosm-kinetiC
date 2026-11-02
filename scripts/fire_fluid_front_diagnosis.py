"""Bounded spatial controls and signed/absolute tails; reuse completed runs."""
import argparse,json,hashlib
from pathlib import Path
import numpy as np
from fire_fluid_front_arrival import reference,analyze
from fire_fluid_thermal_transport import execute


def main():
    p=argparse.ArgumentParser()
    for name in ['build','runs','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    identity=json.loads((a.build/'identity.json').read_text());assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    for name in ['parity','parity-sanitize']:execute([a.build/name],out/(name+'.jsonl'),30)
    cases=[];details=[];inputs={}
    for u in [.003125,.01875]:
        refs=tuple(np.array([reference(256,t*.025,u,.026,q) for t in range(129)]) for q in [16,32])
        for mode in [0,2]:
            name=f'spatial-256-{u}-{mode}'
            rows=execute([a.build/'probe',256,10240,u,mode,.026],out/(name+'.jsonl'),15)
            case=analyze(rows,*refs);case['name']=name;cases.append(case)
    for u in [0,.003125,.01875]:
        exact=np.array([reference(128,t*.025,u,.026) for t in range(129)])
        for mode in range(4):
            arrays=[]
            for steps in [10240,20480]:
                path=a.runs/f'case-128-{steps}-{u}-{mode}.jsonl'
                inputs[str(path.resolve())]=hashlib.sha256(path.read_bytes()).hexdigest()
                arrays.append(np.array([r['profile'] for r in map(json.loads,path.read_text().splitlines()) if 'profile' in r]))
            for patch in [0,2,3,4,7]:
                sl=slice(patch*16,(patch+1)*16);native,half=[v[:,sl].mean(1) for v in arrays];ref=exact[:,sl].mean(1)
                for label,sl_time in [('history',slice(None)),('early',slice(0,61))]:
                    x=native[sl_time];y=ref[sl_time];z=half[sl_time];scale=float(abs(y).sum())
                    details.append(dict(u=u,mode=mode,patch=patch,window=label,reference_peak_k=float(y.max()),native_peak_k=float(x.max()),signed_integrated_difference=float((x-y).sum()/scale),relative_error=float(abs(x-y).sum()/scale),absolute_max_k=float(abs(x-y).max()),half_dt_change_vs_reference=float(abs(x-z).sum()/scale)))
    original=json.loads((a.runs/'report.json').read_text());cases=original['cases']+cases
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,cases=cases,details=details,inputs=inputs,fallback_adopted=False,coupled_run_admitted=False),indent=2)+'\n')
    print(json.dumps(dict(cases=len(cases),fallback_adopted=False,coupled_run_admitted=False)))

if __name__=='__main__':main()
