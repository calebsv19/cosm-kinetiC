"""Verify sealed graded CFD evidence and current documentation without a new solve."""
import hashlib,json,re,subprocess
from pathlib import Path
from assess_cfd_obstacle3d_pressure_attribution import native_trace
from cfd_reference3d_accuracy_graded_evidence import verify,physical_comparison
R=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def main():
    out=R/'build/c3d-accuracy-graded/closeout-verification.json';assert not out.exists()
    names=('c3d-accuracy-graded','c3d-cubic-pressure-projection','c3d-accuracy-admission-guard','c3d-cubic-pressure-attribution')
    audits={name:read(R/'build'/name/'checkpoint-audit.json') for name in names}
    for name,a in audits.items():
        assert a['persistent_goal_complete'] is False
        for q,h in a['source_sha256'].items():assert sha(R/q)==h
    for name in ('c3d-accuracy-admission-guard','c3d-cubic-pressure-attribution'):
        a=audits[name];d=R/'build'/name
        for q,h in a['source_sha256'].items():assert sha(d/'frozen'/q)==h
    guard=audits['c3d-accuracy-admission-guard'];d=R/'build/c3d-accuracy-admission-guard'
    assert sha(d/'tests.log')==guard['test_log_sha256'] and sha(d/'transforms.json')==guard['transforms_sha256'] and guard['tests_passed']==3
    for t in read(d/'transforms.json'):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
        assert v==(R/t['output']).read_text()
    assert not guard['new_numerical_field_run'] and not guard['physical_equations_changed']
    graded=audits['c3d-accuracy-graded'];assessment=read(R/'build/c3d-accuracy-graded/physical-assessment.json')
    assert sha(R/'build/c3d-accuracy-graded/physical-assessment.json')==graded['physical_assessment_sha256']
    fields={}
    for L,item in assessment['fields'].items():
        p=Path(item['receipt']);assert sha(p)==item['receipt_sha256'];_,fields[L]=verify(p)
        assert not assessment['same_domain_refinement'][L]['raw_equilibrium_passed'] and assessment['same_domain_refinement'][L]['force_change_passed']
    assert physical_comparison(fields['L4'],fields['L8'],'domain_sensitivity')==assessment['domain_sensitivity']
    pa=audits['c3d-cubic-pressure-projection'];assert sha(R/'build/c3d-cubic-pressure-projection/support/receipt.json')==pa['support_receipt_sha256']
    support=read(R/'build/c3d-cubic-pressure-projection/support/receipt.json');assert sha(R/'build/c3d-cubic-pressure-projection/support/tests.log')==support['test_log_sha256']
    for q,h in support['source_sha256'].items():assert sha(R/'build/c3d-cubic-pressure-projection/support/frozen'/q)==h
    for obs in pa['observations']:
        p=Path(obs['receipt']);assert sha(p)==obs['receipt_sha256'];rec=read(p)
        assert rec['returncode']==0 and not rec['diagnostic_failure'] and not rec['stop_reason']
        for q,h in rec['artifact_sha256'].items():assert sha(Path(q))==h
        for q,h in rec['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
    attr=read(R/'build/c3d-cubic-pressure-attribution/assessment.json')
    assert sha(R/'build/c3d-cubic-pressure-attribution/assessment.json')==audits['c3d-cubic-pressure-attribution']['assessment_sha256']
    assert sha(R/'build/c3d-obstacle/agent-evidence/qualification.json')==attr['native_qualification_sha256']
    assert sha(R/'build/c3d-cubic-pressure-projection/checkpoint-audit.json')==attr['projection_audit_sha256']
    assert not attr['physical_accuracy_certified'] and not audits['c3d-cubic-pressure-attribution']['new_native_run']
    for row in attr['records']:
        p=Path(row['native_binary']);assert sha(p)==row['native_binary_sha256']
        trace=native_trace(p,2.);assert abs(trace['force_n']-row['archived_native_pressure_force_n'])<1e-12
        assert trace['slab_averages_pa']==row['native_slab_averages_pa']
        assert abs(row['reconstruction_error_n']+row['field_functional_error_n']-row['total_functional_error_n'])<1e-15
        assert not row['complete_pressure_field_error_norm_measured']
    native=graded['native_hashes_preserved']
    for q,h in native.items():assert sha(R/q)==h
    baseline=read(R/'build/c3d-accuracy-graded/baseline.json')
    changed=[q for q,h in baseline.items() if not (R/q).exists() or sha(R/q)!=h]
    assert set(changed)=={'docs/current_truth.md','docs/README.md'}
    docs=('docs/current_truth.md','docs/README.md','docs/cfd_3d_graded_accuracy_assessment.md','docs/cfd_3d_accuracy_graded_checkpoint.md','docs/cfd_3d_cubic_projection_checkpoint.md')
    targets=('cfd_3d_graded_accuracy_assessment.md','cfd_3d_accuracy_graded_checkpoint.md','cfd_3d_cubic_projection_checkpoint.md')
    for q in docs:
        text=(R/q).read_text()
        for link in re.findall(r'\]\(([^)]+)\)',text):
            if link.split('#')[0] in targets:assert ((R/q).parent/link.split('#')[0]).is_file()
    text=(R/docs[2]).read_text()
    assert all(term in text for term in ('1.4037%','1.4066%','-3.5620%','-4.7673%','-8.3293%','Twelve new support controls','steady Stokes'))
    result=dict(status='GRADED ACCURACY CHECKPOINT VERIFIED; PHYSICAL QUALIFICATION OPEN',persistent_goal_complete=False,tests_passed=12,new_accepted_reference_fields=2,pressure_projection_observations=2,archived_native_attribution=True,new_native_run=False,checkpoint_sha256={name:sha(R/'build'/name/'checkpoint-audit.json') for name in names},source_sha256={str(Path(__file__).relative_to(R)):sha(Path(__file__))},current_document_sha256={q:sha(R/q) for q in docs},changed_preexisting_files=sorted(changed),native_hashes_preserved=native,physical_accuracy_certified=False,next_physical_steps=['near-edge reference stress convergence to original raw1% gate','native pressure/operator and force reconstruction known-answer controls'],memory_write_skipped='No human request to write memory',package_or_install_performed=False)
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(path=str(out),sha256=sha(out),status=result['status'],tests_passed=12,changed_preexisting_files=changed)))
if __name__=='__main__':main()
