"""Once-only controlled-fill numerical rejection and complete resource readback."""
import json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-bounded-fill1'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='4662e63459d5d15562d40c2ee684a36b1b5e18d7974114ff42fd6be4bb632b4f'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests']==8 and s['tests_passed'] and s['physical_C_prefix_exact'] and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 p=next((D/'runs').glob('*/*-receipt.json'));r=json.loads(p.read_text());assert r['returncode']==2 and r['stop_reason'] is None and r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert r['factor_build']['source_sha256']==r['source_sha256']['cfd_reference3d_bounded_fill1.c']
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['iterations']==3000 and row['target']==1e-10 and row['retained_target']==1e-11 and not row['numerically_accepted'] and row['final_residual']['true_residual']>.6
 assert not Path(r['command'][r['command'].index('--snapshot')+1]).exists();assert row['outer_iteration']['restart']==6
 a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['numeric_workspace_bytes']==32*row['preconditioner']['physical_prefix_dofs'] if 'physical_prefix_dofs' in row['preconditioner'] else a['numeric_workspace_bytes']==32*(row['condensed_free_dofs']-row['condensation']['condensed_pressure_dofs'])
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'));assert a['basis_reservation_bytes']==sum(a[k] for k in ('outer_basis_reservation_bytes','coarse_pressure_reservation_bytes','coarse_velocity_reservation_bytes'))
 pc=row['preconditioner'];pat=pc['pattern'];assert pat['pattern_blocks']<=pat['pattern_block_cap'] and pat['kept_fill']==pat['pattern_blocks']-pat['original_blocks'] and pat['construction_workspace_bound_bytes']<=384*2**20 and pc['shifted_pivots']==0
 oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'));old=json.loads(oldp.read_text());q=json.loads(Path(old['command'][old['command'].index('--output')+1]).read_text());assert row['identity']==q['identity'] and pc['rounded_values_sha256']==q['preconditioner']['rounded_values_sha256']
 files=list((R/'scripts').glob('*bounded_fill1*'))+list((R/'tests').glob('*bounded_fill1*'))+list((R/'docs').glob('cfd_3d_bounded_fill1*.md'))
 out.write_text(json.dumps(dict(status='REJECTED: six-vector full-equation stall at3000iterations',persistent_goal_complete=False,physical_accuracy_certified=False,large_or_finer_trial_permitted=False,tests_passed=8,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,full_residual=row['final_residual'],timings=row['timings'],preconditioner=pc,numeric_stage_admission=a,native_hashes_preserved=native,changed_preexisting_files=changed,original_identity_preserved=True),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
