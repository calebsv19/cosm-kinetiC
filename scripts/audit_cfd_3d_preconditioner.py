#!/usr/bin/env python3
"""Audit unchanged equations, bounded solver readiness and resumed force testing."""
import hashlib
import json
import math
from pathlib import Path
import numpy as np
from run_cfd_reference3d_preconditioner import SOURCES
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-preconditioner'


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def relative(a,b):return abs(a/b-1)


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    allowed={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'}
    changed=[p for p,d in baseline.items() if sha(ROOT/p)!=d]
    assert set(changed)<=allowed,changed
    hashes={n:sha(ROOT/'scripts'/n) for n in SOURCES}
    bundle=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    directory=DATA/'runs'/bundle
    names=('cube-amg-diagnostic','cube-factor','cube-factor-tight','empty-factor','cube-factor-normal')
    records={};receipts={}
    for name in names:
        p=directory/(name+'-receipt.json');receipt=json.loads(p.read_text());receipts[name]=receipt
        for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
        for n,digest in receipt['source_sha256'].items():assert sha(directory/'source'/n)==digest
        assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2
        assert receipt['wall_cap_s']==180 and receipt['linear_iteration_cap']==3000
        assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
        assert receipt['stop_reason'] is None
        assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        row=json.loads((directory/(name+'.json')).read_text());records[name]=row
        assert row['iterations']<=3000 and row['tetrahedra']<=50000
        if name=='cube-amg-diagnostic':
            assert receipt['returncode']==2 and not row['numerically_accepted']
            assert row['iterations']==1000 and not (directory/(name+'.npz')).exists()
        else:
            assert receipt['returncode']==0 and row['numerically_accepted']
            assert row['final_residual']['true_residual']<1e-8
            assert row['volume_divergence_max_s_inv']<1e-8 and row['flux_error']<1e-8
            assert row['physical_energy_imbalance']<.03
            with np.load(directory/(name+'.npz'),allow_pickle=False) as saved:
                assert bool(saved['numerically_accepted'])
                assert np.all(np.isfinite(saved['velocity_coefficients']))
                assert np.all(np.isfinite(saved['pressure_coefficients']))
            if name!='empty-factor':
                assert np.max(np.abs(row['normal_viscous_force_n']))<1e-8
                assert max(face['boundary_divergence_l2_s_inv_m'] for face in row['consistency_diagnostics']['faces'])<1e-8
            assert not row['physical_accuracy_certified']
    base=records['cube-factor'];tight=records['cube-factor-tight'];control=records['cube-amg-diagnostic']
    assert base['identity']==control['identity']==tight['identity']
    assert base['tetrahedra']==4992 and base['velocity_dofs']==76362 and base['pressure_dofs']==49920
    assert base['final_residual']['true_residual']<=1e-10
    residual=control['final_residual'];dominance=residual['momentum_relative_to_rhs']/residual['continuity_relative_to_rhs']
    assert dominance>50
    modal=control['diagnostics']['pressure_modes']
    assert modal['dimension']==1248 and modal['near_null_modes_below_1e_12']==0
    assert min(modal['smallest_eigenvalues'])>1e-3
    assert modal['constant_pressure_gradient_norm']>.1
    stable={key:relative(tight[key][0],base[key][0]) for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n')}
    stable.update({key:relative(tight[key],base[key]) for key in ('inlet_pressure_pa','physical_dissipation_w')})
    assert max(stable.values())<1e-7
    empty=records['empty-factor']
    s=sum(1/(n*n*m*m*((n*math.pi/2)**2+(m*math.pi/2)**2)) for n in range(1,256,2) for m in range(1,256,2))
    exact=4*.008/(64*4*s/(.1*math.pi**4))
    calibration={'pressure_error':relative(empty['inlet_pressure_pa'],exact),
        'dissipation_error':relative(empty['physical_dissipation_w'],exact*.008)}
    assert max(calibration.values())<.01
    prior=ROOT/'build/c3d-reference-method/runs/59f0e3b5b00e68f217e64161fd2c9e3723f0b29e19706febd6dc0f907026ea1b'
    old_empty=json.loads((prior/'empty4.json').read_text())
    assert relative(empty['inlet_pressure_pa'],old_empty['inlet_pressure_pa'])<1e-7
    assert relative(empty['physical_dissipation_w'],old_empty['physical_dissipation_w'])<1e-7
    refined=records['cube-factor-normal']
    changes={key:relative(refined[key][0],base[key][0]) for key in ('pressure_force_n','raw_symmetric_viscous_force_n','reaction_force_n')}
    mismatch={name:relative(row['pressure_force_n'][0]+row['raw_symmetric_viscous_force_n'][0],row['reaction_force_n'][0])
        for name,row in (('cube-factor',base),('cube-factor-normal',refined))}
    assert refined['tetrahedra']==6720
    assert refined['identity']['mesh_sha256']!=base['identity']['mesh_sha256']
    assert len(refined['axis_nodes_m'][0])==len(base['axis_nodes_m'][0])+2
    assert refined['axis_nodes_m'][1:]==base['axis_nodes_m'][1:]
    assert changes['pressure_force_n']>.01
    scalar_changes={key:relative(refined[key],base[key]) for key in ('physical_dissipation_w','inlet_pressure_pa')}
    physical_pass=max([*changes.values(),*scalar_changes.values(),*mismatch.values()])<=.01
    assert not physical_pass
    log=DATA/'focused-tests.log';text=log.read_text()
    assert text.count('Ran 4 tests')==2 and text.count('\nOK\n')==2
    assert 'Traceback' not in text and 'FAILED (' not in text
    # Every retained failed and successful new receipt must still identify its artifacts.
    all_receipts={}
    for p in (DATA/'runs').glob('*/*-receipt.json'):
        receipt=json.loads(p.read_text())
        for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
        for n,digest in receipt['source_sha256'].items():assert sha(p.parent/'source'/n)==digest
        all_receipts[str(p)]=sha(p)
    # Preserve the predecessor audit and every source/frozen field it had recorded.
    predecessor=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,digest in predecessor['reference_receipts_sha256'].items():
        assert sha(Path(p))==digest
        receipt=json.loads(Path(p).read_text())
        for artifact,h in receipt['artifact_sha256'].items():assert sha(Path(artifact))==h
    for p,digest in predecessor['protected_build_hashes'].items():assert sha(ROOT/p)==digest
    new_files=[p for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*')
        if p.is_file() and str(p.relative_to(ROOT)) not in baseline]
    costs={name:{'iterations':records[name]['iterations'],'wall_s':receipts[name]['wall_s'],
        'peak_observed_rss_bytes':receipts[name]['peak_observed_rss_bytes'],
        'true_residual':records[name]['final_residual']['true_residual']} for name in names}
    audit=dict(schema='physics_sim_c3d_preconditioner_audit_v1',
        status='goal_achieved_force_testing_resumed_physical_reference_unqualified',
        force_testing_ready=True,force_testing_resumed=True,physical_accuracy_certified=False,
        stage_1_complete=False,native_source_unchanged=True,
        source_bundle=bundle,identical_cube_operator=base['identity'],
        residual_block_ratio_momentum_over_continuity=dominance,baseline_residual=residual,
        pressure_modes=modal,mesh_conditioning=control['diagnostics'],costs=costs,
        residual_stability_relative_changes=stable,
        tighter_control={'requested_callback_target':tight['target'],
            'actual_true_residual':tight['final_residual']['true_residual'],
            'requested_callback_target_reached':tight['final_residual']['true_residual']<=tight['target']},
        empty_calibration=calibration,normal_refinement_relative_changes=changes,
        normal_refinement_scalar_relative_changes=scalar_changes,
        raw_surface_reaction_relative_mismatch=mismatch,physical_force_gate_passed=physical_pass,
        prior_original_cube_failure_receipt_sha256=sha(prior/'cube2-receipt.json'),
        reference_receipts_sha256=all_receipts,test_log_sha256=sha(log),
        new_source_sha256={str(p.relative_to(ROOT)):sha(p) for p in new_files},
        changed_preexisting_files=changed,committed=False,packaged=False,installed=False,
        next_gate='bounded pressure/corner spatial refinement and raw-force/reaction convergence under unchanged resource caps')
    (DATA/'completion-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({key:audit[key] for key in ('status','force_testing_ready','costs',
        'residual_stability_relative_changes','normal_refinement_relative_changes',
        'raw_surface_reaction_relative_mismatch','physical_accuracy_certified')}),flush=True)


if __name__=='__main__':main()
