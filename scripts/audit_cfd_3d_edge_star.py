#!/usr/bin/env python3
"""Once-only audit of geometry rejection, exact source/field preservation and caps."""
import json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-edge-star'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists()
 predecessor=json.loads((D/'predecessor.json').read_text());assert sha(Path(predecessor['path']))==predecessor['sha256']=='3aa3b3b178ba8037b1cd4c3e49f53904135b948da375ba3a17a8ad6c408f57e3'
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'support-tests-01.log')==support['log_sha256'] and '\nOK\n' in (D/'support-tests-01.log').read_text()
 p=next((D/'survey-runs').glob('*/*-receipt.json'));r=json.loads(p.read_text())
 assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
 assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180 and r['input_solver_iteration_cap']==3000
 assert r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*1024**2
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(p.parent/'source'/q)==h==sha(R/'scripts'/q)
 assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 a=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert a['diagnostic_accepted'] and a['original_saved_mesh_rebuilt_bitwise']
 assert not a['numeric_factor_attempted'] and not a['numerical_field_published'] and not a['numerically_accepted'] and not a['physical_accuracy_certified']
 assert a['selected_case'] is None and a['geometry_path'] is None and a['geometry_sha256'] is None
 assert not Path(r['command'][r['command'].index('--geometry')+1]).exists() and len(a['candidates'])==16
 reasons_summary={}
 for c in a['candidates']:
  assert not c['geometry_accepted'];m=c['metadata'];old,new=m['affected_original'],m['affected_refined'];og,ng=m['global_original'],m['global_refined'];reasons=[]
  if m['refined_tetrahedra']>50000:reasons.append('mesh cap')
  if not m['all_reflections_and_yz_exchange_preserved']:reasons.append('reflection/y-z symmetry')
  for key in ('worst_shape','max_condition'):
   if ng[key]>og[key]*(1+1e-8):reasons.append('global '+key+' worsened')
  for key in ('worst_shape','weighted_mean_shape','max_condition'):
   if new[key]>old[key]*(1+1e-8):reasons.append('affected '+key+' worsened')
  if new['weighted_mean_shape']>old['weighted_mean_shape']*.99:reasons.append('affected weighted mean improvement below1%')
  assert reasons==c['reasons'] and reasons
  assert m['macro_parent_partition_verified'] and not m['original_alfeld_tet_partition_claimed'] and m['physical_cube_and_domain_preserved']
  assert abs(old['volume_m3']-new['volume_m3'])<1e-12 and abs(og['volume_m3']-31)<1e-9 and abs(ng['volume_m3']-31)<1e-9
  assert m['boundary_areas_m2']==dict(body=6.,inlet=4.,outlet=4.,walls=64.)
  reasons_summary[f"rank{c['rank']}-{c['method']}"]=dict(refined_tetrahedra=m['refined_tetrahedra'],quality_ratios={k:new[k]/old[k] for k in ('worst_shape','weighted_mean_shape','max_condition')},reasons=reasons)
 for key in ('input_observer_receipt','input_receipt'):assert sha(Path(a[key]))==a[key+'_sha256']
 sources=list((R/'scripts').glob('*edge_star*.py'))+[R/'tests/test_cfd_reference3d_edge_star.py',R/'docs/cfd_3d_edge_star_goal.md',R/'docs/cfd_3d_edge_star_checkpoint.md',R/'docs/cfd_3d_edge_star_next_goal.md']
 result=dict(status='PROGRESS: sixteen actual signed-edge geometry candidates rejected before factor/field; continue independent empty-duct baseline',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=predecessor['sha256'],test_count=4,support_receipt_sha256=sha(D/'support-test-receipt.json'),baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,source_sha256={str(p.relative_to(R)):sha(p) for p in sources},receipt_sha256={str(p):sha(p)},geometry_rejections=reasons_summary,numeric_factor_attempted=False,numerical_field_published=False,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
