#!/usr/bin/env python3
"""Audit signed force-gap targeting, geometry rejection and the finer-body cap."""
import json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
from audit_cfd_3d_selective import observer as selective_observer
from audit_cfd_3d_domain import observe as domain_observe
from audit_cfd_3d_graded import sha
from cfd_reference3d_corner_attribution import run as attribution
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT/'build/c3d-corner'


def main():
    baseline=json.loads((DATA/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(ROOT/p)!=h]
    assert set(changed)<={'docs/current_truth.md','docs/README.md','make/rules-tools.mk'},changed
    for family,observe in (('selective',selective_observer),('domain',domain_observe)):
        prior=json.loads((ROOT/f'build/c3d-{family}/checkpoint-audit.json').read_text())
        for p,h in prior['reference_receipts_sha256'].items():assert sha(Path(p))==h;verify_receipt(Path(p))
        for p,h in prior['observer_receipts_sha256'].items():assert sha(Path(p))==h;observe(Path(p))
    prior=json.loads((ROOT/'build/c3d-reference-method/completion-audit.json').read_text())
    for p,h in prior['protected_build_hashes'].items():assert sha(ROOT/p)==h
    report=json.loads((DATA/'force-attribution.json').read_text());assert report==attribution()
    assert not report['force_replaced'] and not report['physical_accuracy_certified']
    target=report['records']['L8-held-normal-stress']['lifts'][0]
    assert abs(target['net_raw_minus_reaction_n'][0]+.00084894505415665)<1e-9
    assert abs(target['total_signed_centroid_buckets_n'][0][0]+.000987618157688344)<1e-12
    fixed=json.loads((DATA/'mesh-survey-detail.json').read_text());assert len(fixed)==6 and all(not r['geometry_admitted'] for r in fixed)
    assert {(r['length_m'],r['edge_interval_m']) for r in fixed}=={(L,e) for L in (4.,8.) for e in (.1,.08,.0625)}
    for row in fixed:
        assert row['controlled_tetrahedra']==row['original_tetrahedra']==13824
        assert row['controlled_corner_max_condition']>row['original_corner_max_condition']
        assert row['controlled_corner_mean_condition']>row['original_corner_mean_condition']
    local=json.loads((DATA/'local-mesh-shape-survey.json').read_text());star=json.loads((DATA/'edge-star-survey.json').read_text())
    assert len(local)==len(star)==63 and all(not r['geometry_admitted'] for r in local+star)
    assert all(r['refined_affected_worst_shape']>r['original_affected_worst_shape'] for r in local+star)
    assert {tuple(r['selected_octant_macro_indices']) for r in local}=={tuple(r['selected_octant_macro_indices']) for r in star}
    better=json.loads((DATA/'body6-preflight.json').read_text());assert len(better)==3
    assert [r['tetrahedra'] for r in better]==[13824,18816,23616]
    assert better[1]['nearest_edge_bucket_worst_shape']<better[0]['nearest_edge_bucket_worst_shape']
    assert better[1]['nearest_edge_bucket_condition_max']<better[0]['nearest_edge_bucket_condition_max']
    for row in better:assert row['coo_scratch_bytes']==row['macro_count']*103**2*16
    hashes={};costs={}
    for path in sorted((DATA/'runs').glob('*/*-receipt.json')):
        receipt,row=verify_receipt(path);name=path.name.removesuffix('-receipt.json');hashes[str(path)]=sha(path)
        assert name=='L4-body6-base' and row is None
        assert receipt['returncode']!=0 and receipt['diagnostic_failure']['kind']=='resource_cap'
        assert receipt['stop_reason']=='reference RSS/time cap' and receipt['wall_s']<20
        assert receipt['peak_observed_rss_bytes']>1800*1024**2
        library=Path(receipt['command'][receipt['command'].index('--factor-library')+1]);build=library.with_name('factor-build.json')
        assert sha(library)==receipt['factor_library_sha256']==receipt['factor_build']['library_sha256']
        assert sha(build)==receipt['factor_build_record_sha256'] and json.loads(build.read_text())==receipt['factor_build']
        assert receipt['factor_build']['source_sha256']==receipt['source_sha256']['cfd_reference3d_accelerate.c']
        phases=[r for r in receipt['progress'] if 'phase' in r]
        assert any(r['phase']=='assembly_complete' for r in phases)
        assert not any(r['phase'] in ('solve','factor_ready','solve_complete') for r in phases)
        assembled=next(r for r in phases if r['phase']=='assembled')
        assert assembled['maximum_local_elimination_residual']<1e-10 and assembled['maximum_local_schur_asymmetry']<1e-10
        costs[name]={k:receipt[k] for k in ('wall_s','peak_observed_rss_bytes','returncode','diagnostic_failure','stop_reason')}
        costs[name].update(assembly_s=assembled['assembly_s'],assembly_owned_peak_rss_bytes=assembled['peak_rss_bytes'],geometry_classes=assembled['geometry_classes'],operator_identity=assembled['identity'])
    assert len(hashes)==1
    log=(DATA/'geometry-tests.log').read_text();assert 'Ran 5 tests' in log and '\nOK\n' in log and 'FAILED (' not in log
    for name in ('cfd_3d_corner_goal.md','cfd_3d_corner_checkpoint.md','cfd_3d_bounded_assembly_goal.md'):assert (ROOT/'docs'/name).is_file()
    audit=dict(schema='physics_sim_c3d_corner_audit_v1',status='signed_gap_targeted_geometry_families_rejected_better_body_mesh_resource_stopped',
        persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,physical_mesh_adopted=False,native_source_unchanged=True,
        predecessor_fields_and_workers_unchanged=True,resource_limits_unchanged=True,reference_receipts_sha256=hashes,costs=costs,
        signed_load_attribution=report['records'],fixed_redistributions_rejected=6,paired_bisections_rejected=63,paired_multiedge_rejections=63,
        body6_geometry_preflight=better,body6_force_result_available=False,resource_stop_phase='factor setup after successful assembly',test_count=5,
        test_log_sha256=sha(DATA/'geometry-tests.log'),evidence_sha256={p.name:sha(p) for p in DATA.glob('*.json') if p.name not in ('checkpoint-audit.json','baseline.json')},
        baseline_sha256=sha(DATA/'baseline.json'),predecessor_audit_sha256=sha(ROOT/'build/c3d-selective/checkpoint-audit.json'),changed_preexisting_files=changed,
        current_source_sha256={str(p.relative_to(ROOT)):sha(p) for folder in ('scripts','tests','docs') for p in (ROOT/folder).glob('*') if p.is_file() and str(p.relative_to(ROOT)) not in baseline},
        committed=False,packaged=False,installed=False,next_gate='bounded reduced sparse assembly with matched original-equation proof before retrying the exact finer-body cube')
    (DATA/'checkpoint-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    print(json.dumps({k:audit[k] for k in ('status','costs','body6_geometry_preflight')},indent=2),flush=True)


if __name__=='__main__':main()
