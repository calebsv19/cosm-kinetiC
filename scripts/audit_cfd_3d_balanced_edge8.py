"""Once-only rejection of all declared six-interval profiles before CFD solve."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_balanced_edge8_mesh import evaluate,SPACINGS
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-balanced-edge8'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='37917f4bcb4244bc2b9f9c06f1a4d3e17753f10147a2719ff6b05f2fd4b31b49'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in protected.items():assert sha(R/p)==h
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4 and sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text()
 for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
 assert sha(D/'source-transform-control.json')==support['transformation_sha256']
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
  assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 e=json.loads((D/'survey-eligibility.json').read_text());assert e['survey_eligible'] and e['matched_domain_numerical_readiness_verified'] and e['attempts_declared']==1 and e['geometry_profiles_declared']==2 and e['domain_lengths']==[4,8]
 for p,h in e['input_sha256'].items():assert sha(Path(p))==h
 ps=list((D/'survey-runs').glob('*/*-receipt.json'));assert len(ps)==1;p=ps[0];r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['wall_s']<180 and r['peak_observed_rss_bytes']<1800*2**20
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and not row['numerically_accepted'] and not row['physical_accuracy_certified'] and not row['numeric_factor_attempted'] and not row['numerical_field_published'] and row['selected_case'] is None and row['geometry_path'] is None
 assert not Path(r['command'][r['command'].index('--geometry')+1]).exists() and len(row['candidates'])==2 and [c['spacing_m'] for c in row['candidates']]==list(SPACINGS)
 for c in row['candidates']:
  bad=[]
  for length in (4.,8.):
   q=c['domains'][str(length)];reasons=evaluate(row['original_quality'][str(length)],q['quality'],length,c['spacing_m']);assert reasons==q['reasons'] and q['geometry_accepted']==(not reasons);bad.extend([str(length)+': '+s for s in reasons])
  assert bad==c['reasons'] and bad and not c['geometry_accepted_on_both_lengths']
 files=list((R/'scripts').glob('*balanced_edge8*.py'))+[R/'tests/test_cfd_reference3d_balanced_edge8.py']+list((R/'docs').glob('cfd_3d_balanced_edge8*.md'))
 out.write_text(json.dumps(dict(status='PROGRESS: balanced eight-interval profiles rejected by outer body band; no factor or field',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p)},native_hashes_preserved=protected,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,support_receipt_sha256=sha(D/'support-test-receipt.json'),survey=row,committed=False,packaged=False,installed=False),indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out))))
if __name__=='__main__':main()
