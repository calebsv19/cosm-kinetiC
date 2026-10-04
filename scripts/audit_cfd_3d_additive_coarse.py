#!/usr/bin/env python3
"""Audit exact coupled two-block controls and full larger-run cost rejection."""
import json
from pathlib import Path
from audit_cfd_3d_graded import sha
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from audit_cfd_3d_shared_factor import fields
from audit_cfd_3d_symbolic import diagnostic
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-additive-coarse'

def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    predecessor=ROOT/'build/c3d-cubic-coarse/checkpoint-audit.json'
    for p,h in json.loads(predecessor.read_text())['reference_receipts_sha256'].items():
        assert sha(Path(p))==h;verify_receipt(Path(p))
    protected=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in protected['protected_build_hashes'].items():assert sha(ROOT/p)==h
    tests=json.loads((DATA/'support-test-receipt.json').read_text())
    assert tests['test_count']==5 and sha(DATA/'support-tests-final.log')==tests['log_sha256']
    for p,h in tests['source_sha256'].items():assert sha(ROOT/p)==h
    assert sha(ROOT/'build/c3d-cholesky/support/factor.dylib')==tests['library_sha256']
    rows={};receipts={};costs={};hashes={};equivalence={};differences={}
    oldname='original-adopted-defaults';oldpath=next((ROOT/'build/c3d-cholesky/runs').glob('*/'+oldname+'-receipt.json'))
    oldreceipt,old=verify_receipt(oldpath)
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        name=path.name.removesuffix('-receipt.json');receipt,row=verify_receipt(path)
        rows[name]=row;receipts[name]=receipt;hashes[str(path)]=sha(path)
        for p,h in receipt['source_sha256'].items():assert sha(ROOT/'scripts'/p)==h
        assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','diagnostic_failure')}
        if receipt['returncode']!=0:continue
        assert row['linear_solve_accepted'] and row['numerically_accepted'] and not row['physical_accuracy_certified']
        pc=row['preconditioner'];assert pc['kind']=='additive_coarse_velocity' and pc['interpolation']['coarse_degree']==3
        assert pc['interpolation']['sparse_left_inverse_verified'] and pc['interpolation']['maximum_projected_left_inverse_error']<1e-11
        assert pc['interpolation']['essential_projection_injective'] and pc['interpolation']['essential_boundary_vanishing_verified']
        assert pc['input_preserved_after_factor'] and pc['shared_input_preserved_after_solve'] and pc['scaling']=='none'
        assert pc['factor_storage_bytes']==pc['coarse_factor']['symbolic_factor_storage_bytes']+pc['local_sweep']['factor_storage_bytes']
        costs[name].update(owned_peak_rss_bytes=row['peak_rss_bytes'],iterations=row['iterations'],tetrahedra=row['tetrahedra'],factor_storage_bytes=pc['factor_storage_bytes'],final_residual=row['final_residual'],phase_timings=row['timings'])
        if name.startswith('original-'):
            for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert row['identity'][key]==old['identity'][key]
            comp=force_comparison(row,old)
            assert max([*comp['component_relative_changes'].values(),*comp['scalar_relative_changes'].values()])<1e-7
            equivalence[name]=comp;differences[name]=fields(path.with_name(name+'.npz'),oldpath.with_name(oldname+'.npz'))
    assert {'original-L4-additive-two-block','L4-body6-base-additive-two-block'}<=set(rows)
    assert set(rows)<={'original-L4-additive-two-block','L4-body6-base-additive-two-block','L4-body6-normal-additive-two-block'}
    assert 'original-L4-additive-two-block' in equivalence
    base_name='L4-body6-base-additive-two-block';base=receipts[base_name]
    previous=next((ROOT/'build/c3d-shared-factor/runs').glob('*/L4-body6-base-shared-receipt.json'));previous_receipt,previous_row=verify_receipt(previous)
    identity=rows[base_name]['identity'] if rows[base_name] else next(p['identity'] for p in base['progress'] if p.get('phase')=='assembled')
    for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert identity[key]==previous_row['identity'][key]
    if base['returncode']==0:
        path=next((DATA/'runs').glob('*/'+base_name+'-receipt.json'))
        comparison=force_comparison(rows[base_name],previous_row)
        assert max([*comparison['component_relative_changes'].values(),*comparison['scalar_relative_changes'].values()])<1e-7
        equivalence[base_name]=comparison;differences[base_name]=fields(path.with_name(base_name+'.npz'),previous.with_name('L4-body6-base-shared.npz'))
    normal_name='L4-body6-normal-additive-two-block';normal_admitted=normal_name in receipts and receipts[normal_name]['returncode']==0
    refinement={}
    if normal_name in receipts:
        previous=next((ROOT/'build/c3d-shared-factor/runs').glob('*/L4-body6-normal-shared-receipt.json'));failed,_=verify_receipt(previous)
        expected=next(p['identity'] for p in failed['progress'] if p.get('phase')=='assembled')
        actual=rows[normal_name]['identity'] if rows[normal_name] else next(p['identity'] for p in receipts[normal_name]['progress'] if p.get('phase')=='assembled')
        for key in ('mesh_sha256','free_dofs_sha256','rhs_sha256'):assert actual[key]==expected[key]
        if normal_admitted:
            assert base['returncode']==0 and rows[normal_name]['tetrahedra']==23616
            assert rows[normal_name]['axis_nodes_m'][1:]==rows[base_name]['axis_nodes_m'][1:]
            refinement=force_comparison(rows[normal_name],rows[base_name])
    for p,h in json.loads(predecessor.read_text()).get('symbolic_receipts_sha256',{}).items():
        assert sha(Path(p))==h
        receipt=json.loads(Path(p).read_text())
        for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
    symbolic_hashes={};symbolic_cost={}
    for path in (ROOT/'build/c3d-cubic-coarse/symbolic-runs').glob('*/L4-body6-normal-cubic-cost-receipt.json'):
        receipt=json.loads(path.read_text());assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
        for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
        for name,h in receipt['source_sha256'].items():assert sha(path.parent/'source'/name)==h and sha(ROOT/'scripts'/name)==h
        import hashlib
        assert path.parent.name==hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
        assert sha(ROOT/'build/c3d-cubic-coarse/supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        diagnostic=json.loads(path.with_name('L4-body6-normal-cubic-cost.json').read_text())
        assert diagnostic['diagnostic_accepted'] and not diagnostic['numerically_accepted'] and not diagnostic['numerical_field_published']
        assert not path.with_name('L4-body6-normal-cubic-cost.npz').exists() and diagnostic['original_mixed_input_preserved']
        assert diagnostic['peak_rss_bytes']<1800*1024**2 and receipt['wall_s']<180
        symbolic_hashes[str(path)]=sha(path);symbolic_cost=diagnostic['symbolic']
        if normal_admitted:assert symbolic_cost['total_factor_storage_bytes']==rows[normal_name]['preconditioner']['factor_storage_bytes']
    audit=dict(schema='physics_sim_c3d_additive_coarse_audit_v1',status='additive_cubic_controls_measured_full_gates_preserved',persistent_goal_complete=False,stage_1_complete=False,
        physical_accuracy_certified=False,larger_path_adopted=normal_admitted,finer_normal_admitted=normal_admitted,force_convergence_testing_resumed=normal_admitted,force_refinement=refinement,native_source_unchanged=True,reference_receipts_sha256=hashes,symbolic_receipts_sha256=symbolic_hashes,symbolic_cost=symbolic_cost,costs=costs,equivalence=equivalence,field_differences=differences,
        support_test_receipt_sha256=sha(DATA/'support-test-receipt.json'),test_count=5,baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(predecessor),changed_preexisting_files=changed,
        next_gate='physical component/domain/raw-reaction/stress force qualification' if normal_admitted else 'invertible cubic/quartic coordinate block preconditioner with exact coupled fine factor')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps(audit,indent=2))

if __name__=='__main__':main()
