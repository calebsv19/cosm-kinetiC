"""Seal complete graded stress attribution and prospective geometry controls."""
import json,hashlib
from pathlib import Path
import numpy as np
from cfd_reference3d_force_attribution import summarize_signed
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-graded-stress'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    out=D/'checkpoint-audit.json';assert not out.exists();s=json.loads((D/'support.json').read_text());assert s['tests_passed']==2 and sha(D/'tests.log')==s['test_log_sha256'] and sha(D/'transforms.json')==s['transforms_sha256']
    for q,h in s['source_sha256'].items():assert sha(R/q)==h==sha(D/'support-frozen'/q)
    for t in json.loads((D/'transforms.json').read_text()):
        assert sha(R/t['parent'])==t['parent_sha256'] and sha(R/t['output'])==t['output_sha256'];v=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in v;v=v.replace(a,b)
        assert v==(R/t['output']).read_text()
    survey=json.loads((D/'edge-geometry-survey.json').read_text());assert sha(D/'edge-geometry-survey.json')==s['geometry_survey_sha256'] and sha(D/'edge-geometry.npz')==s['geometry_archive_sha256']==survey['archive_sha256'] and all(survey['translated_inner_cells_match'].values())
    assert len(survey['records'])==8 and all(q['physical_geometry_passed'] for q in survey['records']) and sum(q['prospective_quality_passed'] for q in survey['records'])==4 and not survey['numerical_factor_attempted'] and not survey['flow_field_published']
    pp=list((D/'observer-runs').glob('*/*-receipt.json'));assert len(pp)==1;p=pp[0];receipt=json.loads(p.read_text());assert receipt['schema']=='physics_sim_c3d_graded_signed_receipt_v1' and receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None and receipt['wall_s']<1800 and receipt['peak_observed_rss_bytes']<3072*2**20
    for q,h in receipt['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in receipt['source_sha256'].items():assert sha(R/'scripts'/q)==h==sha(p.parent/'source'/q)
    assert p.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest() and sha(D/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    row=json.loads(Path(receipt['command'][receipt['command'].index('--output')+1]).read_text());assert row['diagnostic_accepted'] and row['input_complete_numerical_gates_passed'] and sha(Path(row['input_receipt']))==row['input_receipt_sha256'] and sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
    original_receipt=json.loads(Path(row['input_receipt']).read_text());original=json.loads(Path(original_receipt['command'][original_receipt['command'].index('--output')+1]).read_text());assert original['final_residual']['true_residual']<1e-10
    with np.load(Path(row['input_snapshot']),allow_pickle=False) as field,np.load(Path(row['signed_attribution_path']),allow_pickle=False) as saved:
        summary,net,score=summarize_signed(saved['weighted_volume_divergence_n'],saved['interior_jump_n'],saved['adjacency'],saved['cell_centers_m'],saved['face_centers_m'],field['lo'],field['hi'],saved['shells_m'])
        assert summary==row['signed_force_attribution'];np.testing.assert_array_equal(net,saved['cell_net_n']);np.testing.assert_array_equal(score,saved['cell_score_n'])
    fractions=[]
    for q in row['lifts']:
        for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
            values=q[part];np.testing.assert_allclose(values['raw_surface_load_n'],original[key],rtol=0,atol=1e-10);assert max(abs(v) for v in values['identity_error_n'])<1e-9
        assert max(abs(v) for v in q['total_identity_error_n'])<1e-9
    for q in summary['lifts']:fractions.append(abs(q['centroid_bands'][0]['signed_total_n'][0]/q['signed_total_gap_n'][0]))
    result=dict(status='GRADED STRESS IDENTITY CLOSED; EDGE040 FIELD PERMITTED',tests_passed=2,source_sha256={**s['source_sha256'],**{f'scripts/{q}':h for q,h in receipt['source_sha256'].items()},'scripts/audit_cfd_3d_graded_stress.py':sha(Path(__file__))},support_sha256=sha(D/'support.json'),observer_receipt=str(p),observer_receipt_sha256=sha(p),observer_wall_s=receipt['wall_s'],observer_peak_mib=receipt['peak_observed_rss_bytes']/2**20,signed_force_gaps_n=[q['signed_total_gap_n'][0] for q in summary['lifts']],near_edge_signed_gap_fractions=fractions,maximum_identity_error_n=max(abs(v) for q in row['lifts'] for v in q['total_identity_error_n']),geometry_survey_sha256=sha(D/'edge-geometry-survey.json'),geometry_archive_sha256=sha(D/'edge-geometry.npz'),physical_geometry_controls_passed=8,prospective_quality_controls_passed=4,physical_accuracy_certified=False,persistent_goal_complete=False,scope='independent signed stress identity on the accepted graded L4 field; centroid band allocations and rankings are diagnostics rather than force-error bounds')
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],sha256=sha(out),near_edge_fractions=fractions,identity=result['maximum_identity_error_n'])))
if __name__=='__main__':main()
