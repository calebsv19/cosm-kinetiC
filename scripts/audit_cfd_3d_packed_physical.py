"""Seal the candidate's paired action, strict small and exact matched evidence."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
from cfd_reference3d_packed_physical_factor import work_reserve
from cfd_reference3d_packed_physical_control import control_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-packed-physical'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
 r=json.loads(p.read_text())
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 assert sha(p.parent.parent.parent/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 q=Path(r['command'][r['command'].index('--output')+1]);return r,json.loads(q.read_text()) if q.exists() else {}
def equivalent(row,old):
 assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
 diff={k:abs(row[k][0]/old[k][0]-1) if isinstance(row[k],list) else abs(row[k]/old[k]-1) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w')};assert max(diff.values())<1e-7
 assert row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<=1e-10 and row['outer_iteration']['restart']==30 and row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
 pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['constant_pressure_direction_retained'] and pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5
 f=row['preconditioner'];assert f['distributed_work_reservation_bytes']==work_reserve(f['local_factor']['solve_workspace_bytes']//32,f['coarse_factor']['dofs']) and f['coarse_factor']['solve_workspace_bytes']<=8*f['coarse_factor']['dofs']+2*2**20
 return diff
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='848077acf5616baa64484bd77f87c17f9330e6055bf8d28d4df059626fc14c86'
 changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 sources={};tests=0
 for lane,transform in (('support','transforms.json'),('matched-support','matched-transforms.json')):
  s=json.loads((D/lane/'receipt.json').read_text());tests+=s['tests_passed'];assert sha(D/lane/'tests.log')==s['test_log_sha256'] and sha(D/transform)==s['transforms_sha256']
  for q,h in s['source_sha256'].items():assert sha(R/q)==h==sha(D/lane/'frozen'/q);sources[q]=h
  for t in json.loads((D/transform).read_text()):
   assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
   for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
   v+=t.get('append','');assert v==(R/t['output']).read_text()
 assert tests==9
 p=next((D/'control-runs').glob('*/*-receipt.json'));cr,c=read(p);e=json.loads((D/'control-eligibility.json').read_text());assert sha(p)==e['control_receipt_sha256'] and c['diagnostic_accepted'] and not c['flow_field_published'] and c['diagnostic_factor_owners_retired'] and e['full_trial_permitted'];ratio=float(np.median(c['paired_batch_seconds']['native'])/np.median(c['paired_batch_seconds']['baseline']));assert c['selection']==e['selection'] and ratio==c['selection']['time_ratio']<=.95 and c['selection']['max_relative_action_difference']<=1e-10 and c['selection']['min_positive_work']>0
 with np.load(Path(cr['command'][cr['command'].index('--snapshot')+1]),allow_pickle=False) as z:np.testing.assert_array_equal(z['baseline_responses'],z['native_responses'])
 smallp=next((D/'runs').glob('*/L4-body2-quadratic-packed-physical-receipt.json'));sr,small=read(smallp);verify_receipt(smallp.resolve());oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'));oldr,old=verify_receipt(oldp.resolve());sd=equivalent(small,old);assert sr['wall_s']<=23.5018023327 and json.loads((D/'small-eligibility.json').read_text())['matched_trial_permitted']
 massp=next((D/'runs').glob('*/L4-body2-original-packed-physical-receipt.json'));mr,mass=read(massp);verify_receipt(massp.resolve());assert mass['coarse_pressure']=='mass';assert mass['final_residual']['true_residual']<=1e-10
 matchedp=next((D/'matched-runs').glob('*/*-receipt.json'));rr,row=read(matchedp);strict=bool(rr['returncode']==0 and rr['stop_reason'] is None and rr['diagnostic_failure'] is None and row.get('numerically_accepted'));md={}
 measured_factor=row.get('preconditioner') or next(z for z in rr['progress'] if z.get('phase')=='solve')['preconditioner'];complete=measured_factor['factor_storage_bytes']+measured_factor['distributed_work_reservation_bytes'];saving=873198472-complete
 assert measured_factor['distributed_work_reservation_bytes']==work_reserve(191904,87912)==57918976 and measured_factor['coarse_factor']['solve_workspace_bytes']<=8*87912+2*2**20
 if not strict:
  assert not Path(rr['command'][rr['command'].index('--snapshot')+1]).exists() and not Path(rr['command'][rr['command'].index('--output')+1]).exists()
  originalp=next((R/'build/c3d-force-resume/runs').glob('*/*-receipt.json'));_,original=verify_receipt(originalp.resolve());assert next(z for z in rr['progress'] if z.get('phase')=='assembled')['identity']==original['identity'] and measured_factor['rounded_values_sha256']==original['preconditioner']['rounded_values_sha256']
 if strict:
  verify_receipt(matchedp.resolve());op=next((R/'build/c3d-force-resume/runs').glob('*/*-receipt.json'));_,original=verify_receipt(op.resolve());md=equivalent(row,original);complete=row['preconditioner']['factor_storage_bytes']+row['preconditioner']['distributed_work_reservation_bytes'];saving=873198472-complete
 useful=bool(strict and rr['wall_s']<=106.642085 and saving>=300*2**20)
 for r,x in ((cr,c),(sr,small),(rr,row if row else {'preconditioner':measured_factor})):
  if not x or not ('factor' in x or 'preconditioner' in x):continue
  f=x.get('preconditioner',x.get('factor'));nv=f['local_factor']['solve_workspace_bytes']//32;nc=f['coarse_factor']['dofs'];np_=1248 if nv==33744 else 7104;total=basis_reservation(nv+np_,30)+reserve(nv,np_)+work_reserve(nv,nc)+(control_reserve(nv,nc) if r is cr else 0)
  a=next(z for z in r['progress'] if z.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['basis_reservation_bytes']==total and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
  a=next(z for z in r['progress'] if z.get('phase')=='fill1_symbolic_preflight');assert a['complete_work_reservation_bytes']==total and a['estimated_stage_bytes']==a['current_rss_bytes']+a['construction_workspace_bound_bytes']+32*2**20+total<=1800*2**20
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 eligibility=dict(receipt=str(matchedp),receipt_sha256=sha(matchedp),strict_field_passed=strict,finer_trial_permitted=useful,whole_wall_s=rr['wall_s'],complete_velocity_factor_work_bytes=complete,saving_bytes=saving,minimum_saving_bytes=300*2**20,whole_wall_limit_s=106.642085,force_scalar_equivalence=md,physical_accuracy_certified=False);assert not (D/'matched-eligibility.json').exists();(D/'matched-eligibility.json').write_text(json.dumps(eligibility,indent=2)+'\n')
 for q in ('scripts/audit_cfd_3d_packed_physical.py','docs/cfd_3d_packed_physical_checkpoint.md'):sources[q]=sha(R/q)
 out.write_text(json.dumps(dict(status='USEFUL MATCHED FIELD: finer trial earned' if useful else 'SMALL USEFUL; MATCHED STRICT FIELD OR USEFULNESS STILL OPEN',persistent_goal_complete=False,tests_passed=tests,source_sha256=sources,predecessor_sha256=pre['sha256'],selection=c['selection'],small_receipt=str(smallp),small_receipt_sha256=sha(smallp),small_wall_s=sr['wall_s'],small_final_residual=small['final_residual'],small_force_scalar_equivalence=sd,default_mass_receipt_sha256=sha(massp),matched=eligibility,matched_final_residual=row.get('final_residual'),matched_iterations=row.get('iterations'),matched_diagnostic_failure=rr['diagnostic_failure'],matched_owned_peak_mib=max(z.get('peak_rss_bytes',0) for z in rr['progress'])/2**20,matched_sampled_peak_mib=rr['peak_observed_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
