"""Two fixed-timing, fixed-normal-grid interface layouts through .5 s only."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_sharp_transport import relative
from fire_fluid_faint_kernel import arrival


def main():
    p=argparse.ArgumentParser()
    for key in ['base','extended','previous','references','faint','output']:
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--reuse-native',action='store_true')
    a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=a.reuse_native)
    identities={}
    for name in ['base','extended']:
        build=getattr(a,name);identity=json.loads(((out/(name+'-identity.json')) if a.reuse_native else (build/'identity.json')).read_text())
        assert all(hashlib.sha256(Path(k).read_bytes()).hexdigest()==v for k,v in identity['inputs'].items())
        assert '-DPHYSICS_SIM_LOCAL_MICRO_FACTOR=2' in identity['flags']
        assert '-DPHYSICS_SIM_INTERFACE_TRACE=1' in identity['flags']
        assert f'-DPHYSICS_SIM_LOCAL_FINE_END={48 if name=="base" else 64}' in identity['flags']
        identities[name]=identity
        (out/(name+'-identity.json')).write_text(json.dumps(identity,indent=2)+'\n')
    assert identities['base']['inputs']==identities['extended']['inputs']
    def read(path):return [json.loads(x) for x in path.read_text().splitlines()]
    def run(cmd,path,cap):return read(path) if a.reuse_native else execute(cmd,path,cap)
    old=read(a.previous/'runs/normal128-half-scalar.jsonl')
    refs=read(a.references/'reference-time.jsonl')[:21]
    tail=read(a.faint/'tail-time.jsonl')
    bounds=[arrival(i*.025,128) for i in range(21)]
    cases={}
    for name in ['base','extended']:
        build=getattr(a,name)
        for test in ['columns-test','face-test','support-test','slab-test']:
            if a.reuse_native:
                assert (out/(name+'-'+test+'.log')).is_file()
            else:
                result=subprocess.run([build/test],capture_output=True,text=True,check=True,timeout=60)
                (out/(name+'-'+test+'.log')).write_text(result.stdout+result.stderr)
        smoke=run([build/'probe',128,128,5,80,10],out/(name+'-smoke.jsonl'),90)
        sanitizer=run([build/'probe-sanitize',128,128,5,80,10],out/(name+'-sanitize.jsonl'),90)
        assert smoke==sanitizer
        assert (out/(name+'-smoke.log')).read_bytes()==(out/(name+'-sanitize.log')).read_bytes()
        rows=run([build/'probe',128,128,5,800,10],out/(name+'.jsonl'),600)
        terminal=rows.pop()
        assert rows[:3]==smoke[:-1] and len(rows)==21
        assert [r['tick'] for r in rows]==list(range(0,801,40))
        assert all(np.isfinite(v).all() for r in rows for v in r.values())
        if name=='base':assert rows==old[:-1], 'instrumented original history changed'
        assert terminal['steps']==800 and terminal['fine_substeps']==8
        assert terminal['memory_live_bytes']==0 and terminal['velocity_error']==0
        assert terminal['refusal_checks']==6 and terminal['minimum_k']>=0
        assert terminal['lift_balance_max_j']<1e-9 and terminal['restriction_max_j']<1e-9
        assert max(abs(r['gas_balance_j']) for r in rows)<1e-8
        assert max(abs(r['solid_balance_j']) for r in rows)<1e-9
        assert abs(rows[-1]['source_j']-.05888671875)<1e-10
        for r in rows:assert abs(sum(r['local_q_j'])-r['q_j'])<1e-12
        metrics={key:relative([r[key] for r in rows],[r[key] for r in refs]) for key in ['transport_k','near_k']}
        metrics['tail_k']=relative([r['tail_k'] for r in rows],[r['gas_k'] for r in tail])
        actual=np.array([r['q_j'] for r in rows]);low=np.array([r['lower_q_j'] for r in bounds]);high=np.array([r['upper_q_j'] for r in bounds])
        lo=float(np.maximum(np.maximum(low-actual,actual-high),0).sum()/high.sum())
        hi=float(np.maximum(abs(actual-low),abs(actual-high)).sum()/low.sum())
        profiles=read(out/(name+'.log'))
        assert [r['tick'] for r in profiles]==list(range(40,801,40))[:19]+[780,785,800]
        cases[name]=dict(metrics=metrics,wall_heat_error_interval=[lo,hi],gates={**{k:v<=.1 for k,v in metrics.items()},'q_j':True if hi<=.1 else False if lo>.1 else None},terminal=terminal,final=rows[-1])
        print(json.dumps(dict(case=name,metrics=metrics,wall_heat_error_interval=[lo,hi],gates=cases[name]['gates'])),flush=True)
    snapshots=[]
    base=read(out/'base.log');extended=read(out/'extended.log')
    for b,e in zip(base,extended):
        assert b['tick']==e['tick']
        snapshots.append(dict(tick=b['tick'],time_s=b['tick']/1600,
            faces=[dict(parent=x['parent'],base_adv_j=x['advected_j'],extended_adv_j=y['advected_j'],base_diff_j=x['diffused_j'],extended_diff_j=y['diffused_j']) for x,y in zip(b['faces'],e['faces'])],
            profiles=[dict(parent=x['parent'],base_band_k=x['band_k'],extended_band_k=y['band_k'],base_near_k=x['near_k'],extended_near_k=y['near_k']) for x,y in zip(b['profiles'],e['profiles'])]))
    profile_reference=[]
    rp=out.parent
    for normal,modes,time in zip(read(rp/'reference-profiles.jsonl'),read(rp/'reference-modes-profiles.jsonl'),read(rp/'reference-time-profiles.jsonl')):
        profile_reference.append(dict(time_s=normal['time_s'],parent48_band_k=normal['profiles'][4]['band_k'],mode_change_relative=abs(normal['profiles'][4]['band_k']-modes['profiles'][4]['band_k'])/abs(normal['profiles'][4]['band_k']),time_change_relative=abs(normal['profiles'][4]['band_k']-time['profiles'][4]['band_k'])/abs(normal['profiles'][4]['band_k'])))
    # This attempted ordinary-real-axis reference is explicitly unqualified.
    # The separately qualified contour tail history above remains the gate.
    assert any(r['mode_change_relative']>.1 for r in profile_reference)
    operators=json.loads((out.parent/'unchanged-operators.json').read_text())
    root=Path(__file__).resolve().parents[1]
    assert all(hashlib.sha256((root/'src/tools'/k).read_bytes()).hexdigest()==v for k,v in operators.items())
    reference_paths=[a.previous/'runs/normal128-half-scalar.jsonl',a.references/'reference-time.jsonl',a.faint/'tail-time.jsonl',Path(__file__).with_name('fire_fluid_faint_kernel.py'),*rp.glob('reference-*.jsonl')]
    reference_inputs={str(path.resolve()):hashlib.sha256(path.read_bytes()).hexdigest() for path in reference_paths}
    report=dict(reference_inputs=reference_inputs,unchanged_operators=operators,local_fourier_profile_qualified=False,local_fourier_profile_checks=profile_reference,threshold=.1,interval_s=[0,.5],normal_cells=128,scalar_fine_dt_s=1/25600,scalar_coarse_dt_s=1/6400,native_dt_s=1/1600,exchange_dt_s=5/1600,source_end_s=.4,restriction_s=782/1600,cases=cases,baseline_history_exact=True,interface_snapshots=snapshots,feedback_run=False,app_default_adoption=False)
    artifacts={str(path.relative_to(out)):hashlib.sha256(path.read_bytes()).hexdigest() for path in out.iterdir() if path.suffix in ['.jsonl','.log'] or path.name.endswith('.receipt.json')}
    if a.reuse_native:assert artifacts==json.loads((out/'native-sha256.json').read_text())
    else:(out/'native-sha256.json').write_text(json.dumps(artifacts,indent=2)+'\n')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
