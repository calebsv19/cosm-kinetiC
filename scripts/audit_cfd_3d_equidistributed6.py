"""Once-only paired geometry rejection; no numerical field or factor allowed."""
import json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-equidistributed6'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='9dedc491a1d78561c2bfa996e42a7a9213ef4531a5261b76e319ae58bd7c19b4'
 base=json.loads((D/'baseline.json').read_text());changed=[p for p,h in base.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 p=next((D/'survey-runs').glob('*/*-receipt.json'));r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and row['selected_case'] is None and row['geometry_sha256'] is None and not row['numeric_factor_attempted'] and not row['numerical_field_published']
 assert len(row['candidates'])==1 and not row['candidates'][0]['geometry_accepted_on_both_lengths']
 for L,x in row['candidates'][0]['domains'].items():assert not x['geometry_accepted'] and x['reasons'] and x['quality']['tetrahedra']==(28416 if float(L)==4 else 33216)
 files=list((R/'scripts').glob('*equidistributed6*'))+list((R/'tests').glob('*equidistributed6*'))+list((R/'docs').glob('cfd_3d_equidistributed6*.md'))
 out.write_text(json.dumps(dict(status='REJECTED: paired local mesh-conditioning gates',persistent_goal_complete=False,physical_accuracy_certified=False,numerical_trial_permitted=False,numeric_factor_attempted=False,tests_passed=4,predecessor_sha256=pre['sha256'],receipt=str(p),receipt_sha256=sha(p),wall_s=r['wall_s'],sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,candidates=row['candidates'],originals=row['original_quality'],native_hashes_preserved=native,changed_preexisting_files=changed,source_sha256={str(p.relative_to(R)):sha(p) for p in files}),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
