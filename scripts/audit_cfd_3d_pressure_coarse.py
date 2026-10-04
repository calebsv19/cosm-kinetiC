#!/usr/bin/env python3
"""Once-only matched pressure-preconditioner cost, strict field and source audit."""
import json,hashlib
from pathlib import Path
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from cfd_reference3d_flexible import basis_reservation
from cfd_reference3d_pressure_coarse import reserve
import audit_cfd_3d_restart as parent
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-pressure-coarse';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(p):
    r,row=parent.frozen(p);verify_receipt(p)
    if r['returncode']:
        assert not p.with_name(p.name.removesuffix('-receipt.json')+'.npz').exists()
        if row.get('resource_phase_rejected',{}).get('phase')=='numeric_stage_admission':
            g=row['resource_phase_rejected'];assembly=next(q for q in r['progress'] if q.get('phase')=='assembled');n=assembly['reduced_free_dofs'];np_=assembly['condensed_pressure_dofs']
            assert g['outer_basis_reservation_bytes']==basis_reservation(n,6) and g['coarse_pressure_reservation_bytes']==reserve(n-np_,np_) and g['basis_reservation_bytes']==g['outer_basis_reservation_bytes']+g['coarse_pressure_reservation_bytes']
            assert g['reserve_bytes']==32*2**20 and g['estimated_numeric_stage_bytes']==sum(g[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes')) and not g['numeric_stage_admitted'] and g['estimated_numeric_stage_bytes']>1800*2**20
            assert not any(q.get('phase') in ('workspace_numeric_ready','factor_ready','solve','solve_complete') for q in r['progress'])
        return r,row
    assert row['target']==1e-10 and row['retained_target']==1e-11 and row['final_residual']['true_residual']<1e-10 and row['linear_solve_accepted']
    f=row['preconditioner']['flexible_iteration'];assert f['restart']==6 and f['iterations']==row['iterations'] and f['final_true_metric']<=1e-11 and f['basis_array_bytes']<f['basis_reservation_bytes']==basis_reservation(row['condensed_free_dofs'],6)
    assert row['preconditioner']['shared_input_preserved_after_solve'] and row['preconditioner']['user_factor_storage_verified'] and row['preconditioner']['pressure_control']['action_preserved']
    m=row['condensation'];assert m['full_load_residency']['restored_bitwise_after_factor_cleanup'] and m['factor_metadata_residency']['catalogs_restored_bitwise']
    guard=next(q for q in r['progress'] if q.get('phase')=='numeric_stage_admission');base=basis_reservation(row['condensed_free_dofs'],6);extra=reserve(row['condensed_free_dofs']-m['condensed_pressure_dofs'],m['condensed_pressure_dofs']) if row['coarse_pressure']=='quadratic' else 0
    assert guard['outer_basis_reservation_bytes']==base and guard['coarse_pressure_reservation_bytes']==extra and guard['basis_reservation_bytes']==base+extra
    assert guard['reserve_bytes']==32*2**20 and guard['estimated_numeric_stage_bytes']==sum(guard[k] for k in ('factor_storage_bytes','numeric_workspace_bytes','current_rss_before_numeric_bytes','reserve_bytes','basis_reservation_bytes')) and guard['numeric_stage_admitted'] and guard['estimated_numeric_stage_bytes']<=1800*2**20
    if extra:
        pc=row['pressure_preconditioner'];assert pc['columns']==10 and pc['constant_pressure_direction_retained'] and pc['coarse_reservation_bytes']==extra and pc['coarse_relative_skew']<1e-5 and pc['coarse_reproduction_relative_error']<1e-5 and pc['coarse_smallest_eigenvalue']>0
    return r,row

def compare(ar,a,br,b):
    c=force_comparison(b,a);eq=max([*c['component_relative_changes'].values(),*c['scalar_relative_changes'].values()])<1e-7
    setup_solve_a=a['timings']['setup_s']+a['timings']['solve_s'];setup_solve_b=b['timings']['setup_s']+b['timings']['solve_s']
    it=1-b['iterations']/a['iterations'];time=1-setup_solve_b/setup_solve_a;whole=br['wall_s']/ar['wall_s'];rss=b['peak_rss_bytes']/a['peak_rss_bytes']
    return dict(force_comparison=c,physical_equivalence_accepted=eq,iterations_relative_reduction=it,setup_solve_relative_reduction=time,whole_time_ratio=whole,owned_peak_ratio=rss,cost_gate_passed=(it>=.1 or time>=.1) and whole<=1.1 and rss<=1.1,physical_accuracy_certified=False)

def main():
    out=D/'checkpoint-audit.json';assert not out.exists();p=json.loads((D/'predecessor.json').read_text());assert sha(Path(p['path']))==p['sha256']=='542c0bd7cf1c709863128aa37a560ecdecaaaf32155618d6c2ec0df116241d27'
    native=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for f,h in native.items():assert sha(R/f)==h
    baseline=json.loads((D/'baseline.json').read_text());changed=[f for f,h in baseline.items() if sha(R/f)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'}
    support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==4
    for f,h in support['source_sha256'].items():assert sha(R/f)==h==sha(Path(support['frozen_source'])/Path(f).name)
    assert sha(D/support['log'])==support['log_sha256'] and '\nOK\n' in (D/support['log']).read_text() and sha(D/'support-tests-01.log')==support['initial_failure_log_sha256'] and sha(D/'source-transform-control.json')==support['transformation_sha256']
    for t in json.loads((D/'source-transform-control.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
        assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
    pairs={};measurements={};receipts={};eligible=True
    for count,group in ((2,'original'),(6,'normal')):
        rows=[]
        for mode in ('mass','quadratic'):
            name=f'L4-body{count}-{group}-{mode}';rp=next((D/'runs').glob('*/'+name+'-receipt.json'));r,row=check(rp);rows.append((r,row));receipts[str(rp)]=sha(rp)
            prior=next((R/'build/c3d-retained-margin/runs').glob(f'*/L4-body{count}-{group}-r6-receipt.json'));_,old=verify_receipt(prior);assert row['identity']==old['identity'];receipts[str(prior)]=sha(prior)
            measurements[name]=dict(returncode=r['returncode'],whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20 if 'peak_rss_bytes' in row else None,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],timings=row.get('timings'),pressure_preconditioner=row.get('pressure_preconditioner'))
        if all(r['returncode']==0 for r,row in rows):
            ar,a=rows[0];br,b=rows[1];assert a['identity']==b['identity'] and a['preconditioner']['rounded_values_sha256']==b['preconditioner']['rounded_values_sha256'];pairs[group]=compare(ar,a,br,b)
            eligible &= pairs[group]['physical_equivalence_accepted'] and pairs[group]['cost_gate_passed']
        else:pairs[group]=dict(cost_gate_passed=False,physical_equivalence_accepted=False);eligible=False
    matched={}
    eligibility=json.loads((D/'matched-eligibility.json').read_text())
    assert eligibility['matched_eligible']==eligible and eligibility['pairs']==pairs
    for f,h in eligibility['input_sha256'].items():assert sha(Path(f))==h
    control=json.loads((D/'matched-control-receipt.json').read_text());assert control['eligibility_sha256']==sha(D/'matched-eligibility.json') and control['transform_sha256']==sha(D/'matched-transform-control.json')
    for f,h in control['source_sha256'].items():assert sha(R/f)==h==sha(Path(control['frozen_source'])/Path(f).name)
    t=json.loads((D/'matched-transform-control.json').read_text());assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
    for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
    assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
    for rp in (D/'matched-runs').glob('*/*-receipt.json'):
        assert eligible;r,row=check(rp);receipts[str(rp)]=sha(rp)
        prior=next((R/'build/c3d-retained-margin/runs').glob('*/L8-body6-second-normal-held-outer2-r6-receipt.json'));ar,a=verify_receipt(prior);receipts[str(prior)]=sha(prior)
        actual_identity=row.get('identity') or next(q['identity'] for q in r['progress'] if q.get('phase')=='assembled')
        assert actual_identity==a['identity'];matched[str(rp)]=dict(returncode=r['returncode'],row=row)
        if r['returncode']==0:matched[str(rp)]['comparison']=compare(ar,a,r,row)
    matched_adopted=bool(matched) and all(v['returncode']==0 and v['comparison']['cost_gate_passed'] and v['comparison']['physical_equivalence_accepted'] for v in matched.values())
    c=dict(optional_pressure_correction_adopted=matched_adopted,calibration=pairs,measurements=measurements,matched_eligible=bool(eligible),matched=matched,original_physical_gate_replaced=False,physical_accuracy_certified=False);cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(c,indent=2)+'\n')
    sources=list((R/'scripts').glob('*pressure_coarse*.py'))+[R/'tests/test_cfd_reference3d_pressure_coarse.py']+list((R/'docs').glob('cfd_3d_pressure_coarse*.md'))
    result=dict(status='PROGRESS: measured bounded pressure correction controls; original physical force qualification open',persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,predecessor_sha256=p['sha256'],native_hashes_preserved=native,baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,source_sha256={str(f.relative_to(R)):sha(f) for f in sources},receipt_sha256=receipts,support_receipt_sha256=sha(D/'support-test-receipt.json'),transformation_sha256=sha(D/'source-transform-control.json'),comparison_sha256=sha(cp),matched_eligible=bool(eligible),optional_pressure_correction_adopted=matched_adopted,matched_control_sha256=sha(D/'matched-control-receipt.json'),matched_eligibility_sha256=sha(D/'matched-eligibility.json'),committed=False,packaged=False,installed=False)
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),matched_eligible=eligible)))
if __name__=='__main__':main()
