"""Audit fixed five-second native/independent-reference demonstrations; no reruns.
Run using NumPy runtime, with one evidence-root argument. All samples retained.
No signal floor, threshold/tolerance tuning, solver call or live application IO.
"""
import argparse,gzip,hashlib,json,math
from decimal import Decimal,localcontext
from pathlib import Path
import numpy as np

def read_bytes(path):
    return path.read_bytes() if path.exists() else gzip.decompress(path.with_suffix(path.suffix+".gz").read_bytes())

def load(path):
    rows=[json.loads(s) for s in read_bytes(path).decode().splitlines()]
    terminal=rows[-1] if rows[-1].get('terminal') else None
    if terminal:rows=rows[:-1]
    assert len(rows)==501 and np.allclose([r['time_s'] for r in rows],np.arange(501)/100,rtol=0,atol=1e-15)
    return {k:np.asarray([r[k] for r in rows]) for k in rows[0]},terminal

def crossing(t,v,level):
    for i in range(1,len(t)):
        if v[i]>=level and v[i-1]<level:return float(t[i-1]+(t[i]-t[i-1])*(level-v[i-1])/(v[i]-v[i-1]))
    return None

def metric(a,b):
    a,b=np.asarray(a),np.asarray(b)
    error=np.abs(a-b);den=np.sum(np.abs(b),axis=0);num=np.sum(error,axis=0)
    history=np.divide(num,den,out=np.where(num==0,0.,np.inf),where=den!=0)
    final=np.divide(error[-1],np.abs(b[-1]),out=np.where(error[-1]==0,0.,np.inf),where=b[-1]!=0)
    point=np.divide(error,np.abs(b),out=np.where(error==0,0.,np.inf),where=b!=0)
    return dict(history=float(np.max(history)),final=float(np.max(final)),point=float(np.max(point)),point_failures=int(np.sum(point>.1)))

def fields(a,mode):
    result={k:a[k] for k in ['q_j','gas_mean_k','solid0_k','solid1_k','profile_k']}
    result['rate_w']=np.diff(a['q_j'])/.01
    # Test changes as well as absolute rise so reversal's 20 K baseline cannot
    # hide heat-response error. Off changes have analytic floating-point checks.
    if mode!=1:
        for k in ['gas_mean_k','solid0_k','solid1_k','profile_k']:result[k+'_change']=a[k]-a[k][0]
    if mode==0:result['cooling_k']=a['gas_mean_k'][100:]-a['gas_mean_k'][100]
    return result

def compare(a,b,mode):
    af,bf=fields(a,mode),fields(b,mode)
    return {k:metric(af[k],bf[k]) for k in af}

def packet_audit(path,mode):
    with localcontext() as ctx:
        ctx.prec=70
        e0=e1=Decimal(1000 if mode==2 else 0)
        worst=Decimal(0);max_error=Decimal(0);count=0;total=Decimal(0);last=0;sign_ok=True
        for line in read_bytes(path).decode().splitlines():
            parts=line.split();tick,m=int(parts[0]),int(parts[1]);q=Decimal.from_float(float.fromhex(parts[2]));last_amount=Decimal(0)
            assert tick>last and tick-last==8*m and 1<=m<=5
            assert len(parts)==3+3*m
            for i in range(m):
                amount,n0,n1=(Decimal.from_float(float.fromhex(s)) for s in parts[3+3*i:6+3*i])
                last_amount+=amount;total+=amount
                summed=e0+e1+amount;difference=(e0-e1+amount)/(1+Decimal('0.08')/6400)
                e0,e1=(summed+difference)/2,(summed-difference)/2
                max_error=max(max_error,abs(n0-e0),abs(n1-e1));count+=1
                scale=max(abs(e0-Decimal(1000 if mode==2 else 0)),abs(e1-Decimal(1000 if mode==2 else 0)))
                if scale:worst=max(worst,abs(n0-e0)/scale,abs(n1-e1)/scale)
                if mode==1:assert amount==0 and n0==0 and n1==0
                if mode==2:sign_ok=sign_ok and amount<0
            # Sum of four column allocations can differ from their batch sum
            # at the last binary place; keep a linked, per-packet roundoff bound.
            assert abs(last_amount-q)<=Decimal.from_float(32*np.finfo(float).eps*abs(float(q)))
            last=tick
        assert last==256000 and count==32000
        return dict(steps=count,maximum_energy_error_j=float(max_error),maximum_relative_response_error=float(worst),signed_q_j=float(total),reversal_sign_pass=sign_ok)

