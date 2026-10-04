"""Once-only local Galerkin and failed full numerical reference readback."""
import json,hashlib
from pathlib import Path
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_distributed_p3 import work_reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-distributed-p3'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='a1d24dfa96a6cee80bdd2ed60cdf1c0dd867c4d1d52cb0d9926825c854918c5b'
 baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==8 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 assert sha(R/'scripts/cfd_reference3d_distributed_p3_coarse.c')==sha(R/'scripts/cfd_reference3d_encoded_storage.c')==s['coarse_C_exact_parent_sha256'];assert sha(D/'assembly-transform-control.json')==s['assembly_transform_sha256']
 t=json.loads((D/'assembly-transform-control.json').read_text());x=(R/t['parent']).read_text();assert sha(R/t['parent'])==t['parent_sha256']
 for a,b in t['literal_replacements']:assert a in x;x=x.replace(a,b)
 assert x==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 p=next((D/'runs').glob('*/L4-body2-original-distributed-p3-auto-receipt.json'));r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert r['factor_build']['source_sha256']==r['source_sha256']['cfd_reference3d_bounded_fill1.c'];assert r['coarse_factor_build']['source_sha256']==r['source_sha256']['cfd_reference3d_distributed_p3_coarse.c'] and r['coarse_factor_build']['value_dtype']=='float32' and r['coarse_factor_build']['create_abi_arguments']==7 and r['coarse_factor_build']['scalar_block_size']==1
 row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert row['iterations']<3000 and row['target']==1e-10 and row['retained_target']==1e-11 and row['numerically_accepted'] and row['final_residual']['true_residual']<1e-10 and row['outer_iteration']['restart']==30
 assert Path(r['command'][r['command'].index('--snapshot')+1]).exists()
 from audit_cfd_3d_spatial import verify_receipt
 verify_receipt(p.resolve())
 oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'));oldr=json.loads(oldp.read_text());old=json.loads(Path(oldr['command'][oldr['command'].index('--output')+1]).read_text());assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']
 pc=row['preconditioner'];nc=pc['coarse_factor']['dofs'];nv=row['condensed_free_dofs']-row['condensation']['condensed_pressure_dofs'];np_=row['condensation']['condensed_pressure_dofs'];a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['local_factor_storage_bound_bytes']==72*pc['local_factor']['pattern']['pattern_blocks']+1024 and a['coarse_factor_storage_bytes']==pc['coarse_factor']['symbolic_factor_storage_bytes'];assert a['factor_storage_bytes']==a['local_factor_storage_bound_bytes']+a['coarse_factor_storage_bytes'] and a['numeric_workspace_bytes']==max(32*nv,pc['coarse_factor']['numeric_workspace_bytes']) and a['reserve_bytes']==32*2**20
 assert a['outer_basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],30) and a['coarse_pressure_reservation_bytes']==reserve(nv,np_) and a['distributed_velocity_work_reservation_bytes']==work_reserve(nv,nc);assert a['basis_reservation_bytes']==sum(a[k] for k in ('outer_basis_reservation_bytes','coarse_pressure_reservation_bytes','distributed_velocity_work_reservation_bytes'));assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
 cm=row['condensation']['distributed_p3'];assert cm['maximum_local_columns']<=60 and cm['maximum_local_temporary_bytes']<=2*2**20 and cm['maximum_projected_sparse_construction_bytes']<=256*2**20 and cm['physical_upper_orientation_retained'];assert max(pc['distributed_velocity']['sampled_coarse_action_relative_errors'])<=1e-10 and max(pc['distributed_velocity']['sampled_coarse_reproduction_relative_errors'])<=1e-8
 changes={k:abs(row[k][0]/old[k][0]-1) if isinstance(row[k],list) else abs(row[k]/old[k]-1) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w')};assert max(changes.values())<1e-7
 assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
 assert r['wall_s']>2*oldr['wall_s']
 invalid=next((D/'runs').glob('*/L4-body2-original-distributed-p3-receipt.json'));verify_receipt(invalid.resolve());ir=json.loads(invalid.read_text());assert ir['returncode']==2 and not ir['progress'] and 'invalid choice' in Path(ir['command'][ir['command'].index('--output')+1]).with_suffix('.log').read_text()
 files=list((R/'scripts').glob('*distributed_p3*'))+list((R/'tests').glob('*distributed_p3*'))+list((R/'docs').glob('cfd_3d_distributed_p3*.md'))
 out.write_text(json.dumps(dict(status='STRICT NUMERICAL PASS, USEFULNESS REJECTED: whole cost exceeds prospective limit',persistent_goal_complete=False,physical_accuracy_certified=False,large_or_finer_trial_permitted=False,tests_passed=8,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,full_residual=row['final_residual'],timings=row['timings'],coarse_assembly=cm,preconditioner=pc,numeric_stage_admission=a,native_hashes_preserved=native,changed_preexisting_files=changed,original_physical_identity_preserved=True,force_scalar_equivalence=changes,numerical_field_published=True,usefulness_whole_limit_s=2*oldr['wall_s'],invalid_precomputation_receipt_sha256=sha(invalid)),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
