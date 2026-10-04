#!/usr/bin/env python3
"""Audit bounded spatial controls and useful reference memory changes."""
import hashlib
import json
import math
from pathlib import Path
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-spatial'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(a,b):return abs(a/b-1)


def verify_receipt(path):
    """Verify retained success/failure, frozen sources, supervisor and resources."""
    receipt=json.loads(path.read_text());directory=path.parent
    for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
    for name,digest in receipt['source_sha256'].items():assert sha(directory/'source'/name)==digest
    bundle=hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert directory.name==bundle
    supervisor=directory.parent.parent/'supervisor-source'/(receipt['runner_sha256']+'.py')
    assert sha(supervisor)==receipt['runner_sha256']
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2
    assert receipt['wall_cap_s']==180 and receipt['linear_iteration_cap']==3000
    stem=path.name.removesuffix('-receipt.json');result_path=directory/(stem+'.json')
    snapshot=directory/(stem+'.npz');command=receipt['command']
    assert command[-4:]==['--snapshot',str(snapshot),'--output',str(result_path)]
    row=json.loads(result_path.read_text()) if result_path.exists() else None
    if receipt['returncode']==0:
        assert receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
        assert row is not None and row['numerically_accepted'] and not row['physical_accuracy_certified']
        assert row['tetrahedra']<=50000 and row['iterations']<=3000
        assert row['final_residual']['true_residual']<1e-8
        assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
        if 'peak_rss_bytes' in row:assert row['peak_rss_bytes']<1800*1024**2
        assert row['flux_error']<1e-8 and row['volume_divergence_max_s_inv']<1e-8
        assert row['physical_energy_imbalance']<.03
        with np.load(snapshot,allow_pickle=False) as saved:
            assert bool(saved['numerically_accepted'])
            assert np.all(np.isfinite(saved['velocity_coefficients']))
            assert np.all(np.isfinite(saved['pressure_coefficients']))
    else:
        assert receipt['diagnostic_failure'] is not None
        assert row is None or not row['numerically_accepted']
        assert not snapshot.exists()
    return receipt,row


