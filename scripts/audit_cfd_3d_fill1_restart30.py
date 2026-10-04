"""Once-only larger-basis control with unchanged physical and PC provenance."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_bounded_fill1 import velocity_reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-fill1-restart30'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='ec56241628f48e539e41501e9acb2da22ed23404a2bb7a5bba314738d7228bf0'
 pa=json.loads(Path(pre['path']).read_text())
 for p,h in pa['source_sha256'].items():assert sha(R/p)==h
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 for t in json.loads((D/'source-transform-control.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'];x=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in x;x=x.replace(a,b)
  assert x==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 p=next((D/'runs').glob('*/*-receipt.json'));r=json.loads(p.read_text());assert r['returncode']==2 and r['stop_reason'] is None and r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert r['factor_build']['source_sha256']==r['source_sha256']['cfd_reference3d_bounded_fill1.c']
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['iterations']==3000 and row['target']==1e-10 and row['retained_target']==1e-11 and not row['numerically_accepted'] and row['final_residual']['true_residual']>1e-10
 assert not Path(r['command'][r['command'].index('--snapshot')+1]).exists();assert row['outer_iteration']['restart']==30
 oldr=json.loads(Path(pa['receipt']).read_text());old=json.loads(Path(oldr['command'][oldr['command'].index('--output')+1]).read_text());assert row['identity']==old['identity'];pc=row['preconditioner'];opc=old['preconditioner']
 for k in ('pattern','rounded_values_sha256','fill_compensated_pairs','fill_diagonal_compensation_sum','fill_diagonal_compensation_max','shifted_pivots','minimum_factor_pivot','factor_storage_bytes','velocity_coarse'):assert pc[k]==opc[k]
 flex=pc['flexible_iteration'];assert flex['restart']==30 and flex['basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],30) and flex['basis_array_bytes']<flex['basis_reservation_bytes']
 a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');nv=row['condensed_free_dofs']-row['condensation']['condensed_pressure_dofs'];np_=row['condensation']['condensed_pressure_dofs'];assert a['numeric_stage_admitted'] and a['outer_basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],30) and a['coarse_pressure_reservation_bytes']==reserve(nv,np_) and a['coarse_velocity_reservation_bytes']==velocity_reserve(nv) and a['numeric_workspace_bytes']==32*nv and a['reserve_bytes']==32*2**20
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'));assert a['basis_reservation_bytes']==sum(a[k] for k in ('outer_basis_reservation_bytes','coarse_pressure_reservation_bytes','coarse_velocity_reservation_bytes'))
 files=list((R/'scripts').glob('*fill1_restart30*'))+list((R/'tests').glob('*fill1_restart30*'))+list((R/'docs').glob('cfd_3d_fill1_restart30*.md'))
 out.write_text(json.dumps(dict(status='REJECTED: longer restart improves residual but misses strict full target/cost',persistent_goal_complete=False,physical_accuracy_certified=False,large_or_finer_trial_permitted=False,tests_passed=4,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,full_residual=row['final_residual'],old_full_residual=old['final_residual'],residual_improvement_factor=old['final_residual']['true_residual']/row['final_residual']['true_residual'],timings=row['timings'],basis=flex,numeric_stage_admission=a,native_hashes_preserved=native,changed_preexisting_files=changed,original_identity_and_preconditioner_preserved=True),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
