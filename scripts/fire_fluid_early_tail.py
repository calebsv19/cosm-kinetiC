"""Audit bounded early-tail candidate using retained independent physical references."""
import argparse
from decimal import Decimal, localcontext
import gzip
import hashlib
import json
import math
from pathlib import Path

p=argparse.ArgumentParser();p.add_argument('evidence',type=Path);p.add_argument('--horizon',choices=['0.025','0.1'],default='0.1');p.add_argument('--strict-only',action='store_true');p.add_argument('--workspace',type=Path,default=Path(__file__).resolve().parents[3]);a=p.parse_args();root=a.evidence.resolve();workspace=a.workspace.resolve()
runs=root/'coupled-bounded-runs'
def text(path):return path.read_text() if path.exists() else gzip.decompress(path.with_suffix(path.suffix+'.gz').read_bytes()).decode()
def rows(path):return [json.loads(l) for l in text(path).splitlines()]
steps=160 if a.horizon=='0.025' else 640
rr=rows(runs/'early.jsonl');assert rr[-1]['terminal'];complete=True;accepted_steps=steps
assert [v['tick'] for v in rr[:-1]]==list(range(0,steps+1,160))
assert rr[-1]['memory_live_bytes']==0 and rr[-1]['velocity_error']==0 and abs(rr[-1]['gas_balance_j'])<1e-8
packets=text(runs/'early.packets').splitlines()
assert packets[0].split()[:6]==['columns_solid_v2','128','256','5',str(steps),'6400']
assert len(packets)==accepted_steps//5+1
sanitizer_equal=None
if not a.strict_only:
 san=rows(runs/'sanitize.jsonl');assert san[-1]['terminal']
 assert san[:-1]==rr[:len(san)-1]
 sanpackets=text(runs/'sanitize.packets').splitlines()
 assert sanpackets[1:]==packets[1:len(sanpackets)]
 assert san[-1]['memory_live_bytes']==0 and san[-1]['velocity_error']==0
 assert sanpackets[0].split()[4] in (str(steps),'320')
 sanitizer_equal=True
if a.horizon=='0.025':
 baseline=next(v for v in rows(workspace/'_private_workspace_artifacts/fire-fluid-early-tail-20261021/coupled-bounded-runs/early.jsonl') if v.get('tick')==160)
 reference_path=workspace/'_private_workspace_artifacts/fire-fluid-faint-reference-20261017/qualified-final/report.json'
 reference=json.loads(reference_path.read_text());tail=reference['gas_tail_checks']['selected_points']['0.025']['reference_k'];wall=reference['short_mean_wall_heat']['0.025']
 assert reference['gas_tail_checks']['point_check_qualification']['tail25-balanced']
 assert max(reference['gas_tail_checks']['tail25-balanced'].values())<=.005
 assert wall['reference_interval_relative_width']<=.005
 interval=[wall['lower_q_j'],wall['upper_q_j']]
else:
 baseline=next(v for v in rows(workspace/'_private_workspace_artifacts/fire-fluid-shear-integrated-20261020/runs/selected-full.jsonl') if v.get('tick')==640)
 reference_path=workspace/'_private_workspace_artifacts/fire-fluid-shear-integrated-20261020/report.json'
 reference=json.loads(reference_path.read_text());point=reference['early_and_endpoint_points']['0.1'];tail=point['reference_tail_k'];interval=point['physical_wall_interval_j']
new=rr[-2] if complete else rr[-1];wall_errors=[new['q_j']/v-1 for v in interval]
comparison=dict(time_s=steps/6400,baseline_tail_k=baseline['tail_k'],candidate_tail_k=new['tail_k'],reference_tail_k=tail,baseline_tail_relative_error=baseline['tail_k']/tail-1,candidate_tail_relative_error=new['tail_k']/tail-1,baseline_wall_q_j=baseline['q_j'],candidate_wall_q_j=new['q_j'],reference_wall_interval_j=interval,baseline_wall_excess_lower=baseline['q_j']/interval[1]-1,candidate_wall_relative_interval=wall_errors,tail_point_gate=abs(new['tail_k']/tail-1)<=.1,wall_point_gate=max(abs(v) for v in wall_errors)<=.1)
with localcontext() as ctx:
 ctx.prec=80;D=Decimal.from_float;energy=[[Decimal(0),Decimal(0)] for _ in range(128)];incoming=[Decimal(0)]*128
 numerator=[[Decimal(0),Decimal(0)] for _ in range(128)];denominator=[[Decimal(0),Decimal(0)] for _ in range(128)];point_fails=0;largest_failed=Decimal(0);worst_ledger=Decimal(0);negative=0
 worst_debit=worst_received=Decimal(0)
 for i,line in enumerate(packets[1:]):
  f=line.split();assert int(f[0])==5*(i+1);v=[float.fromhex(x) for x in f[1:]];assert len(v)==1280
  for j in range(128):
   q,debit,received,e0,e1,*micro=v[10*j:10*j+10];assert sum(micro)==q;negative+=q<0;incoming[j]+=D(q)
   for name,value in [('debit',debit),('received',received)]:
    residual=abs(D(value)-D(q));bound=D(32768*2**-52*abs(q))+D(64*math.ulp(q))+D(64*math.ulp(value));assert residual<=bound
    if name=='debit':worst_debit=max(worst_debit,residual)
    else:worst_received=max(worst_received,residual)
   for amount in micro:
    total=sum(energy[j])+D(amount);difference=(energy[j][0]-energy[j][1]+D(amount))/(1+Decimal('.0000125'));energy[j]=[(total+difference)/2,(total-difference)/2]
   native=[D(e0),D(e1)]
   for d in range(2):
    num=abs(native[d]-energy[j][d]);numerator[j][d]+=num;denominator[j][d]+=abs(energy[j][d])
    if energy[j][d] and num/abs(energy[j][d])>Decimal('.1'):point_fails+=1;largest_failed=max(largest_failed,abs(energy[j][d]))
   residual=abs(sum(native)-incoming[j]);bound=D(32768*2**-52)*(abs(sum(native))+abs(incoming[j]))+D(64*math.ulp(float(incoming[j])));assert residual<=bound;worst_ledger=max(worst_ledger,residual)
 local=[]
 for j in range(128):
  errors=[float(numerator[j][d]/denominator[j][d]) if denominator[j][d] else 0 for d in range(2)]
  local.append(dict(strip=j,history_error=errors,gate=[v<=.1 for v in errors]))
 solid=dict(local=local,history_failures=sum(not value for item in local for value in item['gate']),worst_history_error=max(max(item['history_error']) for item in local),point_failures=point_fails,largest_failed_reference_j=str(largest_failed),max_energy_ledger_residual_j=float(worst_ledger),max_debit_residual_j=float(worst_debit),max_received_residual_j=float(worst_received),negative_packets=negative)
result=dict(gate=.1,reference_gate=.005,comparison=comparison,solid_given_actual_q=solid,strict_sanitizer_prefix_exact=sanitizer_equal,terminal=rr[-1] if complete else None,completed=complete,accepted_steps=accepted_steps,observations=rr[:-1] if complete else rr,source_ledger_matches_baseline=new['source_j']==baseline['source_j'] if complete else None,reference_report=dict(path=str(reference_path),sha256=hashlib.sha256(reference_path.read_bytes()).hexdigest()),all_early_points_qualified=False,feedback=False,default_adoption=False)
(root/'coupled-report.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(comparison=comparison,solid_summary={k:v for k,v in solid.items() if k!='local'},terminal=result['terminal']),indent=2))
