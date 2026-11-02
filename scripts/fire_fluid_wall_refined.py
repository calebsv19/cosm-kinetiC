"""Qualify the implemented wall-refined solver profile on the fixed experiment.
No reference/source/bound/acceptance tuning; original fixed256 supports retained.
"""
import argparse,gzip,hashlib,json
from pathlib import Path
import numpy as np
from fire_fluid_warm_solid import compare,crossing,packet_audit

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return p.read_bytes() if p.exists() else gzip.decompress(p.with_suffix(p.suffix+'.gz').read_bytes())
def rows(p):return [json.loads(s) for s in read(p).decode().splitlines()]
def arrays(rr):return {k:np.asarray([r[k] for r in rr]) for k in rr[0]}
def metric(a,b):
    err=abs(a-b);den=abs(b);point=np.divide(err,den,out=np.where(err==0,0.,np.inf),where=den!=0)
    return dict(point=float(np.max(point)),history=float(np.max(np.divide(np.sum(err,axis=0),np.sum(den,axis=0),out=np.where(np.sum(err,axis=0)==0,0.,np.inf),where=np.sum(den,axis=0)!=0))),final=float(np.max(point[-1])),failures=int(np.sum(point>.1)))
def main():
    p=argparse.ArgumentParser();p.add_argument('evidence',type=Path);a=p.parse_args();root=a.evidence.resolve();old=root.parent/'fire-fluid-warm-solid-20261020';timing=root.parent/'fire-fluid-reversal-timing-20261020'
    identity=json.loads((root/'build/identity.json' if (root/'build').exists() else root/'identity.json').read_text());assert all(sha(Path(p))==h for p,h in identity['inputs'].items())
    preserve=json.loads((old/'preserved.json').read_text());assert all(sha(Path(p))==h for p,h in preserve.items())
    oldmanifest=json.loads((old/'data-manifest.json').read_text())
    used={}
    def old_rows(name):
        item=oldmanifest['runs/'+name];p=old/item['compressed'];raw=gzip.decompress(p.read_bytes());assert sha(p)==item['compressed_sha256'] and hashlib.sha256(raw).hexdigest()==item['sha256'];used[str(p)]=sha(p)
        return [json.loads(s) for s in raw.decode().splitlines()]
    report=dict(gate=.1,reference_gate=.005,implemented_profile=dict(name='wall-refined',normal_cells=512,scalar_dt_s=1/51200,exchange_dt_s=1/6400,receiver_dt_s=1/6400,common_support_cells=256,normal_method='conservative Crank-Nicolson; centered constant mode; compensated source/flux increments',composition='normal half / native wall+receiver / normal half'),short={},full={},reference_inputs=used)
    fine=arrays(rows(timing/'runs/reference-8192-409600.jsonl'))
    for n in [256,512]:
        rr=rows(root/'runs'/f'short-{n}-probe.jsonl');v=arrays(rr[:-1]);assert len(rr)==4
        assert read(root/'runs'/f'short-{n}-probe.jsonl')==read(root/'runs'/f'short-{n}-probe-sanitize.jsonl')
        assert read(root/'runs'/f'short-{n}-probe.packets')==read(root/'runs'/f'short-{n}-probe-sanitize.packets')
        errors={k:metric(v[k]-v[k][0],fine[k]-fine[k][0]) for k in ['profile_k','q_j','gas_mean_k','solid0_k','solid1_k']}
        report['short'][str(n)]=dict(errors=errors,point_pass=all(v['point']<=.1 for v in errors.values()),native=rr[-1])
    # Exact old fast-exchange control; no implied identity equality of payload hashes.
    assert rows(root/'runs/short-256-probe.jsonl')==rows(timing/'runs/s1-e1-probe.jsonl')
    assert read(root/'runs/short-256-probe.packets')==read(timing/'runs/s1-e1-probe.packets')
    report['old_256_control_exact']=True
    assert not report['short']['256']['point_pass'] and report['short']['512']['point_pass']
    direct=[arrays(rows(root/'direct-reference'/name)) for name in ['4096-204800.jsonl','8192-204800.jsonl','8192-409600.jsonl']]
    dq=dict(spatial=metric(direct[0]['profile_k'],direct[1]['profile_k']),temporal=metric(direct[1]['profile_k'],direct[2]['profile_k']))
    assert all(max(v['point'],v['history'],v['final'])<=.005 for v in dq.values())
    native512=arrays(rows(root/'runs/short-512-probe.jsonl')[:-1])
    report['direct_512_support']=dict(reference_qualification=dq,errors=metric(native512['native_profile_k'],direct[2]['profile_k']))
    assert report['direct_512_support']['errors']['point']<=.1
    allpass=True
    for mode,name in enumerate(['warm','off','reverse']):
        rr=rows(root/'runs'/f'{name}-probe.jsonl');native=arrays(rr[:-1]);term=rr[-1]
        assert len(rr)==502 and np.array_equal(native['time_s'],np.arange(501)/100)
        assert read(root/'runs'/f'{name}-probe.jsonl')==read(root/'runs'/f'{name}-probe-sanitize.jsonl')
        assert read(root/'runs'/f'{name}-probe.packets')==read(root/'runs'/f'{name}-probe-sanitize.packets')
        assert term['normal_cells']==512 and term['scalar_steps']==256000 and term['receiver_steps']==32000 and term['windows']==32000 and term['refusals']==9
        assert term['memory_live_bytes']==0 and term['velocity_error']==0
        assert np.isfinite(native['native_profile_k']).all() and np.min(native['native_profile_k'])>=0 and np.max(native['native_profile_k'])<=21+1e-12
        # Retain the fine state, independently prove it projects onto the old support.
        assert np.allclose(native['native_profile_k'].reshape(501,256,2).mean(axis=2),native['profile_k'],rtol=0,atol=4e-15)
        assert np.max(abs(native['source_j']-4.7109375*np.minimum(native['time_s'],1)))<=1e-11 and np.all(native['source_j'][100:]==native['source_j'][100])
        refs=[arrays(old_rows(f'{name}-reference-{n}-{tps}.jsonl')) for n,tps in [(1024,51200),(2048,51200),(2048,102400)]]
        spatial,temporal,errors=compare(refs[0],refs[1],mode),compare(refs[1],refs[2],mode),compare(native,refs[2],mode)
        refpass=all(max(v['history'],v['final'])<=.005 for group in [spatial,temporal] for v in group.values())
        history=all(max(v['history'],v['final'])<=.1 for v in errors.values())
        # Signed change-about320K is an additional cancellation-sensitive screen,
        # retained separately from the declared above300K gas-rise distribution.
        points=all(v['point']<=.1 for k,v in errors.items() if k!='profile_k_change')
        events={};arrival=True
        if mode==0:
            for k,level in [('solid0_k',.1),('solid1_k',.01)]:
                tt=[crossing(r['time_s'],r[k],level) for r in refs]+[crossing(native['time_s'],native[k],level)]
                assert all(t is not None for t in tt)
                event=dict(native_s=tt[3],reference_s=tt[2],error=abs(tt[3]/tt[2]-1),spatial_change=abs(tt[0]/tt[1]-1),temporal_change=abs(tt[1]/tt[2]-1));events[k]=event
                arrival=arrival and event['error']<=.1;refpass=refpass and max(event['spatial_change'],event['temporal_change'])<=.005
        cooling=float(native['gas_mean_k'][100]-native['gas_mean_k'][-1])
        useful=bool(native['q_j'][-1]>=5 and native['solid0_k'][-1]>=.1 and native['solid1_k'][-1]>=.01 and cooling>=1) if mode==0 else True
        initial=2000 if mode==2 else 94.21875
        residual=4.7109375*native['gas_mean_k']+50*(native['solid0_k']+native['solid1_k'])-initial-native['source_j'];fraction=float(np.max(abs(residual))/(initial+4.7109375));assert fraction<=1e-8
        packet=packet_audit(root/'runs'/f'{name}-probe.packets',mode);assert packet['maximum_relative_response_error']<=.1
        control=True
        if mode==1:
            control=bool(np.all(native['q_j']==0) and np.all(native['solid0_k']==0) and np.all(native['solid1_k']==0) and np.max(abs(native['native_profile_k']-(20+np.minimum(native['time_s'],1))[:,None]))<=1e-8)
        if mode==2:control=bool(term['max_window_q_j']<0 and native['gas_mean_k'][-1]>0 and native['solid0_k'][-1]<20 and native['solid1_k'][-1]<20)
        passed=bool(refpass and history and points and useful and arrival and control and packet['reversal_sign_pass'])
        report['full'][name]=dict(pass_all=passed,history_final_pass=history,declared_point_pass=points,reference_qualified=refpass,usefulness_pass=useful,control_pass=control,errors=errors,spatial=spatial,temporal=temporal,events=events,cooling_1_to_5_k=cooling,energy_residual_fraction=fraction,native=term,packets=packet,final={k:float(native[k][-1]) for k in ['q_j','gas_mean_k','solid0_k','solid1_k','source_j']})
        allpass=allpass and passed
    report['pass_all']=allpass
    (root/'report.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps(dict(pass_all=allpass,short=report['short'],direct_512=report['direct_512_support'],full={k:{p:v[p] for p in ['pass_all','history_final_pass','declared_point_pass','final','cooling_1_to_5_k']} for k,v in report['full'].items()}),indent=2))
    return 0 if allpass else 1
if __name__=='__main__':raise SystemExit(main())
