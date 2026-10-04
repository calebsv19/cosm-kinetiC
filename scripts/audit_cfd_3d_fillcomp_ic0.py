"""Once-only numerical rejection and resource/input readback."""
import json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-fillcomp-ic0'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='7247f191fb37a1a504d20728efb8021e0bfa83ca26536fd681b63f6f6a097209'
 base=json.loads((D/'baseline.json').read_text());changed=[p for p,h in base.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests']==6 and s['tests_passed'] and s['physical_C_prefix_exact']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 assert sha(D/'support/tests.log')==s['test_log_sha256']
 p=next((D/'runs').glob('*/*-receipt.json'));r=json.loads(p.read_text())
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 assert r['factor_build']['source_sha256']==r['source_sha256']['cfd_reference3d_fillcomp_ic0.c']
 assert r['returncode']==2 and r['stop_reason'] is None and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['mesh_cap']==50000 and r['linear_iteration_cap']==3000
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['iterations']==3000 and not row['numerically_accepted'] and row['final_residual']['true_residual']>.7 and row['target']==1e-10 and row['retained_target']==1e-11
 assert not Path(r['command'][r['command'].index('--snapshot')+1]).exists();assert row['peak_rss_bytes']<=1800*2**20
 a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'];assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'));assert a['basis_reservation_bytes']==sum(a[k] for k in ('outer_basis_reservation_bytes','coarse_pressure_reservation_bytes','coarse_velocity_reservation_bytes'))
 oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'));old=json.loads(oldp.read_text());oldrow=json.loads(Path(old['command'][old['command'].index('--output')+1]).read_text());assert row['identity']==oldrow['identity'] and row['preconditioner']['rounded_values_sha256']==oldrow['preconditioner']['rounded_values_sha256']
 pc=row['preconditioner'];assert pc['shifted_pivots']==0 and pc['fill_compensated_pairs']>0 and pc['velocity_coarse']['coarse_reproduction_relative_error']<1e-8
 files=list((R/'scripts').glob('*fillcomp_ic0*'))+list((R/'tests').glob('*fillcomp_ic0*'))+list((R/'docs').glob('cfd_3d_fillcomp_ic0*.md'))
 out.write_text(json.dumps(dict(status='REJECTED:3000iteration cap, momentum dominated full-equation stall',persistent_goal_complete=False,physical_accuracy_certified=False,general_or_native_adopted=False,large_control_permitted=False,finer_mesh_trial_permitted=False,tests_passed=6,predecessor_sha256=pre['sha256'],receipt=str(p),receipt_sha256=sha(p),wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,full_residual=row['final_residual'],numeric_stage_admission=a,preconditioner=pc,original_physical_identity_preserved=True,native_hashes_preserved=native,changed_preexisting_files=changed,source_sha256={str(p.relative_to(R)):sha(p) for p in files}),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