def force_comparison(refined,base):
    components={k:relative(refined[k][0],base[k][0]) for k in
        ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n')}
    scalars={k:relative(refined[k],base[k]) for k in ('inlet_pressure_pa','physical_dissipation_w')}
    mismatch={name:relative(row['pressure_force_n'][0]+row['raw_symmetric_viscous_force_n'][0],row['reaction_force_n'][0])
              for name,row in (('base',base),('refined',refined))}
    return dict(component_relative_changes=components,scalar_relative_changes=scalars,
                raw_surface_reaction_relative_mismatch=mismatch,
                physical_force_gate_passed=max([*components.values(),*scalars.values(),*mismatch.values()])<=.01)


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    allowed={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'}
    changed=[p for p,d in baseline.items() if sha(ROOT/p)!=d]
    assert set(changed)<=allowed,changed
    records={};receipts={};paths={};all_receipts={}
    for path in (DATA/'runs').glob('*/*-receipt.json'):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json')
        assert name not in paths,name
        paths[name]=path;receipts[name]=receipt;records[name]=row;all_receipts[str(path)]=sha(path)
    required=('L4-body4-h03','L4-body4-insert03','L4-body4-insert03-normal',
        'L4-body6-insert03','L4-body4-insert03-chunk','L4-body6-insert03-chunk',
        'L4-body4-insert03-lean','L4-body4-insert03-csr','L4-body4-insert03-implicit',
        'L4-body6-insert03-implicit','L4-body6-insert03-normal-implicit','empty4-implicit','exact-cube-all-macro-modes',
        'exact-cube-mass-pc','exact-cube-macro-pc','exact-cube-patch-pc',
        'L4-body6-insert03-edge04-implicit')
    assert set(required)<=paths.keys()
    full=records['L4-body4-insert03'];implicit=records['L4-body4-insert03-implicit']
    csr=records['L4-body4-insert03-csr']
    assert implicit['identity']==csr['identity']
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):
        assert full['identity'][key]==implicit['identity'][key]
    equivalence=force_comparison(implicit,full)
    assert max([*equivalence['component_relative_changes'].values(),*equivalence['scalar_relative_changes'].values()])<1e-7
    with np.load(paths['L4-body4-insert03'].with_name('L4-body4-insert03.npz')) as a, np.load(paths['L4-body4-insert03-implicit'].with_name('L4-body4-insert03-implicit.npz')) as b:
        fields={key:float(np.max(np.abs(a[key]-b[key]))) for key in ('velocity_coefficients','pressure_coefficients')}
        assert fields['velocity_coefficients']<1e-8 and fields['pressure_coefficients']<1e-6
    measured_memory_reduction=1-receipts['L4-body4-insert03-implicit']['peak_observed_rss_bytes']/receipts['L4-body4-insert03']['peak_observed_rss_bytes']
    assert measured_memory_reduction>.25
    for name in ('L4-body6-insert03','L4-body6-insert03-chunk'):
        assert receipts[name]['diagnostic_failure']['kind']=='resource_cap'
        assert receipts[name]['returncode']!=0
    large=records['L4-body6-insert03-implicit'];normal=records['L4-body6-insert03-normal-implicit']
    assert large['tetrahedra']==23616 and normal['tetrahedra']==28416
    assert normal['axis_nodes_m'][1:]==large['axis_nodes_m'][1:]
    assert len(normal['axis_nodes_m'][0])==len(large['axis_nodes_m'][0])+2
    refinement={'body4_normal':force_comparison(records['L4-body4-insert03-normal'],full),
        'body4_to_body6':force_comparison(large,implicit),
        'body6_normal':force_comparison(normal,large)}
    assert not refinement['body4_normal']['physical_force_gate_passed']
    edge=records['L4-body6-insert03-edge04-implicit']
    if edge is not None and edge['numerically_accepted']:
        assert edge['tetrahedra']==28224 and edge['edge_passes']==1 and edge['edge_radius_m']==.04
        refinement['body6_edges']=force_comparison(edge,large)
    empty=records['empty4-implicit']
    s=sum(1/(n*n*m*m*((n*math.pi/2)**2+(m*math.pi/2)**2)) for n in range(1,256,2) for m in range(1,256,2))
    exact=4*.008/(64*4*s/(.1*math.pi**4))
    calibration={k:relative(empty[k],target) for k,target in
        (('inlet_pressure_pa',exact),('physical_dissipation_w',exact*.008))}
    assert max(calibration.values())<.01
    modal=records['exact-cube-all-macro-modes']['diagnostics']
    local=modal['local_macro_pressure_modes'];global_modes=modal['pressure_modes']
    assert local['macro_count']==1248 and local['all_mean_zero_modes_detected']
    assert local['maximum_constant_gradient_relative_error']<1e-12
    assert local['minimum_positive_generalized_eigenvalue']>1e-6
    assert global_modes['near_null_modes_below_1e_12']==0
    assert min(global_modes['smallest_eigenvalues'])>1e-3
    original_path=next((ROOT/'build/c3d-preconditioner/runs').glob('*/cube-factor.json'))
    original=json.loads(original_path.read_text())
    exact=records['exact-cube-all-macro-modes']
    assert exact['identity']['mesh_sha256']==original['identity']['mesh_sha256']
    exact_equivalence=force_comparison(exact,original)
    assert max([*exact_equivalence['component_relative_changes'].values(),*exact_equivalence['scalar_relative_changes'].values()])<1e-7
    mass_case=records['exact-cube-mass-pc'];failed_candidate=records['exact-cube-macro-pc'];patch=records['exact-cube-patch-pc']
    assert mass_case['identity']==failed_candidate['identity']==patch['identity']==exact['identity']
    assert failed_candidate['iterations']==3000 and not failed_candidate['numerically_accepted']
    assert receipts['exact-cube-macro-pc']['diagnostic_failure']['kind']=='linear_iteration_cap'
    assert patch['iterations']>mass_case['iterations']
    assert receipts['exact-cube-patch-pc']['wall_s']>receipts['exact-cube-mass-pc']['wall_s']
    candidate_equivalence=force_comparison(patch,mass_case)
    assert max([*candidate_equivalence['component_relative_changes'].values(),*candidate_equivalence['scalar_relative_changes'].values()])<1e-7
    # Protect all predecessor artifacts, not just the audit JSON.
    predecessor=json.loads((ROOT/'build/c3d-preconditioner/completion-audit.json').read_text())
    for p,d in predecessor['reference_receipts_sha256'].items():
        assert sha(Path(p))==d
        verify_receipt(Path(p))
    previous=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,d in previous['protected_build_hashes'].items():assert sha(ROOT/p)==d
    tests=DATA/'focused-tests.log';text=tests.read_text()
    assert text.count('Ran 5 tests')==2 and 'Ran 4 tests' in text and 'Ran 2 tests' in text and text.count('\nOK\n')==4
    assert 'Traceback' not in text and 'FAILED (' not in text
    costs={name:{'wall_s':receipts[name]['wall_s'],
        'peak_observed_rss_bytes':receipts[name]['peak_observed_rss_bytes'],
        'own_peak_rss_bytes':records[name].get('peak_rss_bytes') if records[name] else None,
        'iterations':records[name].get('iterations') if records[name] else None,
        'tetrahedra':records[name].get('tetrahedra') if records[name] else None,
        'failure':receipts[name]['diagnostic_failure']} for name in required}
    files=[p for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*')
        if p.is_file() and str(p.relative_to(ROOT)) not in baseline]
    audit=dict(schema='physics_sim_c3d_spatial_audit_v1',status='memory_improvement_verified_force_testing_continues',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,
        native_source_unchanged=True,low_memory_cube_operator_identity=implicit['identity'],
        measured_rss_reduction=measured_memory_reduction,matched_control_equivalence=equivalence,
        matched_field_maximum_absolute_changes=fields,refinement=refinement,empty_fourier_calibration=calibration,
        pressure_mode_diagnostics=modal,exact_cube_force_equivalence=exact_equivalence,
        working_pressure_preconditioner='mass',experimental_pressure_candidates_adopted=False,
        candidate_matched_force_equivalence=candidate_equivalence,
        costs=costs,reference_receipts_sha256=all_receipts,
        new_source_sha256={str(p.relative_to(ROOT)):sha(p) for p in files},
        changed_preexisting_files=changed,baseline_sha256=sha(DATA/'baseline.json'),test_log_sha256=sha(tests),
        committed=False,packaged=False,installed=False,
        next_gate='pressure trace spatial convergence; retain component and raw-surface/reaction gates before native reference use')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','measured_rss_reduction','matched_field_maximum_absolute_changes','refinement','empty_fourier_calibration')}),flush=True)


if __name__=='__main__':main()