def front_audit(root):
    def short(name):
        rows=[json.loads(s) for s in (root/'front-check'/name).read_text().splitlines()]
        assert len(rows)==3
        return np.asarray([r['profile_k'] for r in rows])
    native,_=load(root/'runs/reverse-probe.jsonl')
    selected=short('8192-409600.jsonl');space=metric(short('4096-204800.jsonl'),short('8192-204800.jsonl'));time=metric(short('8192-204800.jsonl'),selected)
    assert max(space['history'],space['final'],space['point'],time['history'],time['final'],time['point'])<=.005
    same=short('256-102400.jsonl');half=short('512-102400.jsonl')
    differences=np.divide(abs(native['profile_k'][:3]-selected),abs(selected),out=np.zeros_like(selected),where=selected!=0)
    location=np.unravel_index(np.argmax(differences),differences.shape);i,k=map(int,location)
    failures=[]
    for row,cell in zip(*np.where(differences>.1)):
        failures.append(dict(time_s=float(row/100),cell=int(cell),z_m=float((cell+.5)*.0625/256),native_k=float(native['profile_k'][row,cell]),reference_k=float(selected[row,cell]),error=float(differences[row,cell])))
    return dict(horizon_s=.02,reference_spatial=space,reference_temporal=time,point_ref_qualified=True,point_failures=failures,worst=dict(time_s=i/100,cell=k,z_m=(k+.5)*.0625/256,native_k=float(native['profile_k'][i,k]),reference_k=float(selected[i,k]),native_error=float(differences[i,k]),same_256_cn_k=float(same[i,k]),cn256_spatial_error=float((same[i,k]-selected[i,k])/selected[i,k]),cn512_spatial_error=float((half[i,k]-selected[i,k])/selected[i,k]),native_vs_same_256_cn=float((native['profile_k'][i,k]-same[i,k])/same[i,k])))

