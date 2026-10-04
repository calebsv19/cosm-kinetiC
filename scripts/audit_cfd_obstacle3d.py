#!/usr/bin/env python3
"""Audit C3D-8 evidence without treating numerical convergence as certification."""
import argparse
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'build/c3d-obstacle'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(name):
    return json.loads((DATA / name).read_text())


def error(value, reference):
    return abs(value - reference) / max(abs(reference), 1e-30)


def check(value, limit):
    return dict(value=value, limit=limit,
                status='passed' if math.isfinite(value) and value <= limit else 'failed')


def reference_gate(previous, final):
    changes = {key: check(error(final[key][0], previous[key][0]), .01)
               for key in ('pressure_force_n', 'raw_symmetric_viscous_force_n')}
    changes['dissipation'] = check(error(final['physical_dissipation_w'],
                                        previous['physical_dissipation_w']), .01)
    integrated = final['pressure_force_n'][0] + final['raw_symmetric_viscous_force_n'][0]
    changes['reaction_vs_integrated'] = check(error(integrated, final['reaction_force_n'][0]), .01)
    return dict(status='passed' if all(c['status'] == 'passed' for c in changes.values())
                else 'not_established', checks=changes,
                pressure_pa=final['inlet_pressure_pa'], reaction_force_n=final['reaction_force_n'][0],
                tetrahedra=final['tetrahedra'], velocity_dofs=final['velocity_dofs'],
                pressure_dofs=final['pressure_dofs'], wall_s=final['wall_s'],
                peak_rss_bytes=final['peak_rss_bytes'])


def screen(record, reference):
    state = record['status']
    f, e, h = (state[k] for k in ('boundary_force_budget', 'energy_budget', 'health'))
    p, v = f['body_pressure_force_n'][0], f['body_viscous_force_n'][0]
    rp, rv = reference['pressure_force_n'][0], reference['raw_symmetric_viscous_force_n'][0]
    physical = dict(pressure_force=check(error(p, rp), .05),
                    viscous_force=check(error(v, rv), .05),
                    total_force=check(error(p + v, rp + rv), .05),
                    dissipation=check(error(e['physical_strain_dissipation_w'], reference['physical_dissipation_w']), .03),
                    inlet_pressure=check(error(state['physics']['pressure_drop_pa'], reference['inlet_pressure_pa']), .03),
                    momentum_closure=check(f['momentum_relative_residual'], .02),
                    energy_imbalance=check(e['physical_relative_imbalance'], .03))
    numerical = dict(momentum=check(h['linear_relative_residual'], 1e-11),
                     divergence=check(h['max_abs_divergence_s_inv'], 1e-8),
                     flux=check(h['flux_conservation_relative_error'], 1e-9),
                     discrete_energy=check(e['discrete_relative_imbalance'], 1e-9),
                     discrete_momentum=check(max(map(abs, f['discrete_momentum_residual_n'])) /
                                              (4*state['physics']['pressure_drop_pa']), 1e-9),
                     transverse_force=check(max(map(abs, f['body_total_force_n'][1:])) / abs(p+v), 1e-7))
    return dict(case=record['case'], grid=record['request']['grid'],
                physical_comparison_scope='screen against unresolved same-domain FEM reference; not certification',
                physical=physical, numerical=numerical,
                numerical_status='passed' if all(c['status']=='passed' for c in numerical.values()) else 'failed',
                values=dict(pressure_force_n=p, viscous_force_n=v, total_force_n=p+v,
                            dissipation_w=e['physical_strain_dissipation_w'],
                            inlet_pressure_pa=state['physics']['pressure_drop_pa']),
                cost=dict(agent_wall_s=record['agent_wall_s'], native_wall_s=record['native_wall_s'],
                          step_ms=state['step_ms'], numerical_peak_bytes=h['numerical_memory']['peak_bytes'],
                          process_peak_rss_bytes=h['process_peak_rss_bytes'],
                          fluid_cells=h['fluid_cells'], stored_face_values=h['stored_face_values'],
                          iterations=h['linear_iterations'], inner_iterations=h['velocity_inner_iterations'],
                          **state['runtime_cost']))


