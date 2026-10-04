import hashlib,json
from pathlib import Path
from cfd_reference3d_balanced_body_mesh import evaluate
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-balanced-body'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();p=json.loads((D/'predecessor.json').read_text());assert sha(Path(p['path']))==p['sha256']=='d1300537f12848ee20f155ba42bad82c147dc403626c0374c04a7eaeb1e3a254'
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for f,h in native.items():assert sha(R/f)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[f for f,h in baseline.items() if sha(R/f)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4
 for f,h in support['source_sha256'].items():assert sha(R/f)==h==sha(Path(support['frozen_source'])/Path(f).name)
 assert sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text();assert sha(D/'support-tests-01.log')==support['preserved_initial_failure_log_sha256']
 rp=next((D/'survey-runs').glob('*/*-receipt.json'));r=json.loads(rp.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
 assert r['mesh_cap']==50000 and r['wall_cap_s']==180 and r['rss_cap_bytes']==1800*2**20 and r['input_solver_iteration_cap']==3000
 for f,h in r['source_sha256'].items():assert sha(R/'scripts'/f)==h==sha(rp.parent/'source'/f)
 for f,h in r['artifact_sha256'].items():assert sha(Path(f))==h
 assert rp.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest();assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and row['original_saved_mesh_rebuilt_bitwise'];assert not row['numeric_factor_attempted'] and not row['numerical_field_published'] and not row['numerically_accepted'] and not row['physical_accuracy_certified']
 assert row['selected_case'] is None and row['geometry_path'] is None and not Path(r['command'][r['command'].index('--geometry')+1]).exists();assert len(row['candidates'])==8 and row['wall_s']<180 and row['peak_rss_bytes']<1800*2**20 and r['peak_observed_rss_bytes']<1800*2**20
 for c in row['candidates']:
  assert not c['geometry_accepted'] and c['reasons']==evaluate(row['original_quality'],c['quality']) and c['reasons'];assert not c['original_macro_partition_claimed'] and not c['original_tetrahedron_partition_claimed'] and not c['body_surface_triangles_preserved']
 assert sha(Path(row['input_receipt']))==row['input_receipt_sha256']
 sources=list((R/'scripts').glob('*balanced_body*.py'))+[R/'tests/test_cfd_reference3d_balanced_body.py']+list((R/'docs').glob('cfd_3d_balanced_body*.md'))
 a=dict(status='PROGRESS: eight predeclared actual body grids rejected before factor; pressure correction next',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=p['sha256'],source_sha256={str(f.relative_to(R)):sha(f) for f in sources},receipt_sha256={str(rp):sha(rp)},support_receipt_sha256=sha(D/'support-test-receipt.json'),baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=native,candidate_rejections={c['name']:c['reasons'] for c in row['candidates']},numeric_factor_attempted=False,numerical_field_published=False,committed=False,packaged=False,installed=False)
 out.write_text(json.dumps(a,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out))))
if __name__=='__main__':main()
