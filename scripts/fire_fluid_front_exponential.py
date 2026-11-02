"""Bounded same-input reconstruction qualification; no trajectories by default."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np
from fire_fluid_front_flux import point_reference
from fire_fluid_thermal_transport import execute

def main():
    p=argparse.ArgumentParser()
    for key in ['build','inputs','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    build=a.build.resolve();identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    results=[];inputs={}
    files=sorted((a.inputs/'runs').glob('reference-128-*.bin'))+sorted((a.inputs/'focus').glob('reference-128-*.bin'))
    for file in files:
        tokens=file.stem.split('-');u=float(tokens[2]) if len(tokens)==4 else .003125;t=float(tokens[-1])
        inputs[str(file.resolve())]=hashlib.sha256(file.read_bytes()).hexdigest()
        rows={}
        for variant in ['flux-probe','flux-exponential','flux-exponential-sanitize']:
            rows[variant]=execute([build/variant,128,u,.026,1/3200,file.resolve()],out/(file.stem+'-'+variant+'.jsonl'),15)[0]
        assert rows['flux-exponential']==rows['flux-exponential-sanitize']
        # Retained binary must reproduce the prior frozen result exactly.
        assert rows['flux-probe']==json.loads(file.with_suffix('.jsonl').read_text())
        x=np.arange(128)*.25/128;temp,gradient=point_reference(x,t,u);adv=u*temp;diff=-.026/1206*gradient
        patches=[]
        for patch in [0,2,3,4,7]:
            faces=[patch*16,((patch+1)*16)%128];metrics={}
            for label,field in [('temperature',temp),('gradient',gradient),('total',adv+diff)]:
                key={'temperature':'bounded_temperature','gradient':'bounded_gradient','total':'corrected'}[label]
                denom=np.abs(field[faces]).sum()
                metrics[label]={v:(float(np.abs(np.asarray(row[key])[faces]-field[faces]).sum()/denom) if denom else None) for v,row in rows.items() if 'sanitize' not in v}
            patches.append(dict(patch=patch,metrics=metrics,theta=np.asarray(rows['flux-exponential']['theta'])[faces].tolist()))
        results.append(dict(input=str(file),u=u,t=t,patches=patches))
    checks={}
    for name in ['exponential-test','exponential-test-sanitize','fct-test','fct-test-sanitize','fct-exponential','fct-exponential-sanitize','parity','parity-sanitize']:
        result=subprocess.run([str(build/name)],capture_output=True,text=True,check=True,timeout=30)
        (out/(name+'.log')).write_text(result.stdout+result.stderr);checks[name]=result.stdout.strip()
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    report=dict(threshold=.1,n=128,new_trajectories=0,cases=results,checks=checks,inputs=inputs,sanitizer_exact=True,baseline_exact=True)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    for case in results:
        print(json.dumps(dict(u=case['u'],t=case['t'],patch3=case['patches'][2]['metrics']['total'])))
if __name__=='__main__':main()
