"""Once-only geometry plus full-field force-sensitivity checkpoint."""
import json,hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from cfd_reference3d_heldfloor6_mesh import build,SPACINGS,evaluate
from cfd_reference3d_pressure_complement10 import reserve
from cfd_reference3d_flexible import basis_reservation
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-heldfloor6'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def frozen(p,kind):
 r=json.loads(p.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['rss_cap_bytes']==1800*2**20 and r['wall_cap_s']==180 and r['mesh_cap']==50000
 for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
 for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
 assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
 return r,json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text())
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert sha(Path(pre['path']))==pre['sha256']=='d71b6504364957e1788b5507c8a65f9ff38d299e74d4e288213089de30524d67'
 base=json.loads((D/'baseline.json').read_text());changed=[p for p,h in base.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for p,h in native.items():assert sha(R/p)==h
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==4 and sha(D/'support/tests.log')==s['test_log_sha256']
 for p,h in s['source_sha256'].items():assert sha(R/p)==h==sha(D/'support/frozen'/Path(p).name)
 p=next((D/'survey-runs').glob('*/*-receipt.json'));gr,g=frozen(p,'survey-runs');assert g['diagnostic_accepted'] and g['selected_case']['geometry_accepted_on_both_lengths'] and not g['numeric_factor_attempted'] and not g['numerical_field_published']
 geometry=Path(g['geometry_path']);assert sha(geometry)==g['geometry_sha256']
 for L,q in g['selected_case']['domains'].items():assert not q['reasons'] and evaluate(g['original_quality'][L],q['quality'],float(L),SPACINGS[0])==[]
 npth=next((D/'numeric-runs').glob('*/*-receipt.json'));nr,row=frozen(npth,'numeric-runs');verify_receipt(npth)
 assert nr['linear_iteration_cap']==3000 and row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<=1e-10 and row['preconditioner']['flexible_iteration']['final_true_metric']<=1e-11
 assert row['numerically_accepted'] and row['info']==0 and row['tetrahedra']==28416 and row['geometry_control']['geometry_sha256']==g['geometry_sha256']
 pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['complementary_mass_inverse_scale']==10 and pc['coarse_correction_scale']==1
 assert row['workspace_retirement']['all_completed_buffers_and_backing_owners_released'] and row['workspace_retirement']['remaining_authority_preserved_bitwise'] and row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise'] and row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup']
 mesh,lo,hi,axes,n=build(4.,SPACINGS[0]);snap=Path(nr['command'][nr['command'].index('--snapshot')+1])
 with np.load(snap,allow_pickle=False) as z:
  for k,a in (('vertices_m',mesh.p),('tetrahedra',mesh.t),('lo',lo),('hi',hi)):np.testing.assert_array_equal(z[k],a)
 a=next(x for x in nr['progress'] if x.get('phase')=='numeric_stage_admission');nv=row['condensed_free_dofs']-n;assert a['outer_basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],6) and a['coarse_pressure_reservation_bytes']==reserve(nv,n) and a['reserve_bytes']==32*2**20 and a['basis_reservation_bytes']==a['outer_basis_reservation_bytes']+a['coarse_pressure_reservation_bytes'] and a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes')) and a['numeric_stage_admitted']
 oldp=next((R/'build/c3d-force-resume/runs').glob('*/L4-body6-second-normal-complement10-receipt.json'));_,old=verify_receipt(oldp);c=force_comparison(row,old);assert c['raw_surface_reaction_relative_mismatch']['refined']>c['raw_surface_reaction_relative_mismatch']['base'] and not c['physical_force_gate_passed'];assert len(list((D/'numeric-runs').glob('*/*-receipt.json')))==1
 comparison=dict(force_testing_resumed=True,numerical_field_accepted=True,mesh_promoted=False,matching_L8_permitted=False,physical_accuracy_certified=False,general_or_native_adopted=False,whole_wall_s=nr['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=nr['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],timings=row['timings'],force_comparison=c,numeric_stage_admission=a,original_force_energy_gates_preserved=True,uniform_or_nested_resolution_claimed=False)
 cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(comparison,indent=2)+'\n')
 files=list((R/'scripts').glob('*heldfloor6*'))+list((R/'tests').glob('*heldfloor6*'))+list((R/'docs').glob('cfd_3d_heldfloor6*.md'))
 out.write_text(json.dumps(dict(status='PROGRESS: full force sensitivity test resumed; redistribution force consistency worsens, promotion rejected',persistent_goal_complete=False,physical_accuracy_certified=False,predecessor_sha256=pre['sha256'],source_sha256={str(p.relative_to(R)):sha(p) for p in files},receipt_sha256={str(p):sha(p) for p in (p,npth,oldp)},geometry_sha256=sha(geometry),native_hashes_preserved=native,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,comparison_sha256=sha(cp),comparisons=comparison,committed=False,packaged=False,installed=False),indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out))))
if __name__=='__main__':main()
