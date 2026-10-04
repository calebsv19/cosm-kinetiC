"""Verify immutable cube fields, optional diagnostic integration and source isolation."""
import json
from pathlib import Path
import re
from run_cfd_native_accuracy_regression import require, save, sha
from assess_cfd_native_cube_pressure import field
from cfd_reference3d_directional_evidence import verify as directional_verify

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT/'build/c3d-native-cube-pressure'


def main():
    manifest = json.loads((DEST/'checkpoint-source/manifest.json').read_text())
    for name, digest in manifest.items():
        require(sha(ROOT/name) == digest == sha(DEST/'checkpoint-source'/name), 'New source drift: '+name)
    native = json.loads((DEST/'physical-assessment.json').read_text())
    require(sha(ROOT/'scripts/assess_cfd_native_cube_pressure.py') == native['assessor_sha256'], 'Assessor identity')
    require(not native['physical_accuracy_certified'] and not native['reference_raw_equilibrium_passed'], 'Certification boundaries')
    require(native['pressure_candidate_retained_as_optional_diagnostic'], 'Candidate utility')
    api = {}
    for key, row in native['native_fields'].items():
        path = Path(row['receipt']); field(path)
        require(sha(path)==row['receipt_sha256'], 'Field receipt changed')
        receipt = path.parent/'pressure-trace-api/receipt.json'
        data = json.loads(receipt.read_text()); contract = json.loads((receipt.parent/'contract.json').read_text())
        require(data['status']=='passed_native_pressure_trace_api_readback', 'Integrated pressure diagnostic parity')
        require(contract['input_receipt_sha256']==sha(path), 'API original field identity')
        original=json.loads((path.parent/'readback/receipt.json').read_text())
        require(data['control']==original['control'], 'All original numerical and physical controls unchanged')
        for name,digest in data['artifact_sha256'].items():
            require(sha(receipt.parent/name)==digest, 'API proof artifact drift')
        for name,digest in data['source_sha256'].items():
            require(sha(ROOT/name)==digest, 'API source drift')
        require(data['control']['peak_owned_bytes']<1024**3 and data['processes']['readback']['wall_s']<180 and
                data['processes']['readback']['peak_sampled_child_rss_bytes']<1536*1024**2, 'API readback caps')
        api[key] = dict(receipt_sha256=sha(receipt), status=data['status'])
    log = (DEST/'api-make.log').read_text()
    controls = [json.loads(line) for line in log.splitlines() if line.startswith('{"polynomial_face_controls"')]
    require(len(controls)==2 and controls[0]==controls[1], 'Ordinary/sanitized API controls')
    require(controls[0]['polynomial_face_controls']==660 and controls[0]['failure_controls']==26 and
            controls[0]['maximum_force_error_n']<1e-11 and controls[0]['maximum_gauge_error_n']<1e-11 and
            controls[0]['allocations']==0 and all(controls[0]['two_three_four_interval_patches']), 'Exact API physical/failure controls')
    anisotropic_root=ROOT/'build/c3d-pressure-trace-api-anisotropic'
    anisotropic=json.loads((anisotropic_root/'receipt.json').read_text())
    require(anisotropic['status']=='passed_anisotropic_displaced_box_pressure_diagnostic', 'Anisotropic box API')
    require(anisotropic['normal']==anisotropic['sanitize'] and anisotropic['normal']['polynomial_face_controls']==240 and
            anisotropic['normal']['failure_controls']==7 and anisotropic['normal']['maximum_force_error_n']<1e-11,
            'Anisotropic physical and failure controls')
    for name,digest in anisotropic['artifact_sha256'].items(): require(sha(anisotropic_root/name)==digest, 'Anisotropic artifact drift')
    ac=json.loads((anisotropic_root/'contract.json').read_text())
    for name,digest in ac['source_sha256'].items(): require(sha(ROOT/name)==digest, 'Anisotropic source drift')
    anisotropic_log=(DEST/'anisotropic-make.log').read_text()
    anisotropic_controls=[json.loads(line) for line in anisotropic_log.splitlines() if line.startswith('{"polynomial_face_controls"')]
    require(anisotropic_controls==[anisotropic['normal'],anisotropic['normal']], 'Anisotropic make integration')
    require('exact-cap cleanup passed' in log, 'Original obstacle contract')
    require(not re.search('ERROR: (AddressSanitizer|UndefinedBehaviorSanitizer)|runtime error:',log), 'Sanitizer evidence')
    regional = json.loads((ROOT/'build/c3d-regional-local/assessment.json').read_text())
    require(not any(r['geometry_screen_passed'] for r in regional['rows']) and not regional['numerical_factor_attempted'], 'Rejected mesh boundary')
    directional = json.loads((ROOT/'build/c3d-directional-quality/assessment.json').read_text())
    require('matched_edge047' in directional['accepted_paired_profiles'], 'Directional paired geometry')
    for data in (regional,directional):
        for name,digest in data['source_sha256'].items(): require(sha(ROOT/name)==digest, 'Geometry source drift')
    for name, data in (('c3d-regional-local',regional),('c3d-directional-quality',directional)):
        require(sha(ROOT/'build'/name/'geometry.npz')==data['geometry_sha256'], 'Geometry identity')
    traction = json.loads((ROOT/'build/c3d-directional-traction-polynomial/checkpoint-audit.json').read_text())
    require(traction['status']=='passed_actual_mesh_polynomial_traction_calibration' and
            len(traction['controls'])==8 and traction['maximum_face_load_error_n']<1e-9, 'Directional traction integration')
    for name,digest in traction['source_sha256'].items(): require(sha(ROOT/name)==digest, 'Traction calibration drift')
    directional_fields = {}
    assessments = sorted((ROOT/'build/c3d-directional-field').glob('L*-physical-assessment.json'))
    require(assessments, 'Terminal directional physical assessment')
    for path in assessments:
        assessment = json.loads(path.read_text()); receipt=Path(assessment['receipt']); _, row=directional_verify(receipt)
        require(sha(receipt)==assessment['receipt_sha256'], 'Directional terminal receipt drift')
        require(not assessment['physical_accuracy_certified'], 'Reference scope')
        directional_fields['L'+str(int(row['length']))] = dict(assessment_sha256=sha(path),receipt_sha256=sha(receipt),
            raw_surface_reaction_mismatch=assessment['same_domain_refinement']['raw_surface_reaction_relative_mismatch'],
            L8_trial_permitted=assessment['L8_trial_permitted'])
    if directional_fields['L4']['L8_trial_permitted']:
        require('L8' in directional_fields, 'Admitted paired domain remains unfinished')
    updated_path=DEST/'physical-directional-reference-assessment.json'
    updated=json.loads(updated_path.read_text())
    require(not updated['reference_raw_equilibrium_passed'] and not updated['physical_accuracy_certified'], 'Improved reference scope')
    require(set(updated['native_fields'])==set(native['native_fields']), 'Same native fields against improved reference')
    for key,record in updated['reference_fields'].items():
        require(abs(record['raw_surface_reaction_mismatch']-directional_fields[key]['raw_surface_reaction_mismatch'])<1e-15,
                'Improved reference raw-force identity')
    fine=json.loads((ROOT/'build/c3d-directional-fine-quality/assessment.json').read_text())
    require(fine['accepted_paired_profiles']==['matched_edge031'] and not fine['numerical_factor_attempted'], 'Next geometry scope')
    for name,digest in fine['source_sha256'].items(): require(sha(ROOT/name)==digest, 'Fine geometry source drift')
    require(sha(ROOT/'build/c3d-directional-fine-quality/geometry.npz')==fine['geometry_sha256'], 'Fine geometry identity')
    protected = {'build/cfd-optimized/physics_sim_session_worker':'c2a234ff0238f12b64c739ccbb1f6304e2ff3244613b125eb453fd40d08caf64',
                 'build/c3d-obstacle/completion-audit.json':'c97de90c740d36c75d3f334eed0d60231757b9ec1bcc22dcc952d4d71bfded36'}
    for name,digest in protected.items(): require(sha(ROOT/name)==digest, 'Protected native authority')
    baseline=json.loads((DEST/'baseline.json').read_text())
    changed=[name for name,digest in baseline.items() if not (ROOT/name).is_file() or sha(ROOT/name)!=digest]
    require(set(changed)=={'make/sources-tools.mk','make/rules-tools.mk','docs/README.md','docs/current_truth.md'}, 'Preexisting source drift: '+repr(changed))
    additions=json.loads((DEST/'build-additions.json').read_text())
    for name,record in additions.items():
        require(baseline[name]==record['before_sha256'] and sha(ROOT/name)==record['after_sha256'], 'Build addition changed')
    previous=json.loads((DEST/'predecessor.json').read_text())
    predecessors={'wall_checkpoint_sha256':'build/c3d-wall-shear/checkpoint-audit.json',
                  'graded_checkpoint_sha256':'build/c3d-accuracy-graded/checkpoint-audit.json'}
    for key,name in predecessors.items(): require(sha(ROOT/name)==previous[key],'Predecessor checkpoint drift')
    result=dict(status='passed_native_cube_pressure_and_directional_checkpoint',new_source_sha256=manifest,
        strict_physical_assessment_sha256=sha(DEST/'physical-assessment.json'),
        improved_reference_native_assessment_sha256=sha(updated_path),
        fine_geometry_assessment_sha256=sha(ROOT/'build/c3d-directional-fine-quality/assessment.json'), complete_native_fields=4,
        integrated_api_readbacks=api, api_physical_controls=controls[0],
        anisotropic_api_controls=anisotropic['normal'],anisotropic_receipt_sha256=sha(anisotropic_root/'receipt.json'),
        anisotropic_make_log_sha256=sha(DEST/'anisotropic-make.log'),make_log_sha256=sha(DEST/'api-make.log'),
        original_obstacle_contract_passed=True,sanitizers_passed=True,directional_fields=directional_fields,
        regional_geometry_assessment_sha256=sha(ROOT/'build/c3d-regional-local/assessment.json'),
        directional_geometry_assessment_sha256=sha(ROOT/'build/c3d-directional-quality/assessment.json'),
        directional_traction_calibration_sha256=sha(ROOT/'build/c3d-directional-traction-polynomial/checkpoint-audit.json'),
        protected_native_sha256=protected,changed_preexisting_files=changed,
        optional_pressure_diagnostic_source_integrated=True,default_force_observer_changed=False,
        solver_equations_changed=False,physical_accuracy_certified=False,persistent_goal_complete=False)
    save(DEST/'checkpoint-audit.json',result)
    print(json.dumps(dict(status=result['status'],native_fields=4,optional_source_diagnostic_integrated=True,
                          physical_accuracy_certified=False,directional_fields=directional_fields)))

if __name__=='__main__': main()
