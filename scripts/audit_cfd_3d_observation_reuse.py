#!/usr/bin/env python3
"""Once-only equation/source/equality/resource and physical-force evidence audit."""
import hashlib,json
from pathlib import Path
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt,force_comparison
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-observation-reuse';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def observer(p):
 r=json.loads(p.read_text());assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180
 assert r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*1024**2
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(p.parent/'source'/q)==h==sha(R/'scripts'/q)
 assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 a=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert a['peak_rss_bytes']<1800*1024**2 and a['wall_s']<180
 return r,a

def main():
 out=D/'checkpoint-audit.json';assert not out.exists()
 predecessor=json.loads((D/'predecessor.json').read_text());assert sha(Path(predecessor['path']))==predecessor['sha256']=='7392f731d911c88a7957d7ea8936b189cf0db3950e688b82b61e19700afd9d8d'
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
 c=json.loads((D/'source-transform-control.json').read_text());parts=[]
 for t in c['parts']:
  p=R/t['parent'];assert sha(p)==t['parent_sha256'];s=t['start']+p.read_text().split(t['start'],1)[1]
  if t['end'] is not None:s=s.split(t['end'],1)[0]
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  parts.append(s)
 assert c['header']+parts[0]+'\n\n'+parts[1]==(R/c['output']).read_text() and sha(R/c['output'])==c['output_sha256']
 for t in json.loads((D/'solver-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==1
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'support-tests-01.log')==support['log_sha256'] and '\nOK\n' in (D/'support-tests-01.log').read_text()
 assert sha(D/'source-transform-control.json')==support['transformation_sha256']
 costp=next((D/'observer-runs').glob('*/L4-second-normal-old-vs-fused-receipt.json'));r,cost=observer(costp)
 assert r['returncode']==0 and cost['diagnostic_accepted'] and cost['bitwise_observation_equality'] and cost['source_field_preserved'] and not cost['numerical_field_published']
 assert cost['cost_ratio']<1 and abs(cost['old_combined_s']-cost['old_metrics_s']-cost['old_consistency_s'])<1e-12
 assert sha(Path(cost['input_receipt']))==cost['input_receipt_sha256'];verify_receipt(Path(cost['input_receipt']))
 receipts={str(costp):sha(costp)};fields={};measurements={}
 for p in sorted((D/'runs').glob('*/L4-*-receipt.json')):
  r,a=parent.accepted(p,6);prior=next((R/'build/c3d-restart-six/runs').glob('*/'+p.name));br,b=verify_receipt(prior)
  assert a['identity']==b['identity'] and a['preconditioner']['rounded_values_sha256']==b['preconditioner']['rounded_values_sha256']
  receipts[str(p)]=sha(p);fields[str(p)]=a;measurements[p.name]=dict(whole_wall_s=r['wall_s'],owned_peak_rss_bytes=a['peak_rss_bytes'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],iterations=a['iterations'],full_residual=a['final_residual'],force_comparison=force_comparison(a,b))
 assert len(fields)==2
 cal=json.loads((D/'calibration-comparisons.json').read_text())
 for p,h in cal['input_sha256'].items():assert sha(Path(p))==h
 stages={};larger={};observers={}
 for p in sorted((D/'stage-runs').glob('*/*-receipt.json')):
  r,a=parent.frozen(p);assert r['returncode']==0 and a['diagnostic_accepted'] and not a['numeric_factor_attempted'] and not a['numerical_field_published']
  assert a['symbolic_handle_cleanup_verified'] and a['original_mixed_input_preserved'] and a['tetrahedra']==33216
  parent.budget(a['admission'],a['condensation']['reduced_free_dofs'],6)
  old=json.loads(next((R/'build/c3d-restart-six/stage-runs').glob('*/L8*-stage.json')).read_text());assert a['identity']==old['identity']
  stages[str(p)]=a['admission'];receipts[str(p)]=sha(p)
 for p in sorted((D/'runs').glob('*/L8-*-receipt.json')):
  r,a=verify_receipt(p);receipts[str(p)]=sha(p)
  if a is not None:parent.frozen(p)
  else:
   for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h
  if r['returncode']==0:
   parent.accepted(p,6);assert a['tetrahedra']==33216
   stage=json.loads(next((D/'stage-runs').glob('*/L8*-stage.json')).read_text());assert a['identity']==stage['identity']
  larger[str(p)]=dict(returncode=r['returncode'],whole_wall_s=r['wall_s'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],stop_reason=r['stop_reason'],row=a)
 for p in sorted((D/'observer-runs').glob('*/*-receipt.json')):
  if p==costp:continue
  r,a=observer(p);receipts[str(p)]=sha(p)
  assert r['returncode']==0 and a['diagnostic_accepted'] and a['original_linear_residual']['true_residual']<1e-10
  assert sha(Path(a['input_receipt']))==a['input_receipt_sha256'] and sha(Path(a['signed_attribution_path']))==a['signed_attribution_sha256']
  error=max(abs(v) for l in a['lifts'] for part in ('pressure','viscous') for v in l[part]['identity_error_n']);assert error<1e-9
  observers[str(p)]=dict(maximum_identity_error_n=error,volume_stress=a['volume_strong_equilibrium_defect_l2'],interior_jump=a['interior_stress_jump_l2'])
 comparisons=json.loads((D/'comparisons.json').read_text())
 for p,h in comparisons['input_sha256'].items():assert sha(Path(p))==h
 for p,c in comparisons.get('matched_force_comparisons',{}).items():
  r,a=verify_receipt(Path(p));br,b=verify_receipt(Path(c['base_receipt']));assert c['force_comparison']==force_comparison(a,b)
 if comparisons.get('L8_normal_refinement'):
  c=comparisons['L8_normal_refinement'];br,b=verify_receipt(Path(c['base_receipt']));a=next(v['row'] for v in larger.values() if v['returncode']==0);assert c['force_comparison']==force_comparison(a,b)
 sources=list((R/'scripts').glob('*observation_reuse*.py'))+list((R/'scripts').glob('*fused_observation*.py'))+[R/'tests/test_cfd_reference3d_fused_observation.py',R/'docs/cfd_3d_observation_reuse_checkpoint.md',R/'docs/cfd_3d_observation_reuse_next_goal.md']
 result=dict(status=comparisons['status'],persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,default_restart_changed=False,test_count=1,predecessor_sha256=predecessor['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,source_sha256={str(p.relative_to(R)):sha(p) for p in sources},support_receipt_sha256=sha(D/'support-test-receipt.json'),source_transformation_sha256=sha(D/'source-transform-control.json'),solver_transformation_sha256=sha(D/'solver-transform-control.json'),receipt_sha256=receipts,cost_measurement=cost,calibration=measurements,comparison_sha256=sha(D/'comparisons.json'),calibration_comparison_sha256=sha(D/'calibration-comparisons.json'),matched_stages=stages,larger=larger,observers=observers,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
