"""Original sharp-field startup qualification; reference is evaluation only."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np
from fire_fluid_front_arrival import reference,analyze
from fire_fluid_thermal_transport import execute


def main():
    p=argparse.ArgumentParser()
    for key in ['build','previous','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve()
    identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    checks={}
    for name in ['subcell-test','subcell-test-sanitize','smooth-stage-test','smooth-stage-test-sanitize','logquadratic-test','logquadratic-test-sanitize','fct-front-smooth','fct-front-smooth-sanitize','parity','parity-sanitize']:
        result=subprocess.run([str(build/name)],capture_output=True,text=True,timeout=60)
        (out/(name+'.log')).write_text(result.stdout+result.stderr);result.check_returncode();checks[name]=result.stdout.strip()
    first=[]
    for ratio,time_refinement in [(1,1),(16,1),(32,1),(32,2)]:
        name=f'first-{ratio}-{time_refinement}'
        rows=execute([build/'startup-front-smooth',10240,ratio,.003125,.026,1,time_refinement],out/(name+'.jsonl'),30)
        native=rows[-1];exact=reference(128,1/3200,.003125,.026,96);fine=reference(128,1/3200,.003125,.026,128)
        heat=float((exact[:16].sum()+exact[32:].sum())*.25/128);heat_check=float((fine[:16].sum()+fine[32:].sum())*.25/128)
        assert abs(heat/heat_check-1)<.005
        error=abs(native['first_outside_k_m']/heat-1)
        first.append(dict(ratio=ratio,time_refinement=time_refinement,native=native,reference_heat_k_m=heat,heat_error=error,gate_pass=error<=.1,cell32_error=abs(native['first_cell32_k']/exact[32]-1)))
    assert first[2]['gate_pass'] and first[3]['gate_pass']
    cases=[];inputs={};startup=[];patch_budgets=[]
    previous=json.loads((a.previous/'qualified/report.json').read_text())
    for steps,u,k in [(10240,.003125,.026),(20480,.003125,.026),(10240,.01875,0)]:
        name=f'{steps}-{u}-{k}'
        rows=execute([build/'startup-front-smooth',steps,32,u,k,steps],out/(name+'.jsonl'),120)
        refs=tuple(np.array([reference(128,j*.025,u,k,q) for j in range(129)]) for q in [16,32])
        result=analyze(rows,*refs);result['name']=name;result['previous']=next(c for c in previous['cases'] if c['name']==name)
        assert all(v is True or v is None for v in result['gates'].values())
        assert all(result['physical_bounds'][k] for k in ['nonnegative','maximum_principle'])
        old=a.previous/'qualified'/(name+'.jsonl');inputs[str(old.resolve())]=hashlib.sha256(old.read_bytes()).hexdigest()
        oldrows=[json.loads(l) for l in old.read_text().splitlines()]
        if steps==10240 and k:
            sanitized=execute([build/'startup-front-smooth-sanitize',steps,32,u,k,steps],out/'sanitize.jsonl',180);assert rows==sanitized
            replay=execute([build/'startup-front-smooth',steps,1,u,k,steps],out/'coarse-replay.jsonl',30)
            assert all(r['profile']==oldrows[i]['profile'] for i,r in enumerate(replay[:-1]))
        if k==0:assert all(r['profile']==oldrows[i]['profile'] for i,r in enumerate(rows[:-1]))
        for index in [1,2,3,4]:
            profile=np.array(rows[index]['profile']);exact=reference(128,rows[index]['time_s'],u,k,64)
            actual=float((profile[:16].sum()+profile[32:].sum())*.25/128);expected=float((exact[:16].sum()+exact[32:].sum())*.25/128)
            startup.append(dict(case=name,t=rows[index]['time_s'],outside_k_m=actual,reference_outside_k_m=expected,relative_error=abs(actual/expected-1) if expected else None))
        for index in [1,60,128]:
            profile=np.array(rows[index]['profile']);flux=np.array(rows[index]['integrated_flux_k_m'])
            for patch in [0,2,3,4,7]:
                west=patch*16;east=((patch+1)*16)%128;change=float(profile[west:west+16].sum()*.25/128)
                net=float(flux[west]-flux[east]);error=abs(change-net)
                patch_budgets.append(dict(case=name,t=rows[index]['time_s'],patch=patch,inventory_k_m=change,face_net_k_m=net,absolute_closure=error,relative_closure=error/abs(change) if change else None))
        cases.append(result);print(json.dumps(dict(name=name,metrics=result['metrics'],gates=result['gates'],native=result['native'])),flush=True)
    # Extra temporal refinement changes only the implicit subcell step, not coarse dt.
    rows=execute([build/'startup-front-smooth',10240,32,.003125,.026,80,2],out/'startup-time-refined.jsonl',60)
    profile=np.array(rows[1]['profile']);exact=reference(128,.025,.003125,.026,64)
    temporal=dict(t=.025,profile_l1=float(abs(profile-exact).sum()/abs(exact).sum()),outside_k_m=float((profile[:16].sum()+profile[32:].sum())*.25/128))
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,reference_threshold=.005,n=128,reference_state_replacement=False,first_step=first,cases=cases,startup=startup,patch_budgets=patch_budgets,fine_time_refinement=temporal,checks=checks,inputs=inputs,sanitizer_exact=True,coarse_replay_exact=True,pure_advection_replay_exact=True),indent=2)+'\n')


if __name__=='__main__':main()
