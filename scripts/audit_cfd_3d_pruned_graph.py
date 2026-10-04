#!/usr/bin/env python3
"""Once-only audit of complete physical controls and optional graph cost decisions."""
import hashlib,json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pruned-graph'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def frozen(p):
    r=json.loads(p.read_text());assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(p.parent/'source'/q)==h==sha(R/'scripts'/q)
    assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    b=r['factor_build'];assert sha(p.parent/'source/factor.dylib')==b['library_sha256']==r['factor_library_sha256']
    assert sha(p.parent/'source/factor-build.json')==r['factor_build_record_sha256']
    assert b==json.loads((p.parent/'source/factor-build.json').read_text()) and b['source_sha256']==r['source_sha256']['cfd_reference3d_mixed_storage.c']
    assert all(x in b['command'] for x in ('-std=c11','-Wall','-Wextra','-Werror'))
    result=Path(r['command'][r['command'].index('--output')+1]);return r,json.loads(result.read_text()) if result.exists() else None

def main():
    out=D/'checkpoint-audit.json';assert not out.exists()
    previous=json.loads((D/'predecessor.json').read_text());assert sha(Path(previous['path']))==previous['sha256']=='55a22fa1d1226fa1d66b971641889c4613d2aefcdfad99648d4b680b158d90f8'
    protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for n,h in protected.items():assert sha(R/n)==h
    baseline=json.loads((D/'baseline.json').read_text());changed=[q for q,h in baseline.items() if sha(R/q)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
    support=json.loads((D/'support-test-receipt.json').read_text());assert support['test_count']==6 and support['passed']
    for n,h in support['source_sha256'].items():assert sha(R/n)==h==sha(Path(support['frozen_source'])/Path(n).name)
    assert sha(D/'support-tests-01.log')==support['log_sha256'] and 'Ran 6 tests' in (D/'support-tests-01.log').read_text() and '\nOK\n' in (D/'support-tests-01.log').read_text()
    assert sha(D/'support-factor.dylib')==support['support_factor_sha256']
    survey_path=next((D/'strength-runs').glob('*/L4-body6-normal-strength-receipt.json'));sr,s=frozen(survey_path)
    assert sr['returncode']==0 and s['diagnostic_accepted'] and not s['symbolic_factor_attempted'] and not s['numeric_factor_attempted'] and not s['numerical_field_published']
    assert s['statistics']['exact_zero_off_diagonal_blocks']==0 and s['tetrahedra']==23616
    stats=s['statistics'];assert [x['threshold'] for x in stats['thresholds']]==[0.,1e-3,1e-2]
    measurements={};receipts={};fields={};rows={}
    for stem in ('L4-body2-original-pruned0','L4-body2-original-pruned1e3','L4-body2-original-pruned1e2','L4-body6-normal-pruned0','L4-body6-normal-pruned1e3'):
        p=next((D/'runs').glob('*/'+stem+'-receipt.json'));r,row=verify_receipt(p);frozen(p)
        if r['returncode']==0:
            assert row['target']==1e-10 and row['final_residual']['true_residual']<1e-10 and row['preconditioner']['shared_input_preserved_after_solve']
            assert row['preconditioner']['graph_approximation']['physical_input_preserved_bitwise']
            assert row['preconditioner']['user_factor_storage_verified'] and row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup'] and row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise']
            guard=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');assert guard['numeric_stage_admitted'] and guard['estimated_numeric_stage_bytes']<=1800*1024**2
            assert guard['estimated_numeric_stage_bytes']==sum(guard[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes'))
            fields[stem]=str(p.parent/(stem+'.npz'))
        else:
            assert stem in ('L4-body2-original-pruned1e2','L4-body6-normal-pruned1e3') and not p.parent.joinpath(stem+'.npz').exists()
        rows[stem]=row;receipts[stem]=str(p);measurements[stem]=dict(receipt_sha256=sha(p),returncode=r['returncode'],wall_s=r['wall_s'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],owned_peak_rss_bytes=row.get('peak_rss_bytes') if row else None,iterations=row.get('iterations') if row else None,full_residual=row.get('final_residual') if row else None,resource_failure=row.get('resource_phase_rejected') if row else None,diagnostic_failure=r['diagnostic_failure'],factor_bytes=row.get('preconditioner',{}).get('symbolic_factor_storage_bytes') if row else None)
    a=rows['L4-body2-original-pruned0'];b=rows['L4-body2-original-pruned1e3'];c=rows['L4-body2-original-pruned1e2'];n=rows['L4-body6-normal-pruned0'];q=rows['L4-body6-normal-pruned1e3']
    assert a['identity']==b['identity']==c['identity'] and n['identity']==s['identity']
    assert c['iterations']==3000 and c['final_residual']['true_residual']>1e-10 and not c['numerically_accepted']
    compare=json.loads((D/'comparisons.json').read_text());assert not compare['approximate_graph_adopted'] and not compare['matched_l8_attempted']
    pressure=json.loads((D/'pressure-readback.json').read_text());assert pressure['diagnostic_accepted'] and not pressure['physical_accuracy_certified'] and len(pressure['pairs'])==3
    for path,h in pressure['input_sha256'].items():assert sha(Path(path))==h
    for name,h in pressure['source_sha256'].items():assert sha(R/name)==h==sha(D/'pressure-readback-source'/Path(name).name)
    cp=Path(compare['matched_controls']['fresh_complete_vs_zero_pruning']['complete_receipt']);cr,complete=verify_receipt(cp);frozen(cp)
    assert complete['identity']==n['identity'] and complete['final_residual']['true_residual']<1e-10
    assert complete['preconditioner']['rounded_values_sha256']==n['preconditioner']['rounded_values_sha256']
    assert sha(cp)==compare['matched_controls']['fresh_complete_vs_zero_pruning']['complete_receipt_sha256']
    for group in ('original','normal'):
        lhs=f'L4-body{2 if group=="original" else 6}-{group}-pruned0';rhs=lhs[:-1]+'1e3'
        if lhs in fields and rhs in fields:
            with np.load(fields[lhs],allow_pickle=False) as x,np.load(fields[rhs],allow_pickle=False) as y:
                for key in ('vertices_m','tetrahedra','velocity_doflocs_m','pressure_doflocs_m'):np.testing.assert_array_equal(x[key],y[key])
                for key in ('velocity_coefficients','pressure_coefficients'):
                    observed=float(np.linalg.norm(x[key]-y[key])/np.linalg.norm(x[key]));assert observed==compare['comparisons'][rhs]['field_relative_differences'][key]
                assert np.linalg.norm(x['velocity_coefficients']-y['velocity_coefficients'])/np.linalg.norm(x['velocity_coefficients'])<1e-8
                pair=next(z for z in pressure['pairs'] if z['base']==lhs and z['comparison']==rhs)
                assert pair['coefficient_relative_difference']==compare['comparisons'][rhs]['field_relative_differences']['pressure_coefficients'] and not pair['velocity_or_pressure_forward_accuracy_certified']
    for stem,path in receipts.items():assert sha(Path(path))==measurements[stem]['receipt_sha256']
    sources=sorted((R/'scripts').glob('*pruned*.py'))+[R/'scripts/cfd_reference3d_strength_survey.py',R/'tests/test_cfd_reference3d_pruned_graph.py',R/'docs/cfd_3d_pruned_graph_checkpoint.md',R/'docs/cfd_3d_preconditioner_correction_goal.md',R/'docs/cfd_3d_normal_force_next_goal.md']
    result=dict(status='compensated_graph_measured_not_adopted',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,approximate_graph_adopted=False,matched_l8_attempted=False,test_count=6,predecessor_sha256=previous['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,source_sha256={str(p.relative_to(R)):sha(p) for p in sources},support_test_receipt_sha256=sha(D/'support-test-receipt.json'),strength_receipt_sha256={str(survey_path):sha(survey_path)},strength_statistics=stats,measurements=measurements,comparison_sha256=sha(D/'comparisons.json'),pressure_readback_sha256=sha(D/'pressure-readback.json'),fresh_complete_control_receipt_sha256={str(cp):sha(cp)},committed=False,packaged=False,installed=False)
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),status=result['status'])))

if __name__=='__main__':main()
