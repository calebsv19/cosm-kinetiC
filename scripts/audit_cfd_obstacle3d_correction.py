#!/usr/bin/env python3
"""C3D-8 correction audit; retain failed references and unchanged acceptance gates."""
import argparse
import json
from pathlib import Path
from audit_cfd_obstacle3d import ROOT, DATA, sha, check, error, reference_gate, screen, distance

CORRECTION = DATA / 'correction-v1'

def load(path):
    return json.loads(path.read_text())

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--require-qualified', action='store_true')
    args = parser.parse_args()
    matrix = load(DATA/'agent-evidence/qualification.json')
    predecessor = load(CORRECTION/'predecessor-qualification.json')
    rows = {r['case']:r for r in matrix['records']}
    previous = {r['case']:r for r in predecessor['records']}
    assert set(rows)==set(previous)=={'n8','n16','n32','n48','outlet6','outlet8','inlet4'}
    worker_hash = matrix['worker_sha256']
    assert sha(ROOT/'build/cfd-optimized/physics_sim_session_worker')==worker_hash
    assert sha(Path(matrix['evidence_root'])/'worker_snapshot')==worker_hash
    unchanged_fields = {name:r['binary_sha256']==previous[name]['binary_sha256'] for name,r in rows.items()}
    assert all(unchanged_fields.values())
    old = load(DATA/'predecessor-sources.json')
    bridges = {'src/app/cfd_3d_session.c','src/app/cfd_3d_observation.c','include/app/cfd_3d_session.h'}
    changed = [p for p,h in old.items() if sha(ROOT/p)!=h]
    assert set(changed)<=bridges
    before = load(CORRECTION/'predecessor-source-sha256.json')
    assert sha(ROOT/'src/app/cfd_obstacle3d_mixed.c')==before['src/app/cfd_obstacle3d_mixed.c']
    extended = load(CORRECTION/'extended40/qualification.json')
    extended['case']='extended40'
    assert extended['worker_sha256']==worker_hash and extended['cells']==256000
    for r in [*rows.values(),extended]:
        assert r['readback_max_difference']==0 and not r['assessment']['physical_accuracy_certified']
        for artifact in r['result']['artifacts']:
            assert sha(Path(artifact['path']))==artifact['sha256']
    ref = lambda name:load(CORRECTION/(name+'.json'))
    legacy_l4 = load(DATA/'reference8-edges-final-l11.json')
    short_gate = reference_gate(load(DATA/'reference8-edges-final-l10.json'),legacy_l4)
    groups = {}
    for label,suffix in [('unmirrored',''),('grad_div_mu','-gd01'),('reflection_symmetric','-mirror')]:
        a,b = ref('reference-graded10'+suffix),ref('reference-graded12'+suffix)
        gate = reference_gate(a,b)
        gate['transverse_force_ratio'] = max(abs(b['pressure_force_n'][k]+b['raw_symmetric_viscous_force_n'][k]) for k in (1,2))/abs(b['reaction_force_n'][0])
        gate['divergence_l2_s_inv_m_3_2']=b['divergence_l2_s_inv_m_3_2']
        gate['free_relative_residual']=b['free_relative_residual']
        groups[label]=gate
    native = [screen(rows[f'n{n}'],legacy_l4) for n in (8,16,32,48)]
    long_screens = {label:screen(extended,ref('reference-graded12'+suffix))
                    for label,suffix in [('unmirrored',''),('reflection_symmetric','-mirror')]}
    all_numerical = [screen(r,legacy_l4) for r in [*rows.values(),extended]]
    assert all(r['numerical_status']=='passed' for r in all_numerical)
    gradient = load(DATA/'native-empty32-calibration.json')['status']['physics']['pressure_drop_pa']/4
    downstream = distance(rows['outlet6'],rows['outlet8'],gradient)
    upstream = distance(rows['outlet8'],rows['inlet4'],gradient)
    monotonic = {}
    for key in ('pressure_force','viscous_force','total_force','dissipation'):
        values = [r['physical'][key]['value'] for r in native]
        monotonic[key]={'errors':values,'status':'passed' if all(a>b for a,b in zip(values,values[1:])) else 'failed',
                        'scope':'all four declared short grids against retained unresolved L4 reference'}
    calibration = ref('reference-empty-graded10-tight')
    continuous = load(CORRECTION/'predecessor-completion-audit.json')['empty_continuous_pressure_pa']*2
    calibration_checks = {'pressure':check(error(calibration['inlet_pressure_pa'],continuous),.01),
                          'dissipation':check(error(calibration['physical_dissipation_w'],continuous*.008),.01)}
    equivalence=ref('reference-equivalence')
    assert equivalence['passed'] and max(equivalence['relative_errors'].values())<1e-7
    reconstruction=ref('reconstruction-qualification')
    assert reconstruction['continuous_quadrature_passed']
    logs={}
    for name in ('native-contracts.log','regression.log','reconstruction-test.log','worker-build.log','agent-matrix.log','extended40.log'):
        p=CORRECTION/name;s=p.read_text()
        assert not any(x in s for x in ('FAILED (','Traceback (most recent','AssertionError','ERROR: AddressSanitizer','runtime error:'))
        logs[name]=sha(p)
    assert (CORRECTION/'native-contracts.log').read_text().count('cleanup passed')==2
    assert (CORRECTION/'regression.log').read_text().count('\nOK\n')==5
    assert '\nOK\n' in (CORRECTION/'reconstruction-test.log').read_text()
    failures={p.name:load(p) for p in CORRECTION.glob('*-receipt.json') if load(p)['returncode']!=0}
    blockers=[]
    if short_gate['status']!='passed':blockers.append('matched L4 independent reference remains unresolved')
    if groups['reflection_symmetric']['status']!='passed':blockers.append('symmetric L8 reference final viscous change exceeds 1%')
    for name,c in long_screens['reflection_symmetric']['physical'].items():
        if c['status']!='passed':blockers.append('extended40 symmetric-reference screening: '+name)
    if any(c['status']!='passed' for c in native[-1]['physical'].values()):blockers.append('short finest native physical screen')
    if any(c['status']!='passed' for c in monotonic.values()):blockers.append('declared native component refinement')
    if downstream['status']!='passed' or upstream['status']!='passed':blockers.append('boundary distance')
    qualified=not blockers and all(c['status']=='passed' for c in calibration_checks.values())
    production = set(old)|{'src/app/cfd_obstacle3d_reconstruction.c','src/app/cfd_obstacle3d.c',
      'src/app/cfd_obstacle3d_mixed.c','src/app/cfd_obstacle3d_observation.c','include/app/cfd_obstacle3d.h',
      'src/tools/cli/physics_sim_session_worker.c','make/sources-tools.mk','make/rules-tools.mk',
      'scripts/cfd_fem_reference3d_scalar.py','scripts/cfd_obstacle3d_correction_reference.py',
      'scripts/audit_cfd_obstacle3d_correction.py','scripts/probe_cfd_obstacle3d_strain.py',
      'scripts/verify_cfd_obstacle3d_extended40.py','tests/cfd_obstacle3d_reconstruction_probe.c',
      'tests/test_cfd_obstacle3d_reconstruction.py','docs/cfd_obstacle3d_correction_goal.md',
      'docs/cfd_obstacle3d_correction_checkpoint.md','docs/cfd_obstacle3d_checkpoint.md','docs/README.md','docs/current_truth.md','docs/agent_session.md','docs/cfd_3d_completion.md'}
    output={'schema':'physics_sim_c3d8_correction_audit_v1','status':'qualified' if qualified else 'implemented_unqualified',
      'physical_accuracy_certified':qualified,'worker_sha256':worker_hash,'blocking_gates':blockers,
      'native_fields_byte_unchanged':unchanged_fields,'predecessor_numerical_sources_preserved':len(old)-len(changed),
      'masked_mixed_solver_byte_unchanged':True,'native_agent_full_field_readback_cases':8,'maximum_readback_difference':0,
      'uniform_short_grid_screens':native,'extended40_screens':long_screens,'all_eight_numerical_screens':all_numerical,
      'reference_L4_inherited_unresolved':short_gate,'reference_controls':groups,
      'reference_storage_equivalence':equivalence,'reference_empty_calibration':calibration_checks,
      'reference_failures_retained':failures,'reconstruction_known_answer':reconstruction,
      'refinement':monotonic,'downstream_distance':downstream,'upstream_distance':upstream,
      'boundary_background_scope':'retained same-spacing empty duct; predecessor channel numerical sources byte-preserved',
      'limitations':['Stationary aligned cube, creeping Stokes only; no inertia, turbulence, arbitrary STL or moving bodies',
        'Interpolated velocity has nonzero pointwise divergence; exact MAC integrated continuity is a separate diagnostic',
        'Known-answer high-degree coarse field has large dissipation error and nonmonotonic viscous force error',
        'Symmetric reference viscous refinement fails; native pressure screen fails on matched long domain',
        'No commit, package, install, local native refinement or canonical adoption'],
      'source_sha256':{p:sha(ROOT/p) for p in sorted(production)},'log_sha256':logs,
      'reference_log_sha256':{p.name:sha(p) for p in CORRECTION.glob('reference*.log')},
      'evidence_sha256':{str(p.relative_to(DATA)):sha(p) for p in CORRECTION.rglob('*.json') if p.name!='completion-audit.json'}}
    (CORRECTION/'completion-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    # Current root points to this correction; historical audit is preserved above.
    (DATA/'completion-audit.json').write_text(json.dumps(output,indent=2)+'\n')
    print(json.dumps({k:output[k] for k in ('status','physical_accuracy_certified','blocking_gates','worker_sha256')}))
    if args.require_qualified and not qualified:raise SystemExit(1)

if __name__=='__main__':main()
