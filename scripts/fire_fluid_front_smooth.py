"""Front-to-smooth curved-profile qualification at fixed 128 cells and unchanged gates."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np
from fire_fluid_front_arrival import reference,analyze
from fire_fluid_front_flux import point_reference
from fire_fluid_thermal_transport import execute


def main():
    p=argparse.ArgumentParser()
    for key in ['build','frozen','baseline','previous','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    previous=json.loads((a.previous/'qualified/report.json').read_text())
    identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n');inputs={};checks={}
    for name in ['smooth-stage-test','smooth-stage-test-sanitize','logquadratic-test','logquadratic-test-sanitize','front-stage-test','front-stage-test-sanitize','fct-front-smooth','fct-front-smooth-sanitize','fct-test','fct-test-sanitize','parity','parity-sanitize','exponential-test','exponential-test-sanitize']:
        result=subprocess.run([str(build/name)],capture_output=True,text=True,timeout=30)
        (out/(name+'.log')).write_text(result.stdout+result.stderr);result.check_returncode();checks[name]=result.stdout.strip()
    def run(name,args,binary='flux-front-smooth'):
        return execute([build/binary,*args],out/(name+'.jsonl'),30)
    def flux_metrics(row,t,u):
        x=np.arange(128)*.25/128;temp,grad=point_reference(x,t,u)
        fields={'temperature':temp,'gradient':grad,'advection':u*temp,'diffusion':-.026/1206*grad,'total':u*temp-.026/1206*grad}
        keys={'temperature':'bounded_temperature','gradient':'bounded_gradient','advection':'advection','diffusion':'diffusion','total':'corrected'}
        patches=[]
        for patch in [0,2,3,4,7]:
            faces=[patch*16,((patch+1)*16)%128];metrics={}
            for key,exact in fields.items():
                actual=np.asarray(row[keys[key]])[faces];ref=exact[faces];denom=np.abs(ref).sum()
                metrics[key]=dict(relative=float(np.abs(actual-ref).sum()/denom) if denom else None,actual=actual.tolist(),reference=ref.tolist())
            patches.append(dict(patch=patch,metrics=metrics,theta=np.asarray(row['theta'])[faces].tolist()))
        return patches
    frozen=[]
    for file in sorted((a.frozen/'runs').glob('reference-128-*.bin'))+sorted((a.frozen/'focus').glob('reference-128-*.bin')):
        tokens=file.stem.split('-');u=float(tokens[2]) if len(tokens)==4 else .003125;t=float(tokens[-1])
        inputs[str(file.resolve())]=hashlib.sha256(file.read_bytes()).hexdigest()
        args=[128,u,.026,1/3200,file.resolve()]
        row=run(file.stem,args)[0];check=run(file.stem+'-sanitize',args,'flux-front-smooth-sanitize')[0];assert row==check
        baseline=run(file.stem+'-baseline',args,'flux-probe')[0];assert baseline==json.loads(file.with_suffix('.jsonl').read_text())
        frozen.append(dict(u=u,t=t,patches=flux_metrics(row,t,u),baseline_patches=flux_metrics(baseline,t,u)))
    cases=[];evolved=[]
    for steps,u,k in [(10240,.003125,.026),(20480,.003125,.026),(10240,.01875,0)]:
        name=f'{steps}-{u}-{k}';args=[128,steps,u,5,k]
        rows=run(name,args,'probe-front-smooth');ref=tuple(np.array([reference(128,j*.025,u,k,q) for j in range(129)]) for q in [16,32])
        result=analyze(rows,*ref);result['name']=name
        prior=next(c for c in previous['cases'] if c['name']==name);result['previous_repair']=prior
        previous_path=a.previous/'qualified'/f'{name}.jsonl'
        inputs[str(previous_path.resolve())]=hashlib.sha256(previous_path.read_bytes()).hexdigest()
        baseline=a.baseline/f'case-128-{steps}-{u}-{k}-5.jsonl';inputs[str(baseline.resolve())]=hashlib.sha256(baseline.read_bytes()).hexdigest()
        oldrows=[json.loads(l) for l in baseline.read_text().splitlines()];result['baseline']=analyze(oldrows,*ref)
        if steps==10240 and k:
            sanitized=run(name+'-sanitize',args,'probe-front-smooth-sanitize');assert rows==sanitized
            # Old binary source/trace identity, independent of repair selection.
            replay=run(name+'-baseline-replay',args,'probe');assert replay==oldrows
            for t in [.025,.5,.75,1,1.5]:
                v=np.array(rows[round(t/.025)]['profile']);file=out/f'evolved-{t}.bin';v.tofile(file)
                row=run(f'evolved-{t}',[128,u,k,1/3200,file])[0];exact=reference(128,t,u,k,32)
                means=[dict(patch=q,actual_k=float(v[q*16:(q+1)*16].mean()),reference_k=float(exact[q*16:(q+1)*16].mean())) for q in [2,3,4,7]]
                evolved.append(dict(t=t,zero_cells=int((v==0).sum()),reference_positive_cells=int((exact>0).sum()),limited_faces=row['limited_faces'],means=means,patches=flux_metrics(row,t,u)))
        cases.append(result)
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    inputs[str((a.previous/'qualified/report.json').resolve())]=hashlib.sha256((a.previous/'qualified/report.json').read_bytes()).hexdigest()
    report=dict(threshold=.1,reference_threshold=.005,n=128,candidate='positive log-quadratic profile with repaired polynomial fallback',checks=checks,frozen=frozen,cases=cases,evolved=evolved,inputs=inputs,sanitizer_exact=True,baseline_exact=True)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    for c in cases:print(json.dumps(dict(name=c['name'],metrics=c['metrics'],gates=c['gates'],bounds=c['physical_bounds'])))


if __name__=='__main__':main()
