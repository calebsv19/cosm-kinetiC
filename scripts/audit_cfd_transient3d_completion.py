#!/usr/bin/env python3
"""Seal C3D-7 only after numerical, exact-worker agent, control and cost evidence."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'build/c3d-wall'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text())


def main():
    numerical = read(OUT/'numerical-checkpoint.json')
    dependency = read(OUT/'dependency-checkpoint.json')
    evidence = read(OUT/'agent-evidence/qualification.json')
    audit = read(OUT/'agent-evidence/readback-audit.json')
    worker = ROOT/'build/cfd-optimized/physics_sim_session_worker'
    identity = sha(worker)
    assert numerical['numerical_matrix_passed']
    assert evidence['agent_matrix_passed'] and audit['agent_readback_passed']
    assert identity == evidence['worker_sha256'] == audit['worker_sha256'] == dependency['worker_sha256']
    assert len(evidence['records']) == len(audit['records']) == 47
    root = Path(evidence['evidence_root'])
    assert sha(root/'worker_snapshot') == identity
    sources = {**numerical['source_sha256'], **dependency['source_sha256']}
    for file, digest in sources.items():
        assert sha(ROOT/file) == digest, file
    for file, digest in numerical['evidence_sha256'].items():
        assert sha(OUT/file) == digest, file
    for file, digest in dependency['evidence_sha256'].items():
        assert sha(OUT/file) == digest, file
    assert sha(Path(dependency['static_dependency']['path'])) == dependency['static_dependency']['sha256']
    assert 'json-c' not in subprocess.check_output(['otool','-L',str(worker)], text=True)
    # Published fields, references, every-cell divergence, separate refinement and
    # outlet/phase screens were reconstructed by the independent readback audit.
    records = {r['case']:r for r in evidence['records']}
    cost = {}
    for case, record in records.items():
        state = record['status']; health = state['health']
        assert state['state'] == 'completed' and state['json_library_version'] == '0.18'
        assert health['linear_relative_residual'] <= 1e-11
        assert health['max_abs_divergence_s_inv'] < 1e-8
        assert record['assessment']['numerical_status'] == 'passed'
        assert record['assessment']['physical_accuracy_certified'] is False
        assert record['total_wall_s'] is not None and record['total_wall_s'] > 0
        memory = health['numerical_memory']
        assert 0 < memory['live_bytes'] <= memory['peak_bytes'] <= memory['limit_bytes']
        assert health['process_peak_rss_bytes'] > 0
        runtime = state['runtime_cost']
        assert runtime['final_export_wall_ms'] > 0
        assert runtime['checkpoint_service_count'] > 0
        assert runtime['checkpoint_service_wall_ms'] >= 0
        for point in state.get('history', []):
            energy = point['energy_budget']
            if not energy['available']:
                continue
            balance = (energy['physical_boundary_power_w']+energy['body_force_power_w']
                       -energy['transport_power_w']-energy['physical_strain_dissipation_w']
                       -energy['kinetic_energy_rate_w'])
            assert abs(balance-energy['residual_w']) < 1e-12
            if case.startswith('b-'):
                assert abs(energy['transport_self_power_w']) < 1e-12
                assert point['health']['transport_cfl'] <= .25
        for artifact in record['result']['artifacts']:
            assert sha(Path(artifact['path'])) == artifact['sha256']
        if case in ('a-spatial32','b-spatial32','b-phase32','c-spatial32-t0.5',
                    'c-spatial32-t2','c-spatial32-t12','co-spatial32','co-outlet6','co-outlet8'):
            assert record['assessment']['reference_accuracy']['status'] == 'passed'
            cost[case] = {'total_wall_s':record['total_wall_s'], 'last_step_wall_ms':state['step_ms'],
                          'last_step_process_cpu_ms':health['phase_cost']['mixed_solve_cpu_ms'],
                          'numerical_peak_bytes':memory['peak_bytes'],
                          'process_peak_rss_bytes':health['process_peak_rss_bytes'], **runtime}
    assert records['a-spatial8']['assessment']['reference_accuracy']['status'] == 'failed'
    assert records['c-spatial8-t0.5']['assessment']['reference_accuracy']['status'] == 'failed'
    assert records['b-phase32']['assessment']['harmonic_accuracy']['status'] == 'passed'
    for file in ('qualification-ab.json','qualification-phase.json','qualification-startup.json',
                 'qualification-open-transient.json'):
        assert read(OUT/file)['passed'] is True
    logs = {
        'json-bound-transient-agent.log':5, 'json-bound-periodic-agent.log':4,
        'json-bound-open-agent.log':2, 'json-bound-refined-agent.log':5,
        'json-bound-owner-boundary.log':2, 'json-bound-session-agent.log':9,
    }
    for file, count in logs.items():
        content = (OUT/file).read_text()
        assert re.search(r'Ran '+str(count)+r' tests in ', content) and content.rstrip().endswith('OK'), file
    contract = (OUT/'wall-final-contract.log').read_text()
    assert 'nonzero wall pressure gradient, conservative transport, cached steps' in contract
    assert 'safe cancellation, CFL and allocation cleanup passed' in contract
    startup = (OUT/'startup-final-contract.log').read_text()
    assert 'startup reference tails/energy, open operators, cached steps, cancellation and cleanup passed' in startup
    budget = [json.loads(line) for line in (OUT/'budget.log').read_text().splitlines() if line.startswith('{')]
    assert {row['mode_index'] for row in budget} == {0,1,2,3}
    assert all(row['one_byte_below_rejected'] and row['exact_peak_bytes'] > 0 for row in budget)
    session = (OUT/'transient-session.log').read_text()
    sanitizer = (OUT/'transient-session-sanitize.log').read_text()
    assert 'complete accepted-state cancellation preservation passed' in session and session == sanitizer
    controls = [json.loads(line[line.index('{'):]) for line in
                (OUT/'json-bound-transient-agent.log').read_text().splitlines()
                if '"c3d7_control_measurement"' in line]
    memory = [json.loads(line[line.index('{'):]) for line in
              (OUT/'json-bound-transient-agent.log').read_text().splitlines()
              if '"c3d7_inspection_memory"' in line]
    assert len(controls) == len(memory) == 1
    assert controls[0]['full_field_exact'] and controls[0]['cancel_receipt_wall_ms'] < 2000
    assert memory[0]['growth_bytes'] < memory[0]['growth_limit_bytes'] and memory[0]['json_version'] == '0.18'
    ownership = [json.loads(line) for line in (OUT/'json-ownership-green.log').read_text().splitlines() if line.startswith('{')]
    assert len(ownership) == 1 and ownership[0]['live_growth_bytes'] < ownership[0]['limit_bytes']
    preserved = {}
    baseline = read(ROOT/'build/s4-resolution/completion-audit.json')['current_source_sha256']
    baseline.update(read(ROOT/'build/c3d/completion-audit.json')['source_sha256'])
    baseline.update(read(ROOT/'build/c3d-open/completion-audit.json')['source_sha256'])
    # App/session adapters legitimately changed. Preserve the established native
    # 2D and prior periodic/open 3D numerical implementation, including headers.
    for file, digest in baseline.items():
        if (file.startswith(('src/app/cfd_', 'include/app/cfd_')) and
            not any(part in file for part in ('session', 'observation', 'wall3d', 'startup3d', 'mixed3d'))):
            if file == 'include/app/cfd_sparse_mg.h':
                # C3D-1 added only the declared backward-compatible XYZ constructor.
                header = (ROOT/file).read_text()
                start = header.index('/* Z-aware')
                end = header.index('void cfd_sparse_mg_destroy', start)
                original_header = header[:start]+header[end:]
                assert hashlib.sha256(original_header.encode()).hexdigest() == digest
                preserved[file] = {'current_sha256':sha(ROOT/file), 'original_2d_sha256':digest,
                                   'additive_3d_constructor_only':True}
            else:
                assert sha(ROOT/file) == digest, file
                preserved[file] = digest
    assert preserved
    canonical = ROOT.parent.parent/'physics_sim'
    assert subprocess.check_output(['git','status','--porcelain'], cwd=canonical, text=True) == ''
    assert subprocess.check_output(['git','rev-parse','HEAD'], cwd=canonical, text=True).strip().startswith('098294d')
    assert subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip() == '4e322dae80c7b70fa6fba27015f55617559da69f'
    subprocess.run(['git','diff','--check'], cwd=ROOT, check=True)
    # Requirement review ties the inspected implementation to the full original
    # contract. Readback/test assertions above are the quantitative evidence.
    requirements = {
        'A_continuous_three_component_wall_reference': {
            'source':'cfd_wall3d_reference.c: curl potential, averaged derivatives and nonzero normal pressure gradient',
            'evidence':['wall-final-contract.log','qualification-ab.json','agent-evidence/readback-audit.json']},
        'A_coupled_physical_pressure_and_wall_treatment': {
            'source':'cfd_mixed3d.c: integrated B/-B^T, component dual volumes, H symmetry, Schur solve and complete residual acceptance',
            'evidence':['wall-final-contract.log','new-sanitize.log','agent-evidence/readback-audit.json']},
        'A_separate_spatial_and_timestep_refinement': {
            'evidence':['qualification-ab.json','agent-evidence/readback-audit.json: spatial_orders/temporal']},
        'B_shared_flux_and_actual_CFL': {
            'source':'cfd_wall3d.c: shared dual-face transport, BE/BDF2 extrapolation, global component CFL and kinetic self-work',
            'evidence':['wall-final-contract.log','qualification-ab.json','agent-evidence/qualification.json']},
        'B_separate_amplitude_phase_pressure_wall_energy': {
            'source':'cfd_3d_harmonic.c: accepted-step offset/sine/cosine fit; observation metrics expose orthogonal and physical energy separately',
            'evidence':['qualification-phase.json','transient-session.log','agent-evidence/readback-audit.json']},
        'C_pressure_driven_rest_startup_independent_continuous_reference': {
            'source':'cfd_startup3d.c: rest, natural Pin/Pout, all X faces unknown, no body forcing; separate Fourier reference source',
            'evidence':['startup-final-contract.log','qualification-startup.json','agent-evidence/readback-audit.json']},
        'C_early_intermediate_late_and_steady_recovery': {
            'evidence':['qualification-startup.json: spatial/temporal/steady_recovery','agent-evidence/readback-audit.json']},
        'C_controlled_nonuniform_natural_traction_and_outlet_extensions': {
            'source':'cfd_wall3d.c: fixed 4 m wavelength, continuous natural end traction and half-slab forcing',
            'evidence':['qualification-open-transient.json','agent-evidence/readback-audit.json: open_outlet_changes/startup_outlet_changes']},
        'shared_scene_agent_inspection_assessment_and_artifacts': {
            'evidence':['transient-session.log','json-bound-transient-agent.log: actual MCP all four modes, Pa images/XYZ probes, reconnect and digest readback','agent-evidence/qualification.json']},
        'safe_controls_failed_candidate_rejection_and_accepted_diagnostics': {
            'source':'private candidates accepted only after mixed residual gates; adapter freezes transport diagnostics and worker preserves ordered cancel identity',
            'evidence':['transient-session.log','json-bound-transient-agent.log','json-bound-owner-boundary.log','json-bound-session-agent.log']},
        'memory_cache_dependency_cleanup_and_measured_cost': {
            'evidence':['budget.log','wall-final-contract.log','startup-final-contract.log','json-ownership-green.log','json-bound-transient-agent.log','agent-evidence/qualification.json']},
        'preserved_2D_periodic3D_and_C3D6': {
            'evidence':['bridge-preserved-regression.log','json-bound-periodic-agent.log','json-bound-open-agent.log','json-bound-refined-agent.log'],
            'source_sha256':preserved},
    }
    bindings = {str(path.relative_to(ROOT)):sha(path) for path in
                [OUT/'numerical-checkpoint.json',OUT/'dependency-checkpoint.json',
                 OUT/'agent-evidence/qualification.json',OUT/'agent-evidence/readback-audit.json']}
    for file in list(logs)+['json-ownership-green.log','budget.log','wall-final-contract.log',
                           'startup-final-contract.log','transient-session.log','transient-session-sanitize.log',
                           'bridge-preserved-regression.log','json-bound-native-build.log']:
        bindings[str((OUT/file).relative_to(ROOT))] = sha(OUT/file)
    output = {'schema':'physics_sim_c3d7_completion_audit_v1','date':'2026-09-30',
              'original_ABC_goal_achieved':True,'requirements':requirements,'worker_sha256':identity,
              'static_json_dependency':dependency['static_dependency'],'source_sha256':sources,
              'evidence_sha256':bindings,'cost':cost,'control_measurement':controls[0],
              'inspection_memory':memory[0],'canonical_changed':False,'committed':False,
              'packaged':False,'desktop_refreshed':False,'stop_before':'C3D-8',
              'limits':['controlled uniform-grid laminar known-answer flows only',
                        'startup is temporal profile development, not axial entrance',
                        'open transient has continuous prescribed natural tractions, no general wake/backflow certificate',
                        'no obstacle, body drag, moving bodies, turbulence, GPU or local 3D refinement',
                        'bounded serial CPU timings; no largest-grid control-latency guarantee',
                        'macOS JSON cleanup screen and source binding; no Linux runtime/dependency certificate',
                        'ASan/UBSan and numerical cleanup pass; LeakSanitizer unavailable on macOS']}
    (OUT/'completion-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({'original_ABC_goal_achieved':True,'requirements':len(requirements),
                      'retained_agent_cases':47,'worker_sha256':identity}))


if __name__ == '__main__':
    main()
