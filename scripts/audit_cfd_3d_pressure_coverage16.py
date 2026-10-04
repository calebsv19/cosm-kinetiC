"""Once-only pressure-only refinement eligibility and full cost rejection."""
import json,hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
from cfd_reference3d_p3_cg8_scalar import work_reserve
from cfd_reference3d_pressure_coverage16 import reserve as pressure_reserve,diagnostic_reserve
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pressure-coverage16'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='2e6538cfd9d024d310322a148fed6583cdee68848436dbf2088f8b894f3f9f73'
 changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 t=json.loads((D/'control-transform-control.json').read_text());assert sha(D/'control-transform-control.json')==s['control_transform_sha256'];v=(R/t['parent']).read_text()
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert v==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
 cp=next((D/'control-runs').glob('*/*-receipt.json'));cr=json.loads(cp.read_text());assert cr['returncode']==0 and cr['stop_reason'] is None and cr['schema']=='physics_sim_c3d_pressure_column_diagnostic_receipt_v1'
 for q,h in cr['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(cp.parent/'source'/q)
 for q,h in cr['artifact_sha256'].items():assert sha(Path(q))==h
 cx=json.loads(Path(cr['command'][cr['command'].index('--output')+1]).read_text());assert cx['diagnostic_accepted'] and not cx['flow_field_published'] and cx['diagnostic_factor_owners_retired'];sel=cx['selection'];assert sel['eligibility_gate_passed'] and max(sel['condition_ratios'].values())<=.9 and min(sel['selected_lowest_six_mass_coverage'])>=.9
 eligibility=json.loads((D/'control-eligibility.json').read_text());assert eligibility['control_receipt_sha256']==sha(cp) and eligibility['full_trial_permitted'];assert cx['transport']['original_ten_retained_bitwise']
 for pp in cx['pressure16_preconditioners'].values():assert pp['coarse_relative_skew']<1e-5 and pp['coarse_reproduction_relative_error']<1e-5 and pp['coarse_smallest_eigenvalue']>0 and pp['columns']==16
 p=next((D/'runs').glob('*/*-receipt.json')).resolve();r,row=verify_receipt(p);assert r['returncode']==0 and row['target']==1e-10 and row['retained_target']==1e-11 and row['outer_iteration']['restart']==30 and row['final_residual']['true_residual']<=1e-10
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h
 oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json')).resolve();oldr,old=verify_receipt(oldp);assert row['identity']==old['identity'] and row['preconditioner']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256'];changes={k:abs(row[k][0]/old[k][0]-1) if isinstance(row[k],list) else abs(row[k]/old[k]-1) for k in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n','inlet_pressure_pa','physical_dissipation_w')};assert max(changes.values())<1e-7
 pc=row['preconditioner'];assert pc['local_inner_iteration_cap']==8;pp=row['pressure_preconditioner'];assert pp['coarse_relative_skew']<1e-5 and pp['coarse_reproduction_relative_error']<1e-5 and pp['columns']==16 and pp['constant_pressure_direction_retained'];assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['factor_and_coarse_owners_released']
 np_=row['condensation']['condensed_pressure_dofs'];nv=row['condensed_free_dofs']-np_;nc=pc['coarse_factor']['dofs'];a=next(x for x in r['progress'] if x.get('phase')=='numeric_stage_admission');assert a['numeric_stage_admitted'] and a['outer_basis_reservation_bytes']==basis_reservation(nv+np_,30) and a['coarse_pressure_reservation_bytes']==pressure_reserve(nv,np_) and a['distributed_velocity_work_reservation_bytes']==work_reserve(nv,nc)
 assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['reserve_bytes']==32*2**20 and r['wall_s']>2*oldr['wall_s']
 co=json.loads((R/'scripts/cfd_reference3d_pressure_coverage16_coefficients.json').read_text());assert sha(R/'scripts/cfd_reference3d_pressure_coverage16_coefficients.json')==s['coefficient_sha256'] and sha(Path(co['reference_receipt']))==co['reference_receipt_sha256'];C=np.array(co['coefficients']);assert C.shape==(58,6) and np.linalg.norm(C[:10])<1e-12 and np.linalg.norm(C.T@C-np.eye(6))<1e-10
 files=list((R/'scripts').glob('*pressure_coverage16*'))+list((R/'tests').glob('*pressure_coverage16*'))+list((R/'docs').glob('cfd_3d_pressure_coverage16*.md'));out.write_text(json.dumps(dict(status='STRICT NUMERICAL PASS, USEFULNESS REJECTED: sampled condition improved but full iterations/runtime worsened',persistent_goal_complete=False,physical_accuracy_certified=False,large_or_finer_trial_permitted=False,tests_passed=4,source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),control_receipt=str(cp),control_receipt_sha256=sha(cp),predecessor_sha256=pre['sha256'],control_selection=sel,full_residual=row['final_residual'],whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,force_scalar_equivalence=changes,numeric_stage_admission=a,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
