"""Audit the single selected shear checkpoint; preserve all point/reference limits."""
import argparse,hashlib,json,math
from decimal import Decimal,localcontext
from pathlib import Path
import numpy as np
from fire_fluid_faint_kernel import arrival


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return [json.loads(x) for x in p.read_text().splitlines()]
def relative(a,b):return float(np.abs(np.asarray(a)-b).sum()/np.abs(b).sum())
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')


def packet_audit(path):
    lines=path.read_text().splitlines();header=lines.pop(0).split()
    assert header[:6]==['columns_solid_v2','128','256','5','3200','6400'] and len(lines)==640
    with localcontext() as ctx:
        ctx.prec=80;D=Decimal.from_float;zero=Decimal(0)
        energy=[[zero,zero] for _ in range(128)];errors=[[zero,zero] for _ in range(128)];den=[[zero,zero] for _ in range(128)];totals=[zero]*128
        point_failures=[[0,0] for _ in range(128)];largest_failed=zero;max_gas=max_received=max_ledger=zero
        negative_packets=0
        for i,line in enumerate(lines):
            fields=line.split();assert int(fields.pop(0))==5*(i+1) and len(fields)==128*10
            for j in range(128):
                q,debit,received,e0,e1,*amounts=map(float.fromhex,fields[j*10:j*10+10]);assert sum(amounts)==q
                assert all(math.isfinite(v) for v in [q,debit,received,e0,e1,*amounts]) and e0>=0 and e1>=0
                negative_packets+=q<0
                for label,value in [('gas',debit),('received',received)]:
                    error=abs(D(value)-D(q));bound=D(32768*np.finfo(float).eps*abs(q))+D(64*math.ulp(q))+D(64*math.ulp(value))
                    assert error<=bound,(i,j,label,error,bound)
                    if label=='gas':max_gas=max(max_gas,error)
                    else:max_received=max(max_received,error)
                for amount in amounts:
                    total=energy[j][0]+energy[j][1]+D(amount);diff=(energy[j][0]-energy[j][1]+D(amount))/(1+Decimal('.08')/6400)
                    energy[j]=[(total+diff)/2,(total-diff)/2]
                for d,actual in enumerate([e0,e1]):
                    expected=energy[j][d];error=abs(D(actual)-expected);errors[j][d]+=error;den[j][d]+=abs(expected)
                    if expected and error/abs(expected)>Decimal('.1'):
                        point_failures[j][d]+=1;largest_failed=max(largest_failed,abs(expected))
                totals[j]+=D(q);error=abs(D(e0)+D(e1)-totals[j])
                bound=D(32768*np.finfo(float).eps)*(abs(D(e0))+abs(D(e1))+abs(totals[j]))+D(64*math.ulp(float(totals[j])))
                assert error<=bound,(i,j,error,bound);max_ledger=max(max_ledger,error)
        local=[]
        for j in range(128):
            assert all(den[j][d] or not errors[j][d] for d in range(2))
            ratios=[float(errors[j][d]/den[j][d]) if den[j][d] else 0 for d in range(2)]
            local.append(dict(strip=j,history_error=ratios,gate=all(e<=.1 for e in ratios),point_failures=point_failures[j]))
        return dict(local=local,all_histories_pass=all(v['gate'] for v in local),worst_history_error=max(max(v['history_error']) for v in local),
            point_failures=sum(sum(v) for v in point_failures),largest_failed_reference_j=str(largest_failed),
            max_gas_debit_residual_j=float(max_gas),max_received_delta_residual_j=float(max_received),max_energy_ledger_residual_j=float(max_ledger),
            windows=640,packets=640*128,column_microsteps=3200*128,negative_packets=negative_packets)


