"""Bounded 128-normal prescribed shear; fixed physical references and solid oracle."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import json
import math
from pathlib import Path
import subprocess
import numpy as np
from fire_fluid_thermal_transport import execute
from fire_fluid_sharp_transport import relative
from fire_fluid_faint_kernel import arrival


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')


def solid_audit(path):
    lines=path.read_text().splitlines();header=lines.pop(0).split()
    assert header[:5]==['columns_solid_v1','128','128','5','800']
    assert len(lines)==160
    with localcontext() as ctx:
        ctx.prec=80;D=Decimal.from_float;zero=Decimal(0)
        energy=[[zero,zero] for _ in range(128)]
        errors=[[zero,zero] for _ in range(128)];den=[[zero,zero] for _ in range(128)]
        failed=[[0,0] for _ in range(128)];largest_failed=zero
        max_ledger=zero;max_ledger_relative=zero;totals=[zero]*128
        rate=Decimal('.08')/1600
        for window,line in enumerate(lines):
            fields=line.split();assert int(fields.pop(0))==5*(window+1) and len(fields)==128*10
            for j in range(128):
                q,debit,received,e0,e1,*amounts=[float.fromhex(x) for x in fields[j*10:j*10+10]]
                assert all(math.isfinite(x) and x>=0 for x in [q,e0,e1,*amounts])
                assert sum(amounts)==q
                for value in [debit,received]:
                    bound=D(32768*np.finfo(float).eps*abs(q))+D(64*math.ulp(q))+D(64*math.ulp(value))
                    assert abs(D(value)-D(q))<=bound,(window,j,value,q)
                for amount in amounts:
                    total=energy[j][0]+energy[j][1]+D(amount)
                    difference=(energy[j][0]-energy[j][1]+D(amount))/(1+rate)
                    energy[j]=[(total+difference)/2,(total-difference)/2]
                for d,actual in enumerate([e0,e1]):
                    ref=energy[j][d];err=abs(D(actual)-ref)
                    errors[j][d]+=err;den[j][d]+=abs(ref)
                    if ref and err/abs(ref)>Decimal('.1'):
                        failed[j][d]+=1;largest_failed=max(largest_failed,abs(ref))
                totals[j]+=D(q);residual=abs(D(e0)+D(e1)-totals[j])
                ledger_bound=D(32768*np.finfo(float).eps)*(abs(D(e0))+abs(D(e1))+abs(totals[j]))+D(64*math.ulp(float(totals[j])))
                assert residual<=ledger_bound,(window,j,residual,ledger_bound)
                max_ledger=max(max_ledger,residual)
                if totals[j]:max_ledger_relative=max(max_ledger_relative,residual/abs(totals[j]))
        local=[]
        for j in range(128):
            assert all(den[j][d] or not errors[j][d] for d in range(2))
            ratios=[float(errors[j][d]/den[j][d]) if den[j][d] else 0 for d in range(2)]
            local.append(dict(strip=j,history_error=ratios,gate_pass=all(e<=.1 for e in ratios),point_failures=failed[j]))
        assert all(v['gate_pass'] for v in local)
        return dict(columns=128,microsteps=160*5*128,all_histories_pass=True,
            worst_history_error=max(max(v['history_error']) for v in local),
            point_failures=sum(sum(v) for v in failed),largest_failed_reference_j=str(largest_failed),
            max_energy_ledger_residual_j=float(max_ledger),max_energy_ledger_relative=float(max_ledger_relative),
            local=local)


def main():
    p=argparse.ArgumentParser()
    for key in ['baseline','repaired','references','faint','output']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--reuse-native',action='store_true');a=p.parse_args()
    out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    inputs={str(Path(__file__).resolve()):sha(Path(__file__))}
    for name in ['fire_fluid_faint_kernel.py','fire_fluid_sharp_transport.py']:
        helper=Path(__file__).parent/name;inputs[str(helper.resolve())]=sha(helper)
    provenance=out.parent/'provenance.json'
    inputs[str(provenance)]=sha(provenance)
    for group in json.loads(provenance.read_text()).values():
        assert all(sha(Path(k))==v for k,v in group.items()),'retained proof drift'
    def read(path):
        inputs[str(path.resolve())]=sha(path)
        return [json.loads(x) for x in path.read_text().splitlines()]
    refs={k:read(a.references/f'reference-{k}.jsonl')[:21] for k in ['base','modes','normal','time']}
    tail=read(a.faint/'tail-time.jsonl')
    rarepath=a.faint/'qualified-final/report.json';inputs[str(rarepath.resolve())]=sha(rarepath)
    rare=json.loads(rarepath.read_text())
    # These local solid reference histories qualify the later full history, not
    # this short faint prefix. Preserve the cancellation evidence explicitly.
    local_reference={}
    for patch in [5,6]:
        vals=read(a.faint/f'p{patch}-time.jsonl')[:21]
        last=vals[-1]
        local_reference[str(patch)]=dict(short_prefix_qualified=False,
            final_direct_solid0_k=last['solid0_k'],final_absolute_transform_sum=last['absolute_solid_sum'],
            reason='Short-prefix inverse-transform cancellation; no physical local-solid pass asserted.')
    bounds=[arrival(i*.025,128) for i in range(21)]
    ref_changes={k:max(relative([r[k] for r in refs[lo]],[r[k] for r in refs[hi]])
        for lo,hi in [('base','modes'),('modes','normal'),('normal','time')]) for k in ['transport_k','near_k']}
    assert max(ref_changes.values())<.005
    identities={};cases={};raw={}
    for name,build in [('baseline',a.baseline),('repaired',a.repaired)]:
        if a.reuse_native:identity=json.loads((out/(name+'-identity.json')).read_text())
        else:identity=json.loads((build/'identity.json').read_text());write(out/(name+'-identity.json'),identity)
        assert identity['core_version']=='0.3.3'
        assert all(sha(Path(k))==v for k,v in identity['inputs'].items()),'source drift'
        assert '-DPHYSICS_SIM_LOCAL_MICRO_FACTOR=2' in identity['flags']
        assert ('-DPHYSICS_SIM_REPAIRED_SCALARS=1' in identity['flags'])==(name=='repaired')
        identities[name]=identity
        def run(exe,tag,steps,cap):
            path=out/(name+'-'+tag+'.jsonl');trace=out/(name+'-'+tag+'.packets')
            command=list(map(str,[build/exe,128,128,5,steps,10,trace]))
            if a.reuse_native or path.exists():
                receipt=json.loads(path.with_suffix('.receipt.json').read_text())
                assert receipt['returncode']==0 and receipt['command']==command
                return read(path)
            return execute(command,path,cap)
        if not a.reuse_native:
            for test in ['columns-test','face-test','support-test','slab-test']:
                result=subprocess.run([build/test],capture_output=True,text=True,timeout=30,check=True)
                (out/(name+'-'+test+'.log')).write_text(result.stdout+result.stderr)
        strict=run('probe','smoke',80,180);san=run('probe-sanitize','sanitize',80,300)
        assert strict==san and (out/(name+'-smoke.packets')).read_bytes()==(out/(name+'-sanitize.packets')).read_bytes()
        print(name+' strict/sanitizer smoke exact',flush=True)
        rows=run('probe','full',800,1200);terminal=rows.pop()
        assert rows[:3]==strict[:-1] and len(rows)==21
        assert [r['tick'] for r in rows]==list(range(0,801,40))
        assert terminal['steps']==800 and terminal['refusal_checks']==6 and terminal['velocity_error']==0 and terminal['memory_live_bytes']==0
        assert terminal['minimum_k']>=0 and terminal['maximum_k']<=.4+1e-10 and terminal['fine_substeps']==8
        assert all(np.isfinite(v).all() for row in rows for v in row.values())
        assert abs(rows[-1]['source_j']-.05888671875)<1e-10
        assert max(abs(r['gas_balance_j']) for r in rows)<1e-8
        assert max(abs(r['solid_balance_j']) for r in rows)<1e-9
        for row in rows:
            assert abs(sum(row['local_q_j'])-row['q_j'])<1e-12
            assert abs(50/8*sum(row['solid0_k']+row['solid1_k'])-row['q_j'])<1e-12
        gas={k:relative([r[k] for r in rows],[r[k] for r in refs['time']]) for k in ['transport_k','near_k']}
        gas['tail_k']=relative([r['tail_k'] for r in rows],[r['gas_k'] for r in tail])
        low=np.array([r['lower_q_j'] for r in bounds]);high=np.array([r['upper_q_j'] for r in bounds]);actual=np.array([r['q_j'] for r in rows])
        lower=float(np.maximum(np.maximum(low-actual,actual-high),0).sum()/high.sum())
        upper=float(np.maximum(abs(actual-low),abs(actual-high)).sum()/low.sum())
        points={}
        for i in [1,4,16,20]:
            t=i*.025;ref=(rare['gas_tail_checks']['selected_points'][str(t)]['reference_k'] if i in [1,4] else tail[i]['gas_k'])
            points[str(t)]=dict(gas_tail_k=rows[i]['tail_k'],reference_tail_k=ref,tail_relative_error=abs(rows[i]['tail_k']/ref-1),
                wall_q_j=rows[i]['q_j'],wall_interval_j=[low[i],high[i]],wall_relative_error_lower=max(low[i]-actual[i],actual[i]-high[i],0)/high[i])
        solid=solid_audit(out/(name+'-full.packets'))
        inputs[str((out/(name+'-full.packets')).resolve())]=sha(out/(name+'-full.packets'))
        cases[name]=dict(gas_history_error=gas,gas_gates={k:v<=.1 for k,v in gas.items()},wall_history_error_interval=[lower,upper],
            wall_gate=True if upper<=.1 else False if lower>.1 else None,solid_given_delivered_heat=solid,
            terminal=terminal,points=points,final=rows[-1]);raw[name]=rows
        print(json.dumps({k:v for k,v in cases[name].items() if k not in ['solid_given_delivered_heat']}),flush=True)
        write(out/'progress.json',cases)
    shared=set(identities['baseline']['inputs'])&set(identities['repaired']['inputs'])
    assert all(identities['baseline']['inputs'][k]==identities['repaired']['inputs'][k] for k in shared)
    delta={k:relative([r[k] for r in raw['repaired']],[r[k] for r in raw['baseline']]) for k in ['transport_k','near_k','tail_k','q_j']}
    result=dict(gate=.1,interval_s=[0,.5],normal_cells=128,scalar_dt_s=1/25600,coarse_scalar_dt_s=1/6400,
        core_version='0.3.3',reference_changes=ref_changes,cases=cases,repaired_vs_baseline=delta,
        local_physical_solid_reference=local_reference,feedback=False,full_box_qualified=False,inputs=inputs)
    assert all(sha(Path(k))==v for k,v in inputs.items())
    write(out/'report.json',result)
    if not a.reuse_native:
        write(out/'native-sha256.json',{p.name:sha(p) for p in out.iterdir() if p.suffix in ['.jsonl','.packets','.log']})
    else:
        assert all(sha(out/k)==v for k,v in json.loads((out/'native-sha256.json').read_text()).items())


if __name__=='__main__':main()
