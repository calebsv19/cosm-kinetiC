#!/usr/bin/env python3
"""Read sealed fields/observers and report signed force-gap and geometry attribution."""
import hashlib,json
from pathlib import Path
import numpy as np
from skfem import MeshTet
from audit_cfd_3d_spatial import verify_receipt
ROOT=Path(__file__).resolve().parents[1];DATA=ROOT/'build/c3d-normal-domain'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def analyze(label,receipt_path,observer_path):
    receipt,row=verify_receipt(receipt_path)
    observed=json.loads(observer_path.read_text());assert observed['diagnostic_accepted'] and observed['input_complete_numerical_gates_passed']
    assert observed['input_receipt_sha256']==sha(receipt_path)
    observer_receipt=observer_path.with_name(observer_path.stem+'-receipt.json');proof=json.loads(observer_receipt.read_text())
    assert proof['returncode']==0 and proof['stop_reason'] is None and proof['diagnostic_failure'] is None
    for p,h in proof['artifact_sha256'].items():assert sha(Path(p))==h
    snapshot=receipt_path.with_name(receipt_path.name.removesuffix('-receipt.json')+'.npz')
    with np.load(snapshot,allow_pickle=False) as field:
        mesh=MeshTet(field['vertices_m'],field['tetrahedra']);lo=field['lo'];hi=field['hi']
    condition=np.linalg.cond(mesh.mapping().A.transpose(2,0,1));centers=mesh.p[:,mesh.t].mean(axis=1)
    first=row['axis_nodes_m'][0][1];last=row['axis_nodes_m'][0][-2]
    end_slabs=(centers[0]<first)|(centers[0]>last)
    groups={}
    for name,mask in (('extended_end_slabs',end_slabs),('held_inner_slabs',~end_slabs)):
        groups[name]=dict(tetrahedra=int(mask.sum()),jacobian_condition_max=float(condition[mask].max()),jacobian_condition_median=float(np.median(condition[mask])),minimum_absolute_jacobian_determinant=float(np.abs(mesh.mapping().detA)[mask].min()))
    raw=np.array(row['pressure_force_n'])+row['raw_symmetric_viscous_force_n'];reaction=np.array(row['reaction_force_n']);lifts=[]
    for lift in observed['lifts']:
        jump=sum(np.array(lift[p]['interior_jump_centroid_buckets_n']) for p in ('pressure','viscous'))
        volume=sum(np.array(lift[p]['volume_divergence_centroid_buckets_n']) for p in ('pressure','viscous'))
        signed=jump-volume
        weak=sum(np.array(lift[p]['weak_load_n']) for p in ('pressure','viscous'))
        np.testing.assert_allclose(signed.sum(axis=0),raw-weak,rtol=0,atol=1e-9)
        np.testing.assert_allclose(weak,reaction,rtol=0,atol=1e-9)
        assert all(np.max(np.abs(lift[p]['identity_error_n']))<1e-9 for p in ('pressure','viscous'))
        order=np.argsort(-np.abs(signed[:,0]))
        lifts.append(dict(shell_m=lift['shell_m'],weak_reaction_max_absolute_difference_n=float(np.max(np.abs(weak-reaction))),signed_total_jump_minus_volume_centroid_buckets_n=signed.tolist(),drag_absolute_bucket_rank=order.tolist(),total_signed_drag_defect_n=float(signed[:,0].sum()),signed_total_jump_drag_n=float(jump[:,0].sum()),signed_total_volume_drag_n=float(volume[:,0].sum())))
    return dict(length_m=row['length'],geometry_groups=groups,raw_minus_reaction_force_n=(raw-reaction).tolist(),signed_lifts=lifts,volume_strong_equilibrium_defect_l2=observed['volume_strong_equilibrium_defect_l2'],interior_stress_jump_l2=observed['interior_stress_jump_l2'],global_h_squared_volume_buckets=observed['h_squared_volume_defect_centroid_buckets'],global_h_weighted_jump_buckets=observed['h_weighted_jump_defect_centroid_buckets'],centroid_bucket_upper_distances_m=observed['centroid_bucket_upper_distances_m'])

def main():
    inputs={}
    for label,folder,stem in (('L4','c3d-factor-catalog','L4-body6-normal-catalog'),('L8','c3d-normal-domain','L8-body6-normal-held')):
        receipt=next((ROOT/'build'/folder/'runs').glob('*/'+stem+'-receipt.json'));observed=next((ROOT/'build'/folder/'observer-runs').glob('*/'+stem+'-stress.json'))
        inputs[label]=(receipt,observed)
    report=dict(schema='physics_sim_c3d_normal_domain_force_localization_v1',physical_accuracy_certified=False,reference_equations_unchanged=True,results={name:analyze(name,*paths) for name,paths in inputs.items()},input_sha256={str(p):sha(p) for paths in inputs.values() for p in paths},analysis_source_sha256=sha(Path(__file__)),scope='exact signed weak-lift identity on accepted approximate fields; centroid buckets are not clipped regions or force-error estimates; ranking varies with lift and is not a proof of refinement benefit')
    output=DATA/'force-localization.json';assert not output.exists();output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({name:dict(geometry_groups=row['geometry_groups'],raw_minus_reaction_force_n=row['raw_minus_reaction_force_n'],signed_lifts=row['signed_lifts']) for name,row in report['results'].items()},indent=2))
if __name__=='__main__':main()
