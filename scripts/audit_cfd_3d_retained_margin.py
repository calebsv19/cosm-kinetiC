#!/usr/bin/env python3
"""Once-only audit of strict publication, small-restart controls and matched force recovery."""
import hashlib
import json
from pathlib import Path
import numpy as np
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt, force_comparison
from cfd_reference3d_requested_target import requested_full_linear_acceptance
R=Path(__file__).resolve().parents[1]
D=R/'build/c3d-retained-margin'
parent.D=D
import audit_cfd_3d_observation_reuse as observers_audit
observers_audit.D=D
parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    out=D/'checkpoint-audit.json';assert not out.exists()
    predecessor=json.loads((D/'predecessor.json').read_text())
    assert sha(Path(predecessor['path']))==predecessor['sha256']=='a14c8a02793988ce8caf168ca8899bdf4352b86c3256343604cc3725fa4c459e'
    protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for p,h in protected.items():assert sha(R/p)==h
    baseline=json.loads((D/'baseline.json').read_text())
    changed=[p for p,h in baseline.items() if sha(R/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
    transforms=json.loads((D/'source-transform-control.json').read_text())
    for t in transforms:
        assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
        assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
    support=json.loads((D/'support-test-receipt.json').read_text())
    assert support['passed'] and support['test_count']==3
    for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
    assert sha(D/'support-tests-01.log')==support['log_sha256'] and '\nOK\n' in (D/'support-tests-01.log').read_text()
    assert sha(D/'source-transform-control.json')==support['transformation_sha256']
    calibration={};receipt_hashes={};identities={}
    for p in sorted((D/'runs').glob('*/L4-*-receipt.json')):
        r,row=parent.frozen(p);verify_receipt(p);restart=row['outer_iteration']['restart'];assert restart == 6
        assert requested_full_linear_acceptance(row['info'],row['final_residual']['true_residual'],row['target'])==row['linear_solve_accepted']
        assert row['target']==1e-10 and row['retained_target']==1e-11 and row['iterations']<=3000
        assert row['preconditioner']['flexible_iteration']['final_true_metric']<=1e-11
        if r['returncode']==0:parent.accepted(p,restart)
        else:
            assert row['numerical_failure_reasons']==['volume_divergence'] and row['volume_divergence_max_s_inv']>=1e-8
            assert not p.with_name(p.name.removesuffix('-receipt.json')+'.npz').exists()
        prior=next((R/'build/c3d-restart/runs').glob('*/'+p.name.replace('-r'+str(restart),'-r60')))
        pr,pa=verify_receipt(prior)
        assert row['identity']==pa['identity'] and row['preconditioner']['rounded_values_sha256']==pa['preconditioner']['rounded_values_sha256']
        name=p.name.removesuffix('-receipt.json');receipt_hashes[str(p)]=sha(p)
        calibration[name]=dict(returncode=r['returncode'],whole_wall_s=r['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],full_residual=row['final_residual'],iterations=row['iterations'],flexible_iteration=row['preconditioner']['flexible_iteration'],maximum_divergence_s_inv=row['volume_divergence_max_s_inv'],numerical_failure_reasons=row['numerical_failure_reasons'])
    assert len(calibration)==2
    cal=json.loads((D/'calibration-comparisons.json').read_text())
    for p,h in cal['input_sha256'].items():assert sha(Path(p))==h
    stages={}
    for p in sorted((D/'stage-runs').glob('*/*-receipt.json')):
        r,row=parent.frozen(p)
        assert r['returncode']==0 and row['diagnostic_accepted'] and not row['numeric_factor_attempted'] and not row['numerical_field_published']
        assert row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved'] and row['tetrahedra']==33216
        restart=row['outer_iteration']['restart'];parent.budget(row['admission'],row['condensation']['reduced_free_dofs'],restart)
        prior=next((R/'build/c3d-restart/stage-runs').glob('*/L8*-r12-stage.json'))
        old=json.loads(prior.read_text());assert row['identity']==old['identity']
        for k in ('factor_storage_bytes','numeric_workspace_bytes','reserve_bytes'):assert row['admission'][k]==old['admission'][k]
        stages[str(restart)]=row;receipt_hashes[str(p)]=sha(p)
    larger={};observers={}
    for p in sorted((D/'runs').glob('*/L8-*-receipt.json')):
        r,row=verify_receipt(p);receipt_hashes[str(p)]=sha(p)
        if row is not None:parent.frozen(p)
        else:
            for q,h in r['source_sha256'].items():assert sha(R/'scripts'/q)==h
            assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
        if r['returncode']==0:
            parent.accepted(p,row['outer_iteration']['restart']);assert row['identity']==stages[str(row['outer_iteration']['restart'])]['identity']
        else:assert not p.with_name(p.name.removesuffix('-receipt.json')+'.npz').exists()
        larger[str(p)]=dict(returncode=r['returncode'],whole_wall_s=r['wall_s'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],stop_reason=r['stop_reason'],row=row)
    for p in sorted((D/'observer-runs').glob('*/*-receipt.json')):
        r,row=observers_audit.observer(p);receipt_hashes[str(p)]=sha(p)
        assert r['returncode']==0 and row['diagnostic_accepted'] and row['original_linear_residual']['true_residual']<1e-10
        assert sha(Path(row['input_receipt']))==row['input_receipt_sha256']
        assert sha(Path(row['signed_attribution_path']))==row['signed_attribution_sha256']
        error=max(abs(v) for l in row['lifts'] for part in ('pressure','viscous') for v in l[part]['identity_error_n']);assert error<1e-9
        observers[str(p)]=dict(maximum_identity_error_n=error,volume_stress=row['volume_strong_equilibrium_defect_l2'],interior_jump=row['interior_stress_jump_l2'])
    comparisons=json.loads((D/'comparisons.json').read_text())
    for p,h in comparisons['input_sha256'].items():assert sha(Path(p))==h
    for p,c in comparisons.get('matched_force_comparisons',{}).items():
        r,row=verify_receipt(Path(p));br,b=verify_receipt(Path(c['base_receipt']))
        assert c['force_comparison']==force_comparison(row,b)
    if comparisons.get('L8_normal_refinement'):
        c=comparisons['L8_normal_refinement'];br,b=verify_receipt(Path(c['base_receipt']));row=next(v['row'] for v in larger.values() if v['returncode']==0);assert c['force_comparison']==force_comparison(row,b)
        assert comparisons['held_inner_tetrahedra']==23616 and comparisons['saved_cube_surface_triangles_preserved']
    sources=list((R/'scripts').glob('*retained_margin*.py'))+[R/'scripts/cfd_reference3d_requested_target.py',R/'tests/test_cfd_reference3d_retained_margin.py',R/'docs/cfd_3d_retained_margin_checkpoint.md',R/'docs/cfd_3d_retained_margin_next_goal.md']
    result=dict(status=comparisons['status'],persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,default_restart_changed=False,test_count=3,predecessor_sha256=predecessor['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,source_sha256={str(p.relative_to(R)):sha(p) for p in sources},support_receipt_sha256=sha(D/'support-test-receipt.json'),source_transformation_sha256=sha(D/'source-transform-control.json'),receipt_sha256=receipt_hashes,calibration=calibration,comparison_sha256=sha(D/'comparisons.json'),calibration_comparison_sha256=sha(D/'calibration-comparisons.json'),matched_stages={k:v['admission'] for k,v in stages.items()},larger=larger,observers=observers,committed=False,packaged=False,installed=False)
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
