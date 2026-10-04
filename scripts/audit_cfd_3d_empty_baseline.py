#!/usr/bin/env python3
"""Once-only calibrated empty-duct and unchanged cube-gate checkpoint audit."""
import hashlib,json
from pathlib import Path
import numpy as np
from skfem import MeshTet,Basis,ElementDG
import audit_cfd_3d_restart as parent
from audit_cfd_3d_spatial import verify_receipt,force_comparison
from cfd_reference3d_quartic_pair import CubicPressureMass,quartic_quadrature
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_empty_duct_reference import baseline_diagnostics
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-empty-baseline';parent.D=D

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def relative(a,b):return abs(a/b-1)
def comparisons():
    fields={};measurements={};inputs={}
    for length in (4,8):
        for count in (4,6):
            name=f'L{length}-empty-n{count}';p=next((D/'runs').glob('*/'+name+'-receipt.json'))
            r,row=parent.accepted(p,6)
            assert row['retained_target']==1e-11 and row['preconditioner']['flexible_iteration']['final_true_metric']<=1e-11
            assert row['velocity_degree']==4 and row['pressure_degree']==3 and row['verified_volume_product_degree']==6
            assert not row['body'] and row['length']==length and row['count']==count and row['mu']==.1 and row['flow_m3_s']==.008
            for k in ('body_force_status','body_traction_status','body_consistency_lift_status'):assert row[k]=='not_applicable'
            assert 'pressure_force_n' not in row and 'reaction_force_n' not in row
            with np.load(p.with_name(name+'.npz'),allow_pickle=False) as s:
                mesh=MeshTet(s['vertices_m'],s['tetrahedra'],sort_t=False)
                pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=quartic_quadrature(),elements=np.array([0]))
                np.testing.assert_array_equal(pb.doflocs,s['pressure_doflocs_m'])
                b=baseline_diagnostics(length,.1,.008,row['inlet_pressure_pa'],row['physical_dissipation_w'],s['pressure_coefficients'],pb,CubicPressureMass(pb,.1))
                assert b==row['analytical_baseline'] and b['baseline_reference_accuracy_accepted']
            inputs[str(p)]=sha(p);fields[length,count]=row
            measurements[name]=dict(receipt=str(p),whole_wall_s=r['wall_s'],owned_peak_mib=row['peak_rss_bytes']/2**20,sampled_peak_mib=r['peak_observed_rss_bytes']/2**20,iterations=row['iterations'],full_residual=row['final_residual'],tetrahedra=row['tetrahedra'],pressure_pa=row['inlet_pressure_pa'],dissipation_w=row['physical_dissipation_w'],analytical_errors=b['errors'],analytic_gate_passed=True,body_checks='not_applicable')
    refinement={}
    for length in (4,8):
        a,b=fields[length,4],fields[length,6]
        ratios={k:b['analytical_baseline']['errors'][k]/v for k,v in a['analytical_baseline']['errors'].items()}
        assert max(ratios.values())<1
        refinement[str(length)]=dict(error_ratios_n6_over_n4=ratios,pressure_relative_change=relative(b['inlet_pressure_pa'],a['inlet_pressure_pa']),dissipation_relative_change=relative(b['physical_dissipation_w'],a['physical_dissipation_w']))
    cubes={}
    for length,root,stem in ((4,'c3d-second-normal','L4-body6-second-normal-tensor'),(8,'c3d-retained-margin','L8-body6-second-normal-held-outer2-r6')):
        p=next((R/'build'/root/'runs').glob('*/'+stem+'-receipt.json'));r,row=verify_receipt(p);inputs[str(p)]=sha(p);cubes[length]=row
    raw=force_comparison(cubes[8],cubes[4]);assert not raw['physical_force_gate_passed']
    excess={};newchanges={}
    for length in (4,8):
        excess[str(length)]={k:dict(raw_cube=cubes[length][k],calibrated_empty=fields[length,6][k],obstacle_excess=cubes[length][k]-fields[length,6][k]) for k in ('inlet_pressure_pa','physical_dissipation_w')}
    for k in ('inlet_pressure_pa','physical_dissipation_w'):
        a,b=excess['4'][k],excess['8'][k];delta=b['raw_cube']-a['raw_cube'];empty_delta=b['calibrated_empty']-a['calibrated_empty']
        newchanges[k]=dict(obstacle_excess_relative_change=relative(b['obstacle_excess'],a['obstacle_excess']),raw_cube_delta=delta,empty_duct_delta=empty_delta,fraction_raw_length_delta_explained_by_empty_duct=empty_delta/delta)
    return dict(status='PROGRESS: four calibrated empty baselines adopted for diagnostic obstacle-excess values; original physical cube gates still fail',input_sha256=inputs,measurements=measurements,refinement=refinement,raw_cube_force_comparison=raw,diagnostic_excess=excess,diagnostic_length_changes=newchanges,diagnostic_excess_adopted=True,original_physical_gate_replaced=False,physical_accuracy_certified=False,float64_roundoff_certified=False)

