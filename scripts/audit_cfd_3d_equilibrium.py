#!/usr/bin/env python3
"""Audit independently checked equilibrium identities on immutable cube fields."""
import hashlib
import json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-equilibrium'


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    baseline=json.loads((DATA/'baseline.json').read_text())
    changed=[p for p,d in baseline.items() if sha(ROOT/p)!=d]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk','scripts/cfd_reference3d_spatial_probe.py'},changed
    records={};costs={};hashes={}
    for path in (DATA/'runs').glob('*/*-receipt.json'):
        receipt=json.loads(path.read_text());directory=path.parent
        for artifact,digest in receipt['artifact_sha256'].items():assert sha(Path(artifact))==digest
        for name,digest in receipt['source_sha256'].items():assert sha(directory/'source'/name)==digest
        bundle=hashlib.sha256(json.dumps(receipt['source_sha256'],sort_keys=True).encode()).hexdigest()
        assert directory.name==bundle
        assert sha(DATA/'supervisor-source'/(receipt['runner_sha256']+'.py'))==receipt['runner_sha256']
        assert receipt['mesh_cap']==50000 and receipt['rss_cap_bytes']==1800*1024**2 and receipt['wall_cap_s']==180
        assert receipt['returncode']==0 and receipt['diagnostic_failure'] is None and receipt['stop_reason'] is None
        assert receipt['wall_s']<180 and receipt['peak_observed_rss_bytes']<1800*1024**2
        name=path.name.removesuffix('-receipt.json');row=json.loads((directory/(name+'.json')).read_text())
        assert row['diagnostic_accepted'] and not row['physical_accuracy_certified']
        assert row['tetrahedra']<=50000 and row['peak_rss_bytes']<1800*1024**2
        input_path=Path(row['input_receipt']);assert sha(input_path)==row['input_receipt_sha256']
        input_receipt,original=verify_receipt(input_path)
        assert sha(Path(row['input_snapshot']))==row['input_snapshot_sha256']
        for lift,old in zip(row['lifts'],original['consistency_diagnostics']['volume_lifts']):
            total_weak=lift['pressure']['weak_load_n'][0]+lift['viscous']['weak_load_n'][0]
            assert abs(total_weak-old['symmetric_stress_load_n'])<1e-9
            for part,key in (('pressure','pressure_force_n'),('viscous','raw_symmetric_viscous_force_n')):
                assert np.max(np.abs(lift[part]['identity_error_n']))<1e-9
                assert np.max(np.abs(np.array(lift[part]['raw_surface_load_n'])-original[key]))<1e-10
        if 'equilibrium_indicator_squared_per_tet' in row:
            scores=np.array(row['equilibrium_indicator_squared_per_tet'])
            assert len(scores)==row['tetrahedra'] and np.all(scores>=0) and np.all(np.isfinite(scores))
            assert abs(scores.sum()-sum(row['h_squared_volume_defect_centroid_buckets'])-sum(row['h_weighted_jump_defect_centroid_buckets']))<1e-12
        assert name not in records
        records[name]=row;hashes[str(path)]=sha(path)
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes')}
    assert {'body6-base','body6-normal','body6-edge','body6-base-indicators','body6-normal-indicators','body6-edge-indicators'}<=records.keys()
    comparisons={}
    for suffix in ('base','normal','edge'):
        older=records['body6-'+suffix];row=records['body6-'+suffix+'-indicators']
        assert row['input_snapshot_sha256']==older['input_snapshot_sha256']
        assert abs(row['volume_strong_equilibrium_defect_l2']-older['volume_strong_equilibrium_defect_l2'])<1e-12
        assert abs(row['interior_stress_jump_l2']-older['interior_stress_jump_l2'])<1e-12
        lift=row['lifts'][0];gap=sum(lift[p]['raw_minus_weak_n'][0] for p in ('pressure','viscous'))
        comparisons[suffix]={'raw_minus_weak_force_n':gap,
            'volume_equilibrium_defect_l2':row['volume_strong_equilibrium_defect_l2'],
            'interior_stress_jump_l2':row['interior_stress_jump_l2'],
            'mesh_scaled_indicator_squared':sum(row['equilibrium_indicator_squared_per_tet']),
            'maximum_identity_error_n':max(abs(v) for lift in row['lifts'] for part in ('pressure','viscous') for v in lift[part]['identity_error_n'])}
    assert comparisons['edge']['volume_equilibrium_defect_l2']<comparisons['base']['volume_equilibrium_defect_l2']
    assert comparisons['edge']['interior_stress_jump_l2']<comparisons['base']['interior_stress_jump_l2']
    assert abs(comparisons['edge']['raw_minus_weak_force_n'])>abs(comparisons['base']['raw_minus_weak_force_n'])
    predecessor=json.loads((ROOT/'build/c3d-spatial/checkpoint-audit.json').read_text())
    for p,d in predecessor['reference_receipts_sha256'].items():
        assert sha(Path(p))==d;verify_receipt(Path(p))
    adaptive_path=next((DATA/'adaptive-runs').glob('*/body6-score32-receipt.json'))
    adaptive_receipt,adaptive=verify_receipt(adaptive_path)
    assert adaptive_receipt['returncode']==0 and adaptive['numerically_accepted']
    diagnostic_path=Path(adaptive_receipt['command'][adaptive_receipt['command'].index('--diagnostic')+1])
    assert sha(diagnostic_path)==adaptive['adaptive_refinement']['diagnostic_sha256']
    assert adaptive['tetrahedra']==27264
    assert adaptive['adaptive_refinement']['marked_macros_per_octant']==32
    assert adaptive['adaptive_refinement']['captured_indicator_fraction']>.5
    adaptive_diagnostic=records['body6-score32-indicators']
    comparisons['score32']={
        'volume_equilibrium_defect_l2':adaptive_diagnostic['volume_strong_equilibrium_defect_l2'],
        'interior_stress_jump_l2':adaptive_diagnostic['interior_stress_jump_l2'],
        'mesh_scaled_indicator_squared':sum(adaptive_diagnostic['equilibrium_indicator_squared_per_tet']),
        'raw_minus_weak_force_n':sum(adaptive_diagnostic['lifts'][0][p]['raw_minus_weak_n'][0] for p in ('pressure','viscous'))}
    original=verify_receipt(Path(records['body6-base-indicators']['input_receipt']))[1]
    from audit_cfd_3d_spatial import force_comparison
    adaptive_comparison=force_comparison(adaptive,original)
    default_path=next((ROOT/'build/c3d-spatial/runs').glob('*/mesh-provider-default-control-receipt.json'))
    default_receipt,default=verify_receipt(default_path)
    earlier_path=next((ROOT/'build/c3d-spatial/runs').glob('*/exact-cube-mass-pc-receipt.json'))
    earlier=verify_receipt(earlier_path)[1]
    assert default['identity']==earlier['identity']
    default_comparison=force_comparison(default,earlier)
    assert max([*default_comparison['component_relative_changes'].values(),*default_comparison['scalar_relative_changes'].values()])<1e-7
    text=(DATA/'focused-tests.log').read_text()
    assert 'Ran 4 tests' in text and '\nOK\n' in text and 'Traceback' not in text and 'FAILED (' not in text
    assert 'Ran 2 tests' in text and text.count('\nOK\n')==2
    reference_method=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,d in reference_method['protected_build_hashes'].items():assert sha(ROOT/p)==d
    sources={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*')
        if p.is_file() and str(p.relative_to(ROOT)) not in baseline}
    audit=dict(schema='physics_sim_c3d_equilibrium_audit_v1',status='equilibrium_attribution_verified_force_reference_unqualified',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,native_source_unchanged=True,
        saved_fields_unchanged=True,comparisons=comparisons,costs=costs,observer_receipts_sha256=hashes,
        adaptive_force_comparison=adaptive_comparison,adaptive_receipt_sha256=sha(adaptive_path),
        default_mesh_equivalence=default_comparison,default_control_receipt_sha256=sha(default_path),
        new_source_sha256=sources,changed_preexisting_files=changed,baseline_sha256=sha(DATA/'baseline.json'),
        predecessor_audit_sha256=sha(ROOT/'build/c3d-spatial/checkpoint-audit.json'),
        test_log_sha256=sha(DATA/'focused-tests.log'),committed=False,packaged=False,installed=False,
        next_gate='test a higher-order pressure/velocity reference pair under unchanged PDE, boundary, raw-force and resource gates; retain failed spatial controls')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','comparisons','saved_fields_unchanged')}),flush=True)


if __name__=='__main__':main()
