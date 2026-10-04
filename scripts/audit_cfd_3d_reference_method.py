#!/usr/bin/env python3
"""Audit the stronger-reference support gate, including its retained cube failure."""
import hashlib
import json
import math
import sys
from pathlib import Path
import numpy as np
import skfem, scipy, pyamg
from cfd_fem_reference3d_solenoidal import mesh_for_case
from run_cfd_reference3d_method import SOURCES
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-reference-method'


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def checked_receipt(p):
    row=json.loads(p.read_text())
    for artifact,digest in row['artifact_sha256'].items():assert sha(Path(artifact))==digest
    for name,digest in row['source_sha256'].items():assert sha(p.parent/'source'/name)==digest
    assert row['mesh_cap']==50000 and row['rss_cap_bytes']==1800*1024**2 and row['wall_cap_s']==180
    assert row['peak_observed_rss_bytes']<row['rss_cap_bytes'] and row['wall_s']<row['wall_cap_s']
    return row


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    allowed={'docs/README.md','docs/current_truth.md','make/rules-tools.mk'}
    changed=[name for name,digest in baseline.items() if sha(ROOT/name)!=digest]
    assert set(changed)<=allowed,changed
    assert not any(name.startswith(('src/','include/')) for name in changed)
    hashes={name:sha(ROOT/'scripts'/name) for name in SOURCES}
    bundle=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    directory=DATA/'runs'/bundle
    receipts={case:checked_receipt(directory/(case+'-receipt.json')) for case in ('empty2','empty4','cube2')}
    rows={case:json.loads((directory/(case+'.json')).read_text()) for case in ('empty2','empty4')}
    fourier_sum=sum(1/(n*n*m*m*((n*math.pi/2)**2+(m*math.pi/2)**2)) for n in range(1,256,2) for m in range(1,256,2))
    expected_pressure=4*.008/(64*4*fourier_sum/(.1*math.pi**4))
    calibration={}
    for case,row in rows.items():
        assert receipts[case]['returncode']==0 and receipts[case]['stop_reason'] is None
        assert row['tetrahedra']<=50000 and row['iterations']<=3000
        assert row['free_relative_residual']<1e-8
        assert row['volume_divergence_max_s_inv']<1e-8 and row['flux_error']<1e-8
        p_error=abs(row['inlet_pressure_pa']/expected_pressure-1)
        d_error=abs(row['physical_dissipation_w']/(expected_pressure*.008)-1)
        calibration[case]={'pressure_relative_error':p_error,'dissipation_relative_error':d_error,
            'volume_divergence_max_s_inv':row['volume_divergence_max_s_inv'],
            'tetrahedra':row['tetrahedra'],'iterations':row['iterations'],
            'wall_s':receipts[case]['wall_s'],'peak_observed_rss_bytes':receipts[case]['peak_observed_rss_bytes']}
    assert calibration['empty4']['pressure_relative_error']<.01
    assert calibration['empty4']['dissipation_relative_error']<.01
    assert calibration['empty4']['pressure_relative_error']<calibration['empty2']['pressure_relative_error']
    assert calibration['empty4']['dissipation_relative_error']<calibration['empty2']['dissipation_relative_error']
    failure=receipts['cube2'];assert failure['returncode']!=0 and failure['stop_reason'] is None
    assert not (directory/'cube2.json').exists() and not (directory/'cube2.npz').exists()
    log=(directory/'cube2.log').read_text();progress=[]
    for line in log.splitlines():
        try:
            row=json.loads(line)
            if isinstance(row,dict):progress.append(row)
        except json.JSONDecodeError:pass
    preassembly=next(row for row in progress if row.get('phase')=='preassembly')
    setup=next(row for row in progress if row.get('phase')=='solve')
    last=next(row for row in reversed(progress) if 'iteration' in row)
    assert last['iteration']==3000 and last['true_residual']>=1e-8
    assert "('linear_residual', 3000," in log
    assert preassembly['tetrahedra']==4992
    tests=DATA/'method-tests.log';text=tests.read_text()
    assert 'Ran 3 tests' in text and 'Ran 1 test' in text and 'Ran 4 tests' in text and text.count('\nOK\n')==3
    assert 'FAILED (' not in text and 'Traceback' not in text
    known=next(json.loads(line) for line in text.splitlines() if line.startswith('{"schema": "c3d_cubic_solved_controls_v1"'))
    assert len(known['cases'])==24
    maxima={key:max(row[key] for row in known['cases']) for key in ('velocity_error','pressure_error',
        'pressure_traction_error','viscous_traction_error','true_residual','wall_divergence_max','volume_divergence_max')}
    assert all(value<1e-8 for value in maxima.values())
    # Preserve predecessor reference results, native test logs and both worker identities.
    predecessor_path=ROOT/'build/c3d-initial-improvements/completion-audit.json'
    predecessor=json.loads(predecessor_path.read_text())
    for filename,digest in predecessor['test_logs_sha256'].items():
        assert sha(ROOT/'build/c3d-initial-improvements/logs'/filename)==digest
    for receipt in predecessor['reference_receipts']:checked_receipt(Path(receipt))
    assert sha(ROOT/'build/c3d-initial-improvements/native-build/physics_sim_session_worker')==predecessor['worker_sha256']
    initial=json.loads((ROOT/'build/c3d-initial-improvements/baseline.json').read_text())['hashes']
    protected={p:d for p,d in initial.items() if p.startswith('build/')}
    for path,digest in protected.items():assert sha(ROOT/path)==digest
    mesh,*_=mesh_for_case();mapping=mesh.mapping().A.transpose(2,0,1);condition=np.linalg.cond(mapping)
    new_files=[p for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*')
        if p.is_file() and str(p.relative_to(ROOT)) not in baseline]
    audit=dict(schema='physics_sim_c3d_reference_method_audit_v1',support_gate_tests_passed=True,
        empty_calibration_passed=True,stage_1_complete=False,physical_accuracy_certified=False,
        status='reference_support_passed_cube_iteration_cap',source_bundle=bundle,
        known_answer_cases=24,known_answer_maxima=maxima,empty_calibration=calibration,
        cube_failure={'kind':'linear_iteration_cap',**preassembly,**setup,**last,
            'required_final_residual_below':1e-8,'callback_target':1e-10,
            'wall_s':failure['wall_s'],'peak_observed_rss_bytes':failure['peak_observed_rss_bytes'],
            'accepted_force_reference_written':False,
            'Jacobian_condition_min':float(condition.min()),'Jacobian_condition_median':float(np.median(condition)),
            'Jacobian_condition_max':float(condition.max())},
        changed_preexisting_files=changed,native_source_unchanged=True,native_worker_preserved=True,
        predecessor_audit_sha256=sha(predecessor_path),protected_build_hashes=protected,
        reference_receipts_sha256={str(p):sha(p) for p in (DATA/'runs').glob('*/*-receipt.json')},
        new_source_sha256={str(p.relative_to(ROOT)):sha(p) for p in new_files},
        test_log_sha256=sha(tests),versions={'python':sys.version,'numpy':np.__version__,
            'skfem':skfem.__version__,'scipy':scipy.__version__,'pyamg':pyamg.__version__},
        next_gate='bounded linear-preconditioner and graded-mesh conditioning diagnostics before any reference force acceptance',
        committed=False,packaged=False,installed=False,canonical_changed=False)
    (DATA/'completion-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({'status':audit['status'],'known_answer_cases':24,'empty_calibration':calibration['empty4'],
        'cube_failure':audit['cube_failure'],'native_source_unchanged':True,'physical_accuracy_certified':False}),flush=True)


if __name__=='__main__':main()