def main():
    out=D/'checkpoint-audit.json';assert not out.exists()
    predecessor=json.loads((D/'predecessor.json').read_text());assert sha(Path(predecessor['path']))==predecessor['sha256']=='eae29bc0f0c85940733db0fade489a5b655372d7afcfc34e26a85ab5528b29e0'
    protected=json.loads((R/'build/c3d-reference-method/completion-audit.json').read_text())['protected_build_hashes']
    for p,h in protected.items():assert sha(R/p)==h
    baseline=json.loads((D/'baseline.json').read_text());changed=[p for p,h in baseline.items() if sha(R/p)!=h];assert set(changed)<={'docs/current_truth.md','docs/README.md'},changed
    support=json.loads((D/'support-test-receipt.json').read_text());assert support['passed'] and support['test_count']==5
    for p,h in support['source_sha256'].items():assert sha(R/p)==h==sha(Path(support['frozen_source'])/Path(p).name)
    assert sha(D/'support-tests-01.log')==support['log_sha256'] and '\nOK\n' in (D/'support-tests-01.log').read_text()
    transforms=json.loads((D/'source-transform-control.json').read_text())
    for t in transforms:
        assert sha(R/t['parent'])==t['parent_sha256'];s=(R/t['parent']).read_text()
        for a,b in t['literal_replacements']:assert a in s;s=s.replace(a,b)
        assert s==(R/t['output']).read_text() and sha(R/t['output'])==t['output_sha256']
    assert sha(D/'source-transform-control.json')==support['transformation_sha256']
    c=comparisons();cp=D/'comparisons.json';assert not cp.exists();cp.write_text(json.dumps(c,indent=2)+'\n')
    sources=sorted((R/'scripts').glob('*empty*duct*.py'))+[Path(__file__),R/'tests/test_cfd_reference3d_empty_duct.py',R/'docs/cfd_3d_empty_baseline_goal.md',R/'docs/cfd_3d_empty_baseline_checkpoint.md',R/'docs/cfd_3d_empty_baseline_next_goal.md']
    result=dict(status=c['status'],persistent_goal_complete=False,stage_1_complete=False,physical_accuracy_certified=False,default_restart_changed=False,predecessor_sha256=predecessor['sha256'],baseline_sha256=sha(D/'baseline.json'),changed_preexisting_files=changed,native_hashes_preserved=protected,test_count=5,support_receipt_sha256=sha(D/'support-test-receipt.json'),comparison_sha256=sha(cp),source_transformation_sha256=sha(D/'source-transform-control.json'),source_sha256={str(p.relative_to(R)):sha(p) for p in sources},receipt_sha256=c['input_sha256'],committed=False,packaged=False,installed=False)
    out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(audit=str(out),sha256=sha(out),status=result['status'])))
if __name__=='__main__':main()