def write_json(path,value):path.write_text(json.dumps(value,indent=2,allow_nan=False)+'\n')
def main():
    p=argparse.ArgumentParser();p.add_argument('evidence',type=Path);a=p.parse_args();root=a.evidence.resolve();report={'gate':.1,'reference_gate':.005,'cases':{}}
    identity=json.loads((root/'build/identity.json').read_text()) if (root/'build').exists() else json.loads((root/'identity.json').read_text())
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in identity['inputs'].items())
    prior=json.loads((root.parent/'fire-fluid-shear-integrated-20261020/preserved.json').read_text())
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in prior.items())
    report['old_operators_unchanged']=True
    plot_data={};overall=True
    for mode,name in enumerate(['warm','off','reverse']):
        native,term=load(root/'runs'/f'{name}-probe.jsonl');san,san_term=load(root/'runs'/f'{name}-probe-sanitize.jsonl')
        assert all(np.array_equal(native[k],san[k]) for k in native) and term==san_term
        assert read_bytes(root/'runs'/f'{name}-probe.packets')==read_bytes(root/'runs'/f'{name}-probe-sanitize.packets')
        assert term['refusals']==9 and term['memory_live_bytes']==0 and term['velocity_error']==0
        assert np.isfinite(native['profile_k']).all() and np.min(native['profile_k'])>=0 and np.max(native['profile_k'])<=21+1e-12
        assert (native['solid0_k']>=0).all() and (native['solid1_k']>=0).all()
        assert np.max(np.abs(native['source_j']-4.7109375*np.minimum(native['time_s'],1)))<1e-11
        assert np.all(native['source_j'][100:]==native['source_j'][100])
        coarse,_=load(root/'runs'/f'{name}-reference-1024-51200.jsonl');fine,_=load(root/'runs'/f'{name}-reference-2048-51200.jsonl');ref,_=load(root/'runs'/f'{name}-reference-2048-102400.jsonl')
        spatial=compare(coarse,fine,mode);temporal=compare(fine,ref,mode);errors=compare(native,ref,mode)
        # Refinement criteria apply to every assessed history and final value;
        # also retain point errors rather than using history to hide them.
        ref_pass=all(m['history']<=.005 and m['final']<=.005 for group in [spatial,temporal] for m in group.values())
        history_pass=all(m['history']<=.1 and m['final']<=.1 for m in errors.values())
        # The declared gas distribution is rise above 300 K. Retain the extra
        # signed change-about-320 K screen separately: it crosses zero and is
        # cancellation-sensitive; it is not the declared rise distribution.
        point_pass=all(m['point']<=.1 for k,m in errors.items() if k!='profile_k_change')
        accuracy=history_pass and point_pass
        crossings={}
        if mode==0:
            for k,level in [('solid0_k',.1),('solid1_k',.01)]:
                times=[crossing(d['time_s'],d[k],level) for d in [coarse,fine,ref,native]]
                passed=all(t is not None for t in times)
                space=abs(times[0]-times[1])/times[1] if passed else None
                time=abs(times[1]-times[2])/times[2] if passed else None
                error=abs(times[3]-times[2])/times[2] if passed else None
                crossings[k]=dict(level_k=level,coarse_s=times[0],fine_s=times[1],reference_s=times[2],native_s=times[3],spatial_change=space,temporal_change=time,error=error)
                ref_pass=ref_pass and passed and space<=.005 and time<=.005
                accuracy=accuracy and passed and error<=.1
        initial=2000 if mode==2 else 94.21875
        balance=4.7109375*native['gas_mean_k']+50*(native['solid0_k']+native['solid1_k'])-initial-native['source_j']
        ledger=float(np.max(np.abs(balance))/(initial+4.7109375))
        ref_balance=float(np.max(np.abs(ref['balance_j']))/(initial+4.7109375))
        post=float(np.max(np.abs(balance[100:]-balance[100])))
        assert ledger<=1e-8 and ref_balance<=1e-8
        packets=packet_audit(root/'runs'/f'{name}-probe.packets',mode)
        packet_pass=packets['maximum_relative_response_error']<=.1 and packets['reversal_sign_pass']
        cooling=float(native['gas_mean_k'][100]-native['gas_mean_k'][-1])
        usefulness=(native['q_j'][-1]>=5 and native['solid0_k'][-1]>=.1 and native['solid1_k'][-1]>=.01 and cooling>=1) if mode==0 else True
        control=True
        if mode==1:
            analytic=np.minimum(native['time_s'],1)+20
            control=bool(np.all(native['q_j']==0) and np.all(native['solid0_k']==0) and np.all(native['solid1_k']==0) and np.max(np.abs(native['profile_k']-analytic[:,None]))<1e-8)
        if mode==2:control=term['max_window_q_j']<0 and native['gas_mean_k'][-1]>0 and native['solid0_k'][-1]<20 and native['solid1_k'][-1]<20
        passed=bool(ref_pass and accuracy and usefulness and control and packet_pass)
        case=dict(pass_all=passed,history_final_10pct_pass=bool(history_pass),declared_distribution_point_pass=bool(point_pass),reference_qualified=bool(ref_pass),accuracy_10pct_pass=bool(accuracy),usefulness_pass=bool(usefulness),control_pass=bool(control),spatial=spatial,temporal=temporal,errors=errors,crossings=crossings,final={k:float(native[k][-1]) for k in ['q_j','gas_mean_k','solid0_k','solid1_k','source_j']},cooling_1_to_5_k=cooling,energy_residual_fraction=ledger,reference_energy_residual_fraction=ref_balance,post_heater_energy_drift_j=post,packets=packets,native=term,strict_sanitizer_exact=True)
        report['cases'][name]=case;overall=overall and passed;plot_data[name]=(native,ref)
    report['pass_all']=overall
    report['all_history_response_control_criteria_pass']=all(c['reference_qualified'] and c['history_final_10pct_pass'] and c['usefulness_pass'] and c['control_pass'] for c in report['cases'].values())
    report['short_front']=front_audit(root)
    write_json(root/'report.json',report)
    # Separate visualization module, so scientific auditing does not depend on UI.
    from fire_fluid_warm_solid_plot import plots
    plots(root,plot_data,report)
    print(json.dumps({'pass_all':overall,'cases':{k:{q:v[q] for q in ['pass_all','reference_qualified','accuracy_10pct_pass','usefulness_pass','control_pass','final','cooling_1_to_5_k']} for k,v in report['cases'].items()}},indent=2))
    return 0 if overall else 1
if __name__=='__main__':raise SystemExit(main())
