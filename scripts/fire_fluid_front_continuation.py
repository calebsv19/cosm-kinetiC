"""Conditional restart attribution; never a production history qualification."""
import argparse,hashlib,json,subprocess
from pathlib import Path
import numpy as np
from fire_fluid_front_arrival import reference
from fire_fluid_front_flux import point_reference
from fire_fluid_sharp_transport import relative
from fire_fluid_thermal_transport import execute


def integrated_reference(start,stop,points=64):
    # t=start+(stop-start)*s^2 regularizes the initial t^(-1/2) flux.
    nodes,weights=np.polynomial.legendre.leggauss(points);s=(nodes+1)/2
    x=np.arange(128)*.25/128;adv=np.zeros(128);diff=np.zeros(128)
    for sample,weight in zip(s,weights):
        t=start+(stop-start)*sample*sample;temperature,gradient=point_reference(x,t,.003125)
        measure=(stop-start)*sample*weight
        adv+=measure*.003125*temperature;diff+=measure*(-.026/1206)*gradient
    return dict(advection=adv,diffusion=diff,total=adv+diff)


def main():
    p=argparse.ArgumentParser()
    for key in ['build','previous','output']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve()
    identity=json.loads((build/'identity.json').read_text())
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'identity.json').write_text(json.dumps(identity,indent=2)+'\n')
    trace=a.previous/'qualified/10240-0.003125-0.026.jsonl'
    old=[json.loads(l) for l in trace.read_text().splitlines()];inputs={str(trace.resolve()):hashlib.sha256(trace.read_bytes()).hexdigest()}
    cases=[];references={}
    for kind,start,denominator in [('native',0,3200),('native',.025,3200),('reference',.025,3200),('reference',.1,3200),('reference',.5,3200),('reference',.1,6400)]:
        stop=1.5;name=f'{kind}-{start}-{denominator}';first=round(start*denominator);last=round(stop*denominator)
        if kind=='native':v=np.array(old[round(start/.025)]['profile'])
        else:
            v=reference(128,start,.003125,.026,64);fine=reference(128,start,.003125,.026,96)
            assert relative(v,fine)<.005
        file=out/(name+'.bin');v.astype('float64').tofile(file)
        rows=execute([build/'continuation-front-smooth',denominator,first,last,file],out/(name+'.jsonl'),60)
        terminal=rows[-1];profile=np.array([r['profile'] for r in rows[:-1]])
        indices=np.array([round(r['time_s']/.025) for r in rows[:-1]])
        assert np.array_equal(indices,np.arange(round(start/.025),61))
        baseline=np.array([old[i]['profile'] for i in indices])
        if kind=='native':assert np.array_equal(profile,baseline)
        if start not in references:
            references[start]=tuple(np.array([reference(128,i*.025,.003125,.026,q) for i in indices]) for q in [32,64])
        expected,check=references[start]
        metrics={'profile_window':relative(profile,expected)};baseline_metrics={'profile_window':relative(baseline,expected)}
        qualified={'profile_window':relative(expected,check)};budgets=[]
        flux=integrated_reference(start,stop,64);flux_check=integrated_reference(start,stop,32)
        end=rows[-2];initial_ref=reference(128,start,.003125,.026,64)
        for patch in [0,2,3,4,7]:
            sl=slice(patch*16,(patch+1)*16);faces=[patch*16,((patch+1)*16)%128]
            actual=profile[:,sl].mean(1);ref=expected[:,sl].mean(1);fine=check[:,sl].mean(1);key=f'patch{patch}_window'
            metrics[key]=relative(actual,ref);baseline_metrics[key]=relative(baseline[:,sl].mean(1),ref);qualified[key]=relative(ref,fine)
            components={}
            for label in ['total','advection','diffusion']:
                key={'total':'integrated_flux_k_m','advection':'integrated_advection_k_m','diffusion':'integrated_diffusion_k_m'}[label]
                actual_flux=np.array(end[key])[faces];ref_flux=flux[label][faces];q=relative(ref_flux,flux_check[label][faces])
                components[label]=dict(actual_k_m=actual_flux.tolist(),reference_k_m=ref_flux.tolist(),relative=relative(actual_flux,ref_flux),reference_quadrature_change=q)
            ref_change=float((expected[-1,sl].sum()-initial_ref[sl].sum())*.25/128)
            net=float(flux['total'][faces[0]]-flux['total'][faces[1]])
            oracle_net_error=abs(net-ref_change)/abs(ref_change) if ref_change else None
            assert oracle_net_error is None or oracle_net_error<.005
            budgets.append(dict(patch=patch,components=components,reference_inventory_change_k_m=ref_change,reference_flux_inventory_relative=oracle_net_error,limiter_adjustment_k_m=np.array(end['integrated_limiter_adjustment_k_m'])[faces].tolist()))
        assert all(q is None or q<.005 for q in qualified.values())
        assert terminal['minimum']>=0 and terminal['maximum']<=1+1e-12
        result=dict(name=name,kind=kind,start_s=start,stop_s=stop,dt=1/denominator,metrics=metrics,baseline_same_window=baseline_metrics,reference_quadrature_change=qualified,conditional_gates={k:(v<=.1 if v is not None and qualified[k] is not None else None) for k,v in metrics.items()},budgets=budgets,native=terminal)
        if name=='reference-0.025-3200':
            checked=execute([build/'continuation-front-smooth-sanitize',denominator,first,last,file],out/'sanitize.jsonl',60);assert checked==rows
        cases.append(result);print(json.dumps(dict(name=name,metrics=metrics,conditional_gates=result['conditional_gates'])),flush=True)
    # Constant-field successful control qualifies zero change and finite through-flow.
    file=out/'constant.bin';np.full(128,.125,dtype='float64').tofile(file)
    control=execute([build/'continuation-front-smooth',3200,320,400,file],out/'constant.jsonl',30)
    assert all(np.array_equal(np.array(row['profile']),np.full(128,.125)) for row in control[:-1])
    assert np.allclose(control[-2]['integrated_advection_k_m'],.003125*.125*.025,rtol=1e-12,atol=0)
    assert np.array_equal(control[-2]['integrated_diffusion_k_m'],np.zeros(128))
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in inputs.items())
    checks={}
    for name in ['logquadratic-test','logquadratic-test-sanitize','smooth-stage-test','smooth-stage-test-sanitize']:
        result=subprocess.run([str(build/name)],capture_output=True,text=True,timeout=30)
        (out/(name+'.log')).write_text(result.stdout+result.stderr);result.check_returncode();checks[name]=result.stdout.strip()
    assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
    (out/'report.json').write_text(json.dumps(dict(threshold=.1,reference_threshold=.005,n=128,conditional_experiment=True,cases=cases,checks=checks,inputs=inputs,baseline_replay_exact=True,native_checkpoint_replay_exact=True,sanitizer_exact=True,constant_control_pass=True),indent=2)+'\n')


if __name__=='__main__':main()
