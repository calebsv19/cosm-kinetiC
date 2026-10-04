#!/usr/bin/env python3
"""Once-only audit of exact basis admission, complete fields and matched force recovery."""
import json,hashlib
from pathlib import Path
import numpy as np
from cfd_reference3d_flexible import basis_reservation
from audit_cfd_3d_spatial import verify_receipt,force_comparison
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-restart'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def frozen(p):
    r=json.loads(p.read_text());assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(p.parent/'source'/q)==h==sha(R/'scripts'/q)
    assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    if 'factor_build' in r:
        b=r['factor_build'];assert sha(p.parent/'source/factor.dylib')==r['factor_library_sha256']==b['library_sha256']
        assert sha(p.parent/'source/factor-build.json')==r['factor_build_record_sha256']
        assert b['source_sha256']==r['source_sha256']['cfd_reference3d_mixed_storage.c']
        assert all(x in b['command'] for x in ('-std=c11','-Wall','-Wextra','-Werror'))
    row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());return r,row

def budget(a,n,restart):
    assert a['basis_reservation_bytes']==basis_reservation(n,restart) and a['reserve_bytes']==32*1024**2
    assert a['estimated_numeric_stage_bytes']==sum(a[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes'))
    assert a['numeric_stage_admitted']==(a['estimated_numeric_stage_bytes']<=1800*1024**2)

def accepted(p,restart):
    r,row=verify_receipt(p);frozen(p)
    assert r['returncode']==0 and row['target']==1e-10 and row['final_residual']['true_residual']<1e-10
    assert row['outer_iteration']['restart']==restart
    f=row['preconditioner']['flexible_iteration'];assert f['restart']==restart and f['iterations']==row['iterations'] and f['basis_array_bytes']<f['basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],restart)
    pc=row['preconditioner'];assert pc['shared_input_preserved_after_solve'] and pc['user_factor_storage_verified'] and pc['pressure_control']['action_preserved'] and pc['pressure_control']['live_input_preserved']
    assert row['condensation']['full_load_residency']['restored_bitwise_after_factor_cleanup'] and row['condensation']['factor_metadata_residency']['catalogs_restored_bitwise']
    guard=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');budget(guard,row['condensed_free_dofs'],restart)
    return r,row

def main():
    out=D/'checkpoint-audit.json';assert not out.exists()
    predecessor=json.loads((D/'predecessor.json').read_text());assert sha(Path(predecessor['path']))==predecessor['sha256']=='9afff94708efc63d6474cf83362b043e47a6182df30663d5924ea4a5a2c96cfb'
    protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for q,h in protected.items():assert sha(R/q)==h
    baseline=json.loads((D/'baseline.json').read_text());changed=[q for q,h in baseline.items() if sha(R/q)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
    transforms=json.loads((D/'source-transform-control.json').read_text())
    for t in transforms:
        assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
        assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
    support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4
    for q,h in support['source_sha256'].items():assert sha(R/q)==h==sha(Path(support['frozen_source'])/Path(q).name)
    assert sha(D/'support-tests-01.log')==support['log_sha256'] and '\nOK\n' in (D/'support-tests-01.log').read_text()
    assert sha(D/'source-transform-control.json')==support['transformation_sha256']
    receipt_hashes={};measurements={};fields={}
    for count,group in ((2,'original'),(6,'normal')):
        rows=[]
        for restart in (60,24,12):
            stem=f'L4-body{count}-{group}-r{restart}';p=next((D/'runs').glob('*/'+stem+'-receipt.json'));r,row=accepted(p,restart);rows.append(row)
            receipt_hashes[str(p)]=sha(p);fields[stem]=row
            measurements[stem]=dict(wall_s=r['wall_s'],owned_peak_rss_bytes=row['peak_rss_bytes'],sampled_peak_rss_bytes=r['peak_observed_rss_bytes'],iterations=row['iterations'],full_residual=row['final_residual'],flexible_iteration=row['preconditioner']['flexible_iteration'])
        assert rows[0]['identity']==rows[1]['identity']==rows[2]['identity']
        assert rows[0]['preconditioner']['rounded_values_sha256']==rows[1]['preconditioner']['rounded_values_sha256']==rows[2]['preconditioner']['rounded_values_sha256']
    comparisons=json.loads((D/'comparisons.json').read_text())
    for stem,c in comparisons['calibration'].items():
        row=fields[stem];base=fields[stem.rsplit('r',1)[0]+'r60'];assert c['force_comparison']==force_comparison(row,base)
    for path,h in comparisons['input_sha256'].items():assert sha(Path(path))==h
    for name in ('import-complete.json','import-placeholder.json'):
        imp=json.loads((D/name).read_text());assert imp['import_only'] and not imp['placeholder_called'] and not imp['physical_algorithm_executed']
        assert imp['owned_peak_bytes']<1800*1024**2 and imp['wall_s']<180
        for q,h in imp['source_sha256'].items():assert sha(R/q)==h
    stages={};stagehashes={}
    for p in sorted((D/'stage-runs').glob('*/*-receipt.json')):
        r,row=frozen(p);assert r['returncode']==0 and row['diagnostic_accepted'] and not row['numeric_factor_attempted'] and not row['numerical_field_published']
        assert row['symbolic_handle_cleanup_verified'] and row['original_mixed_input_preserved'] and row['tetrahedra']==33216
        restart=row['outer_iteration']['restart'];budget(row['admission'],row['condensation']['reduced_free_dofs'],restart)
        stages[str(restart)]=row;stagehashes[str(p)]=sha(p)
    larger={};observers={}
    for p in sorted((D/'runs').glob('*/L8-*-receipt.json')):
        r,row=frozen(p);restart=row.get('outer_iteration',{}).get('restart')
        if r['returncode']==0:
            accepted(p,restart);assert row['tetrahedra']==33216 and row['identity']==stages[str(restart)]['identity']
        else:assert not p.with_name(p.name.removesuffix('-receipt.json')+'.npz').exists()
        receipt_hashes[str(p)]=sha(p);larger[str(p)]=dict(returncode=r['returncode'],wall_s=r['wall_s'],rss_bytes=r['peak_observed_rss_bytes'],row=row,diagnostic_failure=r['diagnostic_failure'])
    for p in sorted((D/'observer-runs').glob('*/*-receipt.json')):
        r,row=frozen(p);assert r['returncode']==0 and row['diagnostic_accepted'] and sha(Path(row['input_receipt']))==row['input_receipt_sha256']
        assert row['original_linear_residual']['true_residual']<1e-10
        identity=max(abs(x) for l in row['lifts'] for part in ('pressure','viscous') for x in l[part]['identity_error_n']);assert identity<1e-9
        assert sha(Path(row['signed_attribution_path']))==row['signed_attribution_sha256'];observers[str(p)]=dict(sha256=sha(p),maximum_identity_error_n=identity,volume_stress=row['volume_strong_equilibrium_defect_l2'],interior_jump=row['interior_stress_jump_l2'])
    sources=sorted((R/'scripts').glob('*restart*.py'))+[R/'tests/test_cfd_reference3d_restart.py',R/'docs/cfd_3d_restart_checkpoint.md',R/'docs/cfd_3d_restart_next_goal.md',R/'docs/cfd_3d_preconditioner_correction_goal.md',R/'docs/cfd_3d_restart_import_survey_goal.md']
    result=dict(status=comparisons['status'],persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,default_restart_changed=False,test_count=4,predecessor_sha256=predecessor['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,source_sha256={str(p.relative_to(R)):sha(p) for p in sources},support_receipt_sha256=sha(D/'support-test-receipt.json'),source_transformation_sha256=sha(D/'source-transform-control.json'),calibration_receipts_sha256=receipt_hashes,calibration=measurements,comparison_sha256=sha(D/'comparisons.json'),matched_stage_receipts_sha256=stagehashes,matched_stages={k:v['admission'] for k,v in stages.items()},larger=larger,observers=observers,optional_short_restart_adopted=comparisons['optional_short_restart_adopted'],import_survey_sha256={n:sha(D/n) for n in ('import-complete.json','import-placeholder.json')},committed=False,packaged=False,installed=False)
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
