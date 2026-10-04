"""Seal graded geometry, preserved equations, complete fields and physical comparisons."""
import hashlib,json
from pathlib import Path
from cfd_reference3d_accuracy_graded_evidence import verify,physical_comparison,sha
from cfd_reference3d_accuracy_evidence import verify as parent_verify
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-accuracy-graded'
def main():
 out=D/'checkpoint-audit.json';assert not out.exists();pre=json.loads((D/'predecessor.json').read_text());assert pre['host_memory_bytes']==16*2**30 and sha(R/'build/c3d-accuracy-first/checkpoint-audit.json')==pre['accuracy_audit_sha256'] and sha(R/'build/c3d-accuracy-side-layer/checkpoint-audit.json')==pre['side_layer_audit_sha256'] and sha(R/'build/c3d-accuracy-side-layer/closeout-verification.json')==pre['closeout_sha256']
 changed=[q for q,h in json.loads((D/'baseline.json').read_text()).items() if sha(R/q)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
 s=json.loads((D/'support/receipt.json').read_text());assert s['tests_passed']==6 and sha(D/'support/tests.log')==s['test_log_sha256'] and sha(D/'transforms.json')==s['transforms_sha256'] and sha(D/'geometry-survey.json')==s['geometry_survey_sha256'] and sha(D/'paired-graded-geometry.npz')==s['geometry_sha256'];sources=s['source_sha256'].copy()
 for q,h in sources.items():assert sha(R/q)==h==sha(D/'support/frozen'/q)
 for t in json.loads((D/'transforms.json').read_text()):
  assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
  for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
  assert v==(R/t['output']).read_text()
 e=json.loads((D/'evidence-support/receipt.json').read_text());assert sha(D/'evidence-transform.json')==e['transform_sha256']
 for q,h in e['source_sha256'].items():assert sha(R/q)==h==sha(D/'evidence-support/frozen'/q);sources[q]=h
 t=json.loads((D/'evidence-transform.json').read_text());v=Path(t['parent']).read_text();assert sha(Path(t['parent']))==t['parent_sha256']
 for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
 assert v==Path(t['output']).read_text() and sha(Path(t['output']))==t['output_sha256']
 geometry=json.loads((D/'geometry-survey.json').read_text());assert geometry['translated_inner_cells_match'] and not geometry['numerical_factor_attempted'] and not geometry['flow_field_published'] and all(q['geometry_accepted'] and not q['reasons'] for q in geometry['domains'].values())
 assessment=json.loads((D/'physical-assessment.json').read_text());fields={};ratios={}
 for L in (4,8):
  a=assessment['fields'][f'L{L}'];p=Path(a['receipt']);r,row=verify(p);fields[L]=row;assert sha(p)==a['receipt_sha256'];oldp=Path(a['old_receipt']);_,old=parent_verify(oldp);assert sha(oldp)==a['old_receipt_sha256']
  expected=physical_comparison(old,row,'same_domain_refinement');assert expected==assessment['same_domain_refinement'][f'L{L}'] and expected['force_change_passed'] and expected['scalar_refinement_passed'] and not expected['raw_equilibrium_passed'];ratios[f'L{L}']=expected['raw_surface_reaction_relative_mismatch']/physical_comparison(old,old,'same_domain_refinement')['raw_surface_reaction_relative_mismatch']
  f=row['preconditioner'];assert f['kind']=='mixed_workspace_coupled_cholesky' and f['physical_operator_dtype']=='float64' and f['shared_input_preserved_after_factor'] and f['shared_input_preserved_after_solve'] and f['exact_encoding']['original_values_preserved_bitwise'] and f['exact_encoding']['bitwise_roundtrip_verified'] and f['exact_encoding']['original_value_sha256']==f['exact_encoding']['decoded_value_sha256'] and f['exact_encoding']['coefficients']<=120000000
  assert f['flexible_iteration']['restart']==6 and f['flexible_iteration']['final_true_metric']<1e-11 and row['final_retained_residual']['estimated_full_residual']<1e-11 and row['pressure_preconditioner']['complementary_mass_inverse_scale']==10
 assert physical_comparison(fields[4],fields[8],'domain_sensitivity')==assessment['domain_sensitivity'] and assessment['domain_sensitivity']['force_change_passed'];selection=json.loads((D/'selection-contract.json').read_text());assert ratios['L4']<=selection['maximum_new_raw_equilibrium_mismatch_ratio_to_old'] and json.loads((D/'L4-physical-assessment.json').read_text())['L8_trial_permitted'] and all(v<.9 for v in ratios.values())
 native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
 for q,h in native.items():assert sha(R/q)==h
 for q in ('scripts/audit_cfd_3d_accuracy_graded.py','docs/cfd_3d_accuracy_graded_checkpoint.md'):sources[q]=sha(R/q)
 result=dict(status='GRADED PAIR USEFUL: FORCE GAP REDUCED; RAW1% STILL OPEN',persistent_goal_complete=False,tests_passed=6,source_sha256=sources,physical_assessment_sha256=sha(D/'physical-assessment.json'),geometry_sha256=s['geometry_sha256'],geometry_survey_sha256=s['geometry_survey_sha256'],physical_assessment=assessment,new_old_raw_mismatch_ratios=ratios,native_hashes_preserved=native,changed_preexisting_files=changed)
 out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(checkpoint=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
