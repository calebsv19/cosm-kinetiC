"""Seal wall physical results, candidate choices and preserved source boundaries."""
import ast
import hashlib
import json
import math
from pathlib import Path
import re
import numpy as np
from assess_cfd_native_wall_accuracy import assess, sha
ROOT=Path(__file__).resolve().parents[1];DEST=ROOT/'build/c3d-wall-shear'

def load(path):return json.loads(path.read_text())

def verify_receipt(root):
    candidates=list((ROOT/root/'runs').glob('*/*/receipt.json'));assert len(candidates)==1,candidates
    path=candidates[0];receipt=load(path);contract=load(path.parent/'contract.json')
    for q,h in contract['source_sha256'].items():assert sha(ROOT/q)==h==sha(path.parent/'source'/q)
    for q,h in receipt['artifact_sha256'].items():assert sha(path.parent/q)==h
    return path,receipt,contract

def main():
    out=DEST/'checkpoint-audit.json';assert not out.exists()
    preceding=load(DEST/'predecessor.json');previous=ROOT/'build/c3d-local-continuation/closeout-verification.json'
    assert sha(previous)==preceding['local_closeout_sha256']
    old=load(previous)
    for q,h in old['native_hashes'].items():assert sha(ROOT/q)==h
    roots=('build/c3d-wall-shear','build/c3d-wall-pressure','build/c3d-wall-shear-supported','build/c3d-wall-shear-fine')
    receipts={root:verify_receipt(root) for root in roots}
    original_path,original,_=receipts[roots[0]];pressure_path,pressure,_=receipts[roots[1]];fine_path,fine,fine_contract=receipts[roots[2]];failed_path,failed,_=receipts[roots[3]]
    assert original['status']=='completed_nonzero_cube_wall_shear_investigation'
    assert pressure['status']=='completed_nonzero_cube_wall_shear_investigation'
    assert fine['status']=='completed_supported_native_wall_shear_known_answer'
    assert not original['candidate_permitted_for_next_diagnostic'] and not pressure['candidate_permitted_for_next_diagnostic']
    assert pressure['pressure_candidate_permitted_for_next_diagnostic']
    assert fine['known_answer_wall_accuracy_targets_passed']
    assert fine_contract['n']==80 and 2*80**3<=1048576
    assert fine_contract['owned_cap_bytes']==1024**3 and fine_contract['fixture_wall_cap_s']==600
    assert failed['status']=='failed' and failed['failure']=='n128: child exited -6'
    assert 'cfd_cartesian3d_init' in (failed_path.parent/'n128.stderr').read_text()
    assert 'if (count > 1048576)' in (ROOT/'src/app/cfd_cartesian3d.c').read_text()
    for n in (32,64):
        a=original['controls'][str(n)];b=pressure['controls'][str(n)]
        for key in ('velocity_relative_error','pressure_relative_error','maximum_divergence','momentum_residual',
                    'analytic_viscous_force_n','solved_pressure_force_n','solved_viscous_force_n','prescribed_pressure_force_n','prescribed_viscous_force_n'):
            np.testing.assert_allclose(a[key],b[key],rtol=0,atol=1e-15)
    for root in ('build/c3d-wall-consistency','build/c3d-wall-pressure-calibration'):
        d=ROOT/root;c=load(d/'checkpoint-audit.json')
        for q,h in c['source_sha256'].items():assert sha(ROOT/q)==h==sha(d/'source'/q)
        for q,h in c['artifact_sha256'].items():assert sha(d/q)==h
    consistency=load(ROOT/'build/c3d-wall-consistency/checkpoint-audit.json');assert consistency['status']=='verified_native_wall_curvature_consistency_defect'
    assert len(consistency['controls'])==3 and all(q['local_polynomial_cases']==6 for q in consistency['controls'].values())
    calibration=load(ROOT/'build/c3d-wall-pressure-calibration/checkpoint-audit.json');assert calibration['status']=='passed_actual_c_pressure_candidate_calibration'
    control=calibration['control'];assert control['samples']==940 and control['fallback_degree_samples']==[64,76,800]
    assert control['maximum_patch_error_n']<1e-11 and control['gauge_shift_error_n']<1e-11
    assert abs(control['cusp_candidate_ratio']-math.sqrt(.5))<1e-9
    transforms=[DEST/'fixture-transform.json',DEST/'pressure-fixture-transform.json',DEST/'pressure-runner-transform.json',
                ROOT/'build/c3d-wall-shear-fine/fixture-transform.json',ROOT/'build/c3d-wall-shear-supported/fixture-transform.json',
                ROOT/'build/c3d-wall-shear-supported/runner-transform.json']
    for path in transforms:
        t=load(path);assert sha(ROOT/t['parent'])==t['parent_sha256'] and sha(ROOT/t['output'])==t['output_sha256']
        text=(ROOT/t['parent']).read_text()
        for a,b in t['literal_replacements']+t.get('additional_literal_replacements',[]):assert a in text;text=text.replace(a,b)
        assert text==(ROOT/t['output']).read_text()
    physical=assess(fine_path);assert physical==load(ROOT/'build/c3d-wall-shear-supported/physical-assessment.json')
    assert physical['status']=='known_answer_wall_accuracy_passed'
    coarse=assess(original_path);assert coarse==load(DEST/'coarse-physical-assessment.json')
    assert coarse['status']=='known_answer_wall_accuracy_targets_not_met'
    baseline=load(DEST/'baseline.json');changes=[q for q,h in baseline.items() if not (ROOT/q).exists() or sha(ROOT/q)!=h]
    assert set(changes)<= {'docs/README.md','docs/current_truth.md'},changes
    for q in ('docs/current_truth.md','docs/README.md','docs/cfd_3d_wall_accuracy_checkpoint.md'):
        assert 'cfd_3d_wall_accuracy_checkpoint.md' in (ROOT/q).read_text() or q.endswith('checkpoint.md')
    for target in re.findall(r'\]\(([^)]+)\)',(ROOT/'docs/cfd_3d_wall_accuracy_checkpoint.md').read_text()):
        assert (ROOT/'docs'/target).exists()
    new_sources=[q for q in ROOT.glob('scripts/*wall*py') if str(q.relative_to(ROOT)) not in baseline]
    for p in new_sources:ast.parse(p.read_text(),filename=str(p))
    frozen=DEST/'verification-frozen';frozen.mkdir(exist_ok=False)
    for p in (Path(__file__),ROOT/'scripts/assess_cfd_native_wall_accuracy.py',ROOT/'docs/cfd_3d_wall_accuracy_checkpoint.md'):
        (frozen/p.name).write_bytes(p.read_bytes())
    result=dict(status='NATIVE NONZERO WALL ACCURACY PASSES; PRESSURE DIAGNOSTIC SELECTED; VISCOUS CANDIDATE REJECTED',
        source_changes_preexisting=changes,predecessor_sha256=sha(previous),
        receipt_sha256={root:sha(value[0]) for root,value in receipts.items()},
        local_stencil_checkpoint_sha256=sha(ROOT/'build/c3d-wall-consistency/checkpoint-audit.json'),
        pressure_patch_checkpoint_sha256=sha(ROOT/'build/c3d-wall-pressure-calibration/checkpoint-audit.json'),
        distinct_wall_resolutions=[8,16,32,64,80],native_wall_solves=7,
        unique_independent_forcing_samples=84,local_polynomial_stencil_controls=18,pressure_patch_controls=940,
        fine_known_answer=physical,coarse_physical_targets_rejected=True,larger_grid_admission_rejected=True,
        pressure_candidate_diagnostic_selected=True,viscous_candidate_rejected=True,
        audit_source_sha256=sha(Path(__file__)),
        docs_sha256={q:sha(ROOT/q) for q in ('docs/current_truth.md','docs/README.md','docs/cfd_3d_wall_accuracy_checkpoint.md')},
        protected_native_sha256=old['native_hashes'],native_operator_changed=False,native_default_adopted=False,
        original_pressure_driven_cube_qualification=False,persistent_goal_complete=False,
        doc_sync=dict(diff_window='HEAD~1..HEAD',current_source_inspected=True,bounded_memdb_project_query_limit=3,
                      future_intent_inspected_preserved=True,private_log_appended=True,memory_write_skipped='No explicit user authorization'),
        scope='independently forced nonzero shear at a flat cube wall away from edges; no pressure-driven sharp-edge cube, arbitrary-object, transient or inertial qualification')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],sha256=sha(out),preserved_existing_source=True)))
if __name__=='__main__':main()
