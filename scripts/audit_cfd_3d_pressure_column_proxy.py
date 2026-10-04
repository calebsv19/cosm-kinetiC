"""Once-only original Double pressure-column diagnostic readback."""
import json,hashlib
from pathlib import Path
import numpy as np
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_p3_cg8_scalar import work_reserve
from cfd_reference3d_pressure_column_proxy import diagnostic_reserve,select_scale
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pressure-column-proxy'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='0e2d65e3e09ba50c4209d8b018fa11d0a8b6d46a00e7a59d2529f970007a0f68'
 changed=[p for p,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==5 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'support/tests-four.log')==s['initial_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 p=next((D/'runs').glob('*/*-receipt.json'));r=json.loads(p.read_text());assert r['schema']=='physics_sim_c3d_pressure_column_diagnostic_receipt_v1' and r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None and r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256'];assert r['coarse_factor_build']['create_abi_arguments']==7 and r['coarse_factor_build']['scalar_action_abi_arguments']==6 and r['double_factor_build']['create_abi_arguments']==6 and r['double_factor_build']['value_dtype']=='float64'
 x=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());assert x['diagnostic_accepted'] and not x['numerically_accepted'] and not x['flow_field_published'] and not x['complete_spectrum_certified'] and x['diagnostic_factor_owners_retired'] and x['original_inputs_preserved'] and x['pressure_constant_kept'] and x['tetrahedra']==4992 and x['peak_rss_bytes']<1800*2**20
 oldp=next((R/'build/c3d-complement10/runs').glob('*/L4-body2-original-complement10-receipt.json'));oldr=json.loads(oldp.read_text());old=json.loads(Path(oldr['command'][oldr['command'].index('--output')+1]).read_text())
 for k,v in x['identity'].items():assert v==old['identity'][k]
 assert x['factors']['p3']['rounded_values_sha256']==old['preconditioner']['rounded_values_sha256']==x['factors']['qualified_float']['rounded_values_sha256'] and x['conversion_full_mixed_relative_action_change']<=1e-12
 nv=x['factors']['p3']['local_factor']['solve_workspace_bytes']//32;np_=1248;nc=x['factors']['p3']['coarse_factor']['dofs'];outer=basis_reservation(nv+np_,30);pw=reserve(nv,np_);dw=diagnostic_reserve(nv,np_);vw=work_reserve(nv,nc);total=outer+pw+dw+vw;assert x['reserved_work_bytes']==total
 for a in x['admissions']:
  assert a['numeric_stage_admitted'] and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'))+total and a['reserve_bytes']==32*2**20
 assert len(x['admissions'])==3 and max(x['columns']['exact_double']['velocity_true_relative_residuals'])<=1e-10
 conditions={name:{int(k):v['balanced_ten_sampled_condition'] for k,v in records.items()} for name,records in x['coverage'].items()};assert x['selection']==select_scale(conditions) and x['selection']['selected_scale'] is None
 with np.load(Path(r['command'][r['command'].index('--snapshot')+1]),allow_pickle=False) as z:
  assert 'velocity_coefficients' not in z.files and 'pressure_coefficients' not in z.files;Q=z['coverage_basis_mass'];Z=z['pressure_basis'];assert Q.shape[1]==x['coverage_basis']['columns']<=64 and np.linalg.norm(Q.T@Q-np.eye(Q.shape[1]))<1e-10;refW=z['pressure_schur_exact_double'];refU=z['velocity_load_response_exact_double']
  for name,m in x['columns'].items():
   W=z['pressure_schur_'+name];U=z['velocity_load_response_'+name];K=Z.T@W;Kr=Z.T@refW;assert np.isclose(np.linalg.norm(W-refW)/np.linalg.norm(refW),m['schur_column_relative_error'],rtol=1e-10,atol=1e-14);assert np.isclose(np.linalg.norm(K-Kr)/np.linalg.norm(Kr),m['projected_coarse_relative_error'],rtol=1e-10,atol=1e-14);np.testing.assert_allclose(np.linalg.norm(U-refU,axis=0)/np.linalg.norm(refU,axis=0),m['velocity_solution_relative_errors'],rtol=1e-10,atol=1e-14)
 files=list((R/'scripts').glob('*pressure_column_proxy*'))+list((R/'tests').glob('*pressure_column_proxy*'))+list((R/'docs').glob('cfd_3d_pressure_column_proxy*.md'));out.write_text(json.dumps(dict(status='DIAGNOSTIC PASS: fixed pressure proxy error quantified; scalar sweep not eligible',persistent_goal_complete=False,flow_field_published=False,physical_accuracy_certified=False,tests_passed=5,source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt=str(p),receipt_sha256=sha(p),predecessor_sha256=pre['sha256'],columns=x['columns'],selection=x['selection'],geometry=x['geometry'],whole_wall_s=r['wall_s'],owned_peak_mib=x['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,native_hashes_preserved=native,changed_preexisting_files=changed),indent=2)+'\n');print(sha(out))
if __name__=='__main__':main()