def distance(first, second, gradient):
    a, b = first['status'], second['status']
    af, bf = a['boundary_force_budget'], b['boundary_force_budget']
    checks = {key: check(error(bf[key][0], af[key][0]), .01)
              for key in ('body_pressure_force_n', 'body_viscous_force_n', 'body_total_force_n')}
    la, lb = a['physics']['dimensions_m'][0], b['physics']['dimensions_m'][0]
    checks['excess_inlet_pressure'] = check(error(b['physics']['pressure_drop_pa']-gradient*lb,
                                                a['physics']['pressure_drop_pa']-gradient*la), .01)
    ca = first['native']['center_x']; cb = second['native']['center_x']
    ap = {round(p['x_m']-ca, 8): p for p in a['centerline_recovery']}
    bp = {round(p['x_m']-cb, 8): p for p in b['centerline_recovery']}
    keys = sorted(ap.keys() & bp.keys())
    assert len(keys) >= 3
    for quantity in ('vx_m_s', 'pressure_deficit_pa'):
        def value(p, length):
            return p['vx_m_s'] if quantity=='vx_m_s' else p['pressure_pa']-gradient*(length-p['x_m'])
        aa, bb = [value(ap[k], la) for k in keys], [value(bp[k], lb) for k in keys]
        # Common physical body-relative probes; subtract the matched empty-duct
        # pressure background so a longer downstream gauge is not solver error.
        change = max(abs(x-y) for x,y in zip(aa,bb)) / max(max(map(abs, aa)), 1e-30)
        checks[quantity] = check(change, .01)
    return dict(status='passed' if all(c['status']=='passed' for c in checks.values()) else 'failed',
                checks=checks, common_body_relative_probes_m=keys,
                pressure_normalization='same-spacing empty duct G; excess Pin=Pin-G*L; pressure deficit=p-G*(L-x)',
                empty_gradient_pa_m=gradient)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--require-qualified', action='store_true')
    args = parser.parse_args()
    matrix = read('agent-evidence/qualification.json')
    rows = {r['case']: r for r in matrix['records']}
    assert set(rows)=={'n8','n16','n32','n48','outlet6','outlet8','inlet4'}
    worker = Path(matrix['evidence_root']) / 'worker_snapshot'
    assert sha(worker)==matrix['worker_sha256']==sha(ROOT/'build/cfd-optimized/physics_sim_session_worker')
    for r in rows.values():
        assert r['readback_max_difference']==0 and not r['assessment']['physical_accuracy_certified']
        for artifact in r['result']['artifacts']:
            assert sha(Path(artifact['path']))==artifact['sha256']
    old = read('predecessor-sources.json')
    allowed = {'src/app/cfd_3d_session.c','src/app/cfd_3d_observation.c','include/app/cfd_3d_session.h'}
    changed = [p for p,digest in old.items() if sha(ROOT/p)!=digest]
    assert set(changed) <= allowed, changed
    reference = read('reference8-edges-final-l11.json')
    rg = reference_gate(read('reference8-edges-final-l10.json'), reference)
    extended = read('reference8-L8-cx4-edge-l10.json')
    extended_gate = reference_gate(read('reference8-L8-cx4-edge-l9.json'), extended)
    native = [screen(rows[f'n{n}'], reference) for n in (8,16,32,48)]
    extended_screen = screen(rows['inlet4'], extended)
    empty = read('native-empty32-calibration.json')
    assert empty['worker_sha256']==matrix['worker_sha256']
    gradient = empty['status']['physics']['pressure_drop_pa']/4
    downstream = distance(rows['outlet6'], rows['outlet8'], gradient)
    upstream = distance(rows['outlet8'], rows['inlet4'], gradient)
    monotonic = {}
    for key in ('pressure_force','viscous_force','total_force','dissipation'):
        values = [r['physical'][key]['value'] for r in native]
        monotonic[key] = dict(errors=values, status='passed' if all(x>y for x,y in zip(values,values[1:])) else 'failed')
    # A chosen three-grid subset cannot be selected after seeing errors to hide
    # the declared 8/16/32 viscous error reversal. Record all four grids.
    fourier_sum = sum(1/(n*n*m*m*((n*math.pi/2)**2+(m*math.pi/2)**2))
                      for n in range(1,256,2) for m in range(1,256,2))
    empty_pressure = 4*.008/(64*4*fourier_sum/(.1*math.pi**4))
    calibration = read('reference-empty8-final.json')
    checks = dict(empty_pressure=check(error(calibration['inlet_pressure_pa'], empty_pressure), .01),
                  empty_dissipation=check(error(calibration['physical_dissipation_w'], empty_pressure*.008), .01))
    required_logs = {'contracts-final.log':'cleanup passed','agent-controls-mcp-final.log':'Ran 3 tests',
                     'preserved-regression-final.log':'mixed channel units','source-gui-build.log':None}
    for filename, phrase in required_logs.items():
        path = DATA/filename
        assert path.exists() and (phrase is None or phrase in path.read_text())
        text = path.read_text()
        assert not any(marker in text for marker in ('FAILED (', 'Traceback (most recent', 'AssertionError', 'ERROR: AddressSanitizer', 'runtime error:'))
    assert (DATA/'contracts-final.log').read_text().count('cleanup passed') == 2
    assert '\nOK\n' in (DATA/'agent-controls-mcp-final.log').read_text()
    assert (DATA/'preserved-regression-final.log').read_text().count('\nOK\n') == 4
    qualified = (rg['status']=='passed' and extended_gate['status']=='passed'
                 and all(c['status']=='passed' for c in checks.values())
                 and all(c['status']=='passed' for c in native[-1]['physical'].values())
                 and all(c['status']=='passed' for c in extended_screen['physical'].values())
                 and all(r['numerical_status']=='passed' for r in native+[extended_screen])
                 and all(m['status']=='passed' for m in monotonic.values())
                 and downstream['status']=='passed' and upstream['status']=='passed')
    source_paths = set(old) | set(allowed)
    source_paths.update(['include/app/cfd_obstacle3d.h','src/app/cfd_obstacle3d.c',
                         'src/app/cfd_obstacle3d_mixed.c','src/app/cfd_obstacle3d_observation.c',
                         'src/tools/cli/physics_sim_session_worker.c','scripts/agent_session/cartesian3d.py',
                         'scripts/agent_session/service.py','scripts/agent_session/protocol.py',
                         'scripts/cfd_fem_reference3d.py','scripts/verify_cfd_obstacle3d_agent.py',
                         'scripts/cfd_obstacle3d_reference_bounded.py','scripts/audit_cfd_obstacle3d.py',
                         'tests/cfd_obstacle3d_test.c','tests/cfd_obstacle3d_contract_test.c',
                         'tests/test_agent_obstacle3d.py','make/sources-tools.mk','make/rules-tools.mk',
                         'docs/cfd_obstacle3d_goal.md','docs/cfd_obstacle3d_checkpoint.md'])
    output = dict(schema='physics_sim_c3d8_boundary_audit_v1', physical_accuracy_certified=qualified,
                  status='qualified' if qualified else 'implemented_unqualified',
                  worker_sha256=matrix['worker_sha256'], evidence_matrix_sha256=sha(DATA/'agent-evidence/qualification.json'),
                  predecessor_cfd_sources_preserved=len(old)-len(changed), changed_adapter_sources=changed,
                  empty_continuous_pressure_pa=empty_pressure, reference_calibration=checks,
                  reference_L4=rg, reference_L8_center4=extended_gate, uniform_grid_screens=native,
                  final_extended_domain_screen=extended_screen, refinement=monotonic,
                  downstream_distance=downstream, upstream_distance=upstream,
                  stages={
                      '8A': {'status':'implemented_reference_unresolved',
                             'scope':'same-domain calibrated independent reference; separate force convergence unpassed'},
                      '8B': {'status':'implemented_numerical_agent_contract_verified',
                             'scope':'stationary masked solver, closed diagnostics, controls/MCP and fields; no physical certificate'},
                      '8C': {'status':'executed_physical_gate_unpassed',
                             'scope':'four grids, inlet/outlet and reference comparison; thresholds unchanged'}},
                  numerical_maxima_all_seven_cases={
                      'true_relative_momentum_residual': max(r['status']['health']['linear_relative_residual'] for r in rows.values()),
                      'max_abs_divergence_s_inv': max(r['independent_export_divergence'] for r in rows.values()),
                      'flux_relative_error': max(r['status']['health']['flux_conservation_relative_error'] for r in rows.values()),
                      'discrete_energy_relative_imbalance': max(r['status']['energy_budget']['discrete_relative_imbalance'] for r in rows.values())},
                  blocking_gates=['independent separate reference forces <=1% changes',
                                  'native separate viscous force <=5% and physical D <=3%',
                                  'physical momentum closure <=2%',
                                  'declared grid component error monotonicity',
                                  'matching finest extended-domain force/energy proof'],
                  native_agent_full_field_readback_cases=7, maximum_readback_difference=0,
                  source_sha256={p:sha(ROOT/p) for p in sorted(source_paths)},
                  evidence_sha256={p.name:sha(p) for p in DATA.glob('*.json') if p.name!='completion-audit.json'},
                  log_sha256={name:sha(DATA/name) for name in required_logs},
                  limitations=['Stationary aligned 1 m cube; creeping Stokes, no inertial transport',
                               'Unresolved separate reference forces: comparison errors are screening estimates',
                               'Native force component, physical dissipation and momentum gates remain unpassed',
                               'No arbitrary STL, moving body, turbulence, adaptive 3D, GPU, package or install qualification'])
    (DATA/'completion-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({k:output[k] for k in ('status','physical_accuracy_certified','worker_sha256',
                                          'predecessor_cfd_sources_preserved','downstream_distance','upstream_distance')}))
    if args.require_qualified and not qualified:
        raise SystemExit(1)


if __name__=='__main__':
    main()
