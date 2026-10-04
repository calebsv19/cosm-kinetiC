"""Once-only full strict field and fixed pressure/cost readback."""
import json,hashlib
from pathlib import Path
from audit_cfd_3d_spatial import verify_receipt
from cfd_reference3d_p3_cg8_scalar_pressure import work_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-p3-cg8-scalar'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='f630e064f273f941d014cf3a20cbb878537db1ccf1459d5965b2b12d85bf430a'
 changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==3 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(Path(s['reused_pressure_support_path']))==s['reused_pressure_support_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 t=json.loads((D/'factor-transform-control.json').read_text());assert sha(D/'factor-transform-control.json')==s['factor_transform_sha256'];x=(R/t['parent']).read_text();assert sha(R/t['parent'])==t['parent_sha256']
 for a,b in t['literal_replacements']:assert a in x;x=x.replace(a,b)
 assert x==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 p=next((D/'runs').glob('*/*-receipt.json')).resolve();r,row=verify_receipt(p);assert r['returncode']==0 and row['target']==1e-10 and row['retained_target']==1e-11 and row['outer_iteration']['restart']==30 and row['final_residual']['true_residual']<=1e-10
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h
 assert r['coarse_factor_build']['scalar_action_abi_arguments']==6 and r['coarse_factor_build']['encoded_parent_sha256']==s['encoded_C_prefix_sha256']
 assert r['coarse_factor_build']['value_dtype']=='float32' and r['coarse_factor_build']['create_abi_arguments']==7 and r['coarse_factor_build']['scalar_block_size']==1
 oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json')).resolve();oldr,old=verify_receipt(oldp);assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256'];changes={k:abs(row[k][0]/old[k][0]-1) if isinstance(row[k],list) else abs(row[k]/old[k]-1) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w')};assert max(changes.values())<1e-7
 pc=row['preconditioner'];assert pc['local_inner_iteration_cap']==8 and pc['coarse_fixed_residual_steps']==3;pp=row['pressure_preconditioner'];assert pp['coarse_relative_skew']<1e-5 and pp['coarse_reproduction_relative_error']<1e-5 and pp['columns']==10 and pp['constant_pressure_direction_retained']
 assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
 np_=row['condensation']['condensed_pressure_dofs'];nv=row['condensed_free_dofs']-np_;nc=pc['coarse_factor']['dofs'];a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['outer_basis_reservation_bytes']==basis_reservation(nv+np_,30) and a['coarse_pressure_reservation_bytes']==reserve(nv,np_) and a['distributed_velocity_work_reservation_bytes']==work_reserve(nv,nc)
 assert a['factor_storage_bytes']==a['local_factor_storage_bound_bytes']+a['coarse_factor_storage_bytes'] and a['numeric_workspace_bytes']==max(32*nv,pc['coarse_factor']['numeric_workspace_bytes']) and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['reserve_bytes']==32*2**20
 assert r['wall_s']>2*oldr['wall_s']
 prefix=(R/'scripts/cfd_reference3d_encoded_storage.c').read_bytes();assert (R/'scripts/cfd_reference3d_p3_coarse_scalar.c').read_bytes().startswith(prefix) and len(prefix)==s['encoded_C_prefix_bytes'] and sha(R/'scripts/cfd_reference3d_encoded_storage.c')==s['encoded_C_prefix_sha256']
 b=json.loads((D/'support/action-benchmark.json').read_text());br=json.loads((D/'support/benchmark-readback.json').read_text());assert sha(D/'support/action-benchmark.json')==br['benchmark_sha256'] and sha(D/'support/benchmark.log')==br['log_sha256'] and b['relative_action_error']<=1e-12 and b['median_native_to_scipy']<=.8 and b['action_speed_gate_passed'] and b['coarse_input_sha256']==pc['coarse_factor']['physical_sha256']
 files=list((R/'scripts').glob('*p3_cg8_scalar*'))+[R/'scripts/cfd_reference3d_p3_coarse_scalar.c']+list((R/'tests').glob('*p3_cg8_scalar*'))+list((R/'docs').glob('cfd_3d_p3_cg8_scalar*.md'))
 out.write_text(json.dumps(dict(status='STRICT NUMERICAL PASS, USEFULNESS REJECTED: one-pass coarse gain but whole-cost miss',persistent_goal_complete=False,numerical_field_published=True,physical_accuracy_certified=False,large_or_finer_trial_permitted=False,tests_passed=3,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),whole_wall_s=r['wall_s'],full_residual=row['final_residual'],pressure_coarse=pp,force_scalar_equivalence=changes,owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,numeric_stage_admission=a,native_hashes_preserved=native,changed_preexisting_files=changed,action_benchmark=b),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
