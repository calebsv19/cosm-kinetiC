"""Once-only accepted geometry and safely rejected numerical memory admission."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_floor_balanced8_mesh import evaluate,SPACINGS
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_flexible import basis_reservation
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-floor-balanced8'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def receipt(p,success):
 r=json.loads(p.read_text());assert r['returncode']==(0 if success else 2) and r['stop_reason'] is None and r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*2**20 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['mesh_cap']==50000
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
 return r,json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text())
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='b6ab7937be491631b87e8a7a13b20606ef490ec177af1f698d820d3dfd417415'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4 and sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text()
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text())+[json.loads((D/'numeric-supervisor-transform.json').read_text())]:
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 e=json.loads((D/'survey-eligibility.json').read_text());assert e['survey_eligible'] and e['matched_domain_numerical_readiness_verified'] and e['attempts_declared']==1 and e['geometry_profiles_declared']==1 and e['domain_lengths']==[4,8]
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 ps=list((D/'survey-runs').glob('*/*-receipt.json'));assert len(ps)==1;sp=ps[0];sr,survey=receipt(sp,True)
 assert survey['diagnostic_accepted'] and not survey['numerically_accepted'] and not survey['physical_accuracy_certified'] and not survey['numeric_factor_attempted'] and not survey['numerical_field_published'] and survey['selected_case'] is not None and len(survey['candidates'])==1
 assert survey['selected_case']==survey['candidates'][0] and survey['selected_case']['spacing_m']==SPACINGS[0] and survey['selected_case']['geometry_accepted_on_both_lengths'] and not survey['selected_case']['reasons']
 gp=Path(survey['geometry_path']);assert sha(gp)==survey['geometry_sha256']
 for length,q in survey['selected_case']['domains'].items():assert evaluate(survey['original_quality'][length],q['quality'],float(length),SPACINGS[0])==q['reasons']==[] and q['geometry_accepted']
 e=json.loads((D/'L4-numeric-eligibility.json').read_text());assert e['selected_geometry_eligible'] and e['dual_domain_all_geometry_gates_passed'] and e['attempts_declared']==1 and e['selected_spacing_m']==SPACINGS[0]
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 nps=list((D/'numeric-runs').glob('*/*-receipt.json'));assert len(nps)==1;np_=nps[0];nr,numeric=receipt(np_,False);verify_receipt(np_)
 assert nr['diagnostic_failure']['kind']=='resource_cap' and numeric['numerical_failure_reasons']==['resources'] and numeric['iterations']==0 and numeric['final_residual'] is None and not numeric['numerically_accepted']
 a=next(q for q in nr['progress'] if q.get('phase')=='numeric_stage_admission');m=next(q for q in nr['progress'] if q.get('phase')=='assembled');n=m['reduced_free_dofs'];npres=m['condensed_pressure_dofs'];assert m['full_pressure_dofs']==860160 and m['block_storage']['velocity']['full_mixed_action_bitwise_preserved'] and m['block_storage']['velocity']['encoding']['bitwise_roundtrip_verified']
 assert a['outer_basis_reservation_bytes']==basis_reservation(n,6) and a['coarse_pressure_reservation_bytes']==reserve(n-npres,npres) and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and not a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']>1800*2**20 and a['residency_measurement'].startswith('fresh at stage admission')
 phases=[q.get('phase') for q in nr['progress']];assert 'workspace_numeric_ready' not in phases and 'solve' not in phases and 'publication_complete' not in phases and not Path(nr['command'][nr['command'].index('--snapshot')+1]).exists()
 geom=next(q for q in nr['progress'] if q.get('phase')=='selected_geometry_verified');assert geom['geometry_sha256']==sha(gp) and geom['tetrahedra']==43008 and geom['body_intervals']==8
 for q,h in nr['artifact_sha256'].items():assert sha(Path(q))==h
 original=next((R/'build/c3d-force-resume/runs').glob('*/*-receipt.json'));orr,old=verify_receipt(original);assert m['identity']['mesh_sha256']!=old['identity']['mesh_sha256']
 comp=dict(dual_domain_geometry_accepted=True,numerical_field_published=False,physical_force_test_completed=False,physical_accuracy_certified=False,original_force_gate_unchanged=True,original_resource_cap_bytes=1800*2**20,L4_numeric_stage_admission=a,L4_required_recovery_mib=a['estimated_numeric_stage_bytes']/2**20-1800,L4_numerical_factor_attempted=False,L4_iterations=0,L8_numeric_attempted=False,survey_wall_s=sr['wall_s'],L4_child_wall_s=nr['wall_s'],stage_resource_rejection_not_actual_RSS_overshoot=True)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(comp,indent=2)+'\n')
 files=list((R/'scripts').glob('*floor_balanced8*.py'))+[R/'tests/test_cfd_reference3d_floor_balanced8.py']+list((R/'docs').glob('cfd_3d_floor_balanced8*.md'))
 out.write_text(json.dumps(dict(status='PROGRESS: fine geometry passes; exact velocity factor memory admission rejected before solve',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p) for p in (sp,np_,original)},geometry_sha256=sha(gp),native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),comparison_sha256=sha(cp),comparisons=comp,survey=survey,committed=False,packaged=False,installed=False),indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),physical_force_test_completed=False)))
if __name__=='__main__':main()