def main():
    p=argparse.ArgumentParser()
    for key in ['root','previous','mean','faint']:p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=root/'runs';refroot=root/'local-reference';inputs={}
    def bind(path):inputs[str(path.resolve())]=sha(path)
    def bound_read(path):bind(path);return read(path)
    preserved=json.loads((root/'preserved.json').read_text());assert all(sha(Path(k))==v for k,v in preserved.items())
    for name in ['control','selected']:
        identity=json.loads((out/(name+'-identity.json')).read_text())
        assert all(sha(Path(k))==v for k,v in identity['inputs'].items());bind(out/(name+'-identity.json'))
    for folder in [out,refroot]:
        assert all(sha(folder/k)==v for k,v in json.loads((folder/'native-sha256.json').read_text()).items());bind(folder/'native-sha256.json')
    ref_identity=json.loads((refroot/'identity.json').read_text());assert all(sha(Path(k))==v for k,v in ref_identity['inputs'].items())
    rows=bound_read(out/'selected-full.jsonl');terminal=rows.pop();prior=bound_read(a.previous/'runs/repaired-full.jsonl')[:-1]
    assert len(rows)==21 and terminal['steps']==3200 and terminal['refusal_checks']==8 and terminal['memory_live_bytes']==0 and terminal['velocity_error']==0
    assert terminal['restriction_tick']==3128 and terminal['last_source_tick']==terminal['configured_source_off_tick']==2560
    assert terminal['fine_scalar_dt_s']==1/51200 and terminal['coarse_scalar_dt_s']==1/12800
    assert terminal['observation_ticks']==160 and [r['tick'] for r in rows]==list(range(0,3201,160))
    assert all(np.isfinite(v).all() for row in rows for v in row.values())
    assert terminal['minimum_k']>=0 and terminal['maximum_k']<=.4+1e-10 and terminal['memory_peak_bytes']<=128*1024*1024
    assert abs(rows[-1]['source_j']-.05888671875)<1e-10
    assert all(row['source_j']==rows[16]['source_j'] for row in rows[16:]),'source continued after physical0.4s'
    for row in rows:
        assert abs(sum(row['local_q_j'])-row['q_j'])<1e-12
        assert abs(50/8*sum(row['solid0_k']+row['solid1_k'])-row['q_j'])<1e-12
    refs={key:bound_read(refroot/(key+'.jsonl')) for key in ['normal1024','normal2048','time','modes64']}
    tail=bound_read(a.faint/'tail-time.jsonl');rarepath=a.faint/'qualified-final/report.json';bind(rarepath);rare=json.loads(rarepath.read_text())
    bounds=[arrival(i*.025,256) for i in range(21)];low=np.array([v['lower_q_j'] for v in bounds]);high=np.array([v['upper_q_j'] for v in bounds])
    def interval_error(actual,lo,hi):
        x=np.array(actual);lower=float(np.maximum(np.maximum(lo-x,x-hi),0).sum()/np.abs(hi).sum());upper=float(np.maximum(abs(x-lo),abs(x-hi)).sum()/np.abs(lo).sum())
        return dict(error_interval=[lower,upper],gate=True if upper<=.1 else False if lower>.1 else None)
    total=interval_error([r['q_j'] for r in rows],low,high)
    prior_total=interval_error([r['q_j'] for r in prior],low,high)
    rate=interval_error(np.diff([r['q_j'] for r in rows])/.025,(low[1:]-high[:-1])/.025,(high[1:]-low[:-1])/.025)
    local=[]
    for j in range(8):
        item=dict(patch=j,x_m=[j/32,(j+1)/32],quantities={})
        for key in ['local_q_j','solid0_k','solid1_k']:
            changes={label:relative([r[key][j] for r in refs[lo]],[r[key][j] for r in refs[hi]]) for label,lo,hi in [('space','normal1024','normal2048'),('time','normal2048','time'),('modes','modes64','time')]}
            qualified=max(changes.values())<=.005
            actual=[r[key][j] for r in rows];expected=[r[key][j] for r in refs['time']]
            result=dict(reference_changes=changes,reference_history_qualified=qualified,pointwise_reference_qualified=False,
                native_final=actual[-1],reference_final=expected[-1],native_change_from_prior128=relative(actual,[r[key][j] for r in prior]))
            if qualified:result.update(physical_history_error=relative(actual,expected),gate=relative(actual,expected)<=.1,prior128_history_error_same_reference=relative([r[key][j] for r in prior],expected))
            else:result.update(gate=None,reason='Modal cancellation/refinement fails; no physical acceptance asserted.')
            item['quantities'][key]=result
        local.append(item)
    gas={}
    for key in ['transport_k','near_k']:
        changes={label:relative([r[key] for r in refs[lo]],[r[key] for r in refs[hi]]) for label,lo,hi in [('space','normal1024','normal2048'),('time','normal2048','time'),('modes','modes64','time')]}
        assert max(changes.values())<=.005
        error=relative([r[key] for r in rows],[r[key] for r in refs['time']]);gas[key]=dict(history_error=error,gate=error<=.1,reference_changes=changes,prior128_error_same_reference=relative([r[key] for r in prior],[r[key] for r in refs['time']]))
    error=relative([r['tail_k'] for r in rows],[r['gas_k'] for r in tail]);gas['tail_k']=dict(history_error=error,gate=error<=.1,reference='retained independently qualified faint-tail contour history')
    points={}
    for i in [1,4,20]:
        t=i*.025;expected=rare['gas_tail_checks']['selected_points'][str(t)]['reference_k'];actual=rows[i]['tail_k'];q=rows[i]['q_j']
        points[str(t)]=dict(tail_k=actual,reference_tail_k=expected,tail_relative_error=abs(actual/expected-1),tail_gate=abs(actual/expected-1)<=.1,
            wall_q_j=q,physical_wall_interval_j=[low[i],high[i]],wall_excess_relative_lower=max(low[i]-q,q-high[i],0)/high[i])
    packet=packet_audit(out/'selected-full.packets');bind(out/'selected-full.packets')
    mean=bound_read(a.mean/'runs/n256-quarter-exchange.jsonl')[:-1]
    mean_change=relative([r['q_j'] for r in rows],[r['q_j'] for r in mean]);assert mean_change<1e-8
    for path in [Path(__file__),Path(__file__).parent/'fire_fluid_faint_kernel.py',root/'preserved.json',out/'plan.json',refroot/'identity.json']:bind(path)
    result=dict(gate=.1,reference_refinement_gate=.005,duration_s=.5,gas=gas,total_wall_heat=total,prior128_total_wall_heat=prior_total,total_wall_rate_observation_average=rate,
        local_physical=local,solid_given_actual_q=packet,early_and_endpoint_points=points,normal_mean_parity=mean_change,terminal=terminal,
        final=rows[-1],reciprocal_gas_solid_diagnostic=True,thermal_return_channel_enabled=False,evolving_flow_qualified=False,
        application_default_adopted=False,all_local_physical_qualified=all(q['reference_history_qualified'] for item in local for q in item['quantities'].values()),inputs=inputs)
    # A derived view adds physical times/rates without rewriting native receipts.
    with (root/'physical-observations.jsonl').open('w') as stream:
        for i,row in enumerate(rows):
            item=dict(row,time_s=row['tick']/6400,wall_rate_interval_w=(row['q_j']-rows[i-1]['q_j'])/.025 if i else 0,
                local_wall_rate_interval_w=[(q-rows[i-1]['local_q_j'][j])/.025 if i else 0 for j,q in enumerate(row['local_q_j'])])
            stream.write(json.dumps(item)+'\n')
    assert all(sha(Path(k))==v for k,v in inputs.items());write(root/'report.json',result)
    print(json.dumps({k:result[k] for k in ['gas','total_wall_heat','total_wall_rate_observation_average','normal_mean_parity','all_local_physical_qualified']}),flush=True)


if __name__=='__main__':main()
