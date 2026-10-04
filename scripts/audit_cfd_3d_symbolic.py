#!/usr/bin/env python3
"""Audit exact graph/cost diagnostics without claiming numerical/physical fields."""
import json,hashlib
from pathlib import Path
import numpy as np
from audit_cfd_3d_graded import sha
from audit_cfd_3d_spatial import verify_receipt
from audit_cfd_3d_shared_factor import observer as shared_observer
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-symbolic'


def diagnostic(path):
    receipt=json.loads(path.read_text())
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/n)==h
    assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
    assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180 and receipt['linear_iteration_cap']==3000
    name=path.name.removesuffix('-receipt.json');snapshot=path.with_name(name+'.npz');output=path.with_name(name+'.json')
    assert receipt['command'][-4:]==['--snapshot',str(snapshot),'--output',str(output)] and not snapshot.exists()
    library=Path(receipt['command'][receipt['command'].index('--symbolic-library')+1]);build=library.with_name('symbolic-build.json')
    assert library.parent==path.parent/'source'
    assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
    assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
    assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_symbolic.c']
    row=json.loads(output.read_text()) if output.exists() else None
    if receipt['returncode']==0:
        assert receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
        assert row['diagnostic_accepted'] and not row['numerically_accepted'] and not row['physical_accuracy_certified'] and not row['numerical_field_published']
        assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2 and row['peak_rss_bytes']<1800*1024**2
        assert row['wall_s']<180 and row['tetrahedra']<=50000
        assert row['original_mixed_input_preserved'] and row['symbolic']['input_preserved'] and row['symbolic']['status']==0
        assert row['velocity_prefix_dofs']==row['symbolic']['represented_scalar_dofs']
        if row['graph_mode']=='interleaved_scalar':assert row['scalar_permutation_action_check']['bijective'] and row['scalar_permutation_action_check']['relative_error']<1e-12
        for n,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/n)==h
    else:
        assert receipt['diagnostic_failure'] is not None and row is None
        assert 'not JSON serializable' in path.with_name(name+'.log').read_text()
    return receipt,row


def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-factor-peak/checkpoint-audit.json';old=json.loads(predecessor.read_text())
    for p,h in old['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    for p,h in old['preserved_observer_receipts_sha256'].items():assert sha(Path(p))==h;shared_observer(Path(p))
    prior=json.loads((ROOT/'build/c3d-shared-factor/checkpoint-audit.json').read_text())
    for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
    protected=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in protected['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-final-receipt.json').read_text());assert tests['test_count']==7
    assert sha(DATA/'support-tests-final.log')==tests['log_sha256']
    for n,h in tests['source_sha256'].items():assert sha(ROOT/n)==h
    assert sha(DATA/'support/symbolic.dylib')==tests['library_sha256']
    assert sha(DATA/'support/symbolic-build.json')==tests['build_record_sha256']
    assert sha(DATA/'support/api-receipt.json')==tests['api_receipt_sha256']
    assert sha(DATA/'support/Solve.h')==tests['sdk_header_sha256']==json.loads((DATA/'support/api-receipt.json').read_text())['header_sha256']
    log=(DATA/'support-tests-final.log').read_text();assert 'Ran 7 tests' in log and chr(10)+'OK'+chr(10) in log
    rows={};costs={};hashes={};failed={}
    for path in (DATA/'runs').glob('*/*-receipt.json'):
        receipt,row=diagnostic(path);name=path.name.removesuffix('-receipt.json');assert name not in rows and name not in failed
        hashes[str(path)]=sha(path)
        if row is None:failed[name]=receipt['diagnostic_failure'];continue
        rows[name]=row;costs[name]=dict(symbolic=row['symbolic'],owned_peak_rss_bytes=row['peak_rss_bytes'],peak_observed_rss_bytes=receipt['peak_observed_rss_bytes'],wall_s=receipt['wall_s'],timings=row['timings'])
    required={'L4-body6-base-component-fixed','L4-body6-normal-component','L4-body6-base-interleaved','L4-body6-normal-interleaved','L4-body6-base-vector','L4-body6-normal-vector'}
    assert set(rows)==required and set(failed)=={'L4-body6-base-component'}
    comparison={}
    for suffix,oldname in (('base','L4-body6-base-shared'),('normal','L4-body6-normal-shared')):
        oldpath=next((ROOT/'build/c3d-shared-factor/runs').glob('*/'+oldname+'-receipt.json'));receipt,original=verify_receipt(oldpath)
        identity=original['identity'] if original else next(r['identity'] for r in receipt['progress'] if r.get('phase')=='assembled')
        control=rows['L4-body6-'+suffix+'-component'+('-fixed' if suffix=='base' else '')]
        assert control['tetrahedra']==(18816 if suffix=='base' else 23616)
        for mode in ('component'+('-fixed' if suffix=='base' else ''),'interleaved','vector'):
            row=rows['L4-body6-'+suffix+'-'+mode]
            for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==identity[key]
            assert row['original_mixed_input_sha256']==control['original_mixed_input_sha256']
        if suffix=='base':assert control['symbolic']['factor_storage_bytes']==original['preconditioner']['symbolic_factor_storage_bytes']
        comparison[suffix]={mode:dict(factor_storage_reduction=1-rows['L4-body6-'+suffix+'-'+mode]['symbolic']['factor_storage_bytes']/control['symbolic']['factor_storage_bytes'],
            factor_plus_workspace_reduction=1-rows['L4-body6-'+suffix+'-'+mode]['symbolic']['factor_plus_workspace_bytes']/control['symbolic']['factor_plus_workspace_bytes'],
            owned_analysis_memory_change=rows['L4-body6-'+suffix+'-'+mode]['peak_rss_bytes']/control['peak_rss_bytes']-1) for mode in ('interleaved','vector')}
    assert comparison['normal']['interleaved']['factor_storage_reduction']<.03 and comparison['normal']['interleaved']['owned_analysis_memory_change']>0
    assert comparison['normal']['vector']['factor_plus_workspace_reduction']<0
    audit=dict(schema='physics_sim_c3d_symbolic_audit_v1',status='exact_symbolic_cost_established_ordering_controls_rejected',persistent_goal_complete=False,stage_1_complete=False,
        physical_accuracy_certified=False,physical_mesh_adopted=False,ordering_candidate_adopted=False,numerical_candidate_attempted=False,
        native_source_unchanged=True,predecessor_fields_and_workers_unchanged=True,reference_receipts_sha256=hashes,costs=costs,comparisons=comparison,retained_failed_prototype=failed,
        symbolic_diagnostic_count=6,numerically_accepted_field_count=0,test_count=7,support_test_receipt_sha256=sha(DATA/'support-test-final-receipt.json'),
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='bounded exact principal-component Cholesky factors and fixed SPD repeated coupled sweeps with full FE acceptance')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+chr(10))
    print(json.dumps({k:audit[k] for k in ('status','comparisons','symbolic_diagnostic_count','numerically_accepted_field_count')},indent=2))


if __name__=='__main__':main()
