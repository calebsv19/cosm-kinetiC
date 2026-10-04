#!/usr/bin/env python3
"""Eight predeclared body grids screened before factorization with immutable inputs."""
import argparse,hashlib,json,time,resource
from pathlib import Path
import numpy as np
from cfd_reference3d_balanced_body_mesh import build,quality,evaluate,PROFILES,NORMALS,LATERALS
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_domain_budget import enforce_phase,PhaseResourceStopped

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(input_receipt,geometry_path):
    begin=time.monotonic();r=json.loads(input_receipt.read_text());assert r['returncode']==0 and r['stop_reason'] is None and r['diagnostic_failure'] is None
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    inp=Path(r['command'][r['command'].index('--snapshot')+1]);result=Path(r['command'][r['command'].index('--output')+1]);row=json.loads(result.read_text())
    assert row['numerically_accepted'] and row['length']==4 and row['tetrahedra']==28416 and row['final_residual']['true_residual']<1e-10 and row['flux_error']<1e-8 and row['volume_divergence_max_s_inv']<1e-8 and row['physical_energy_imbalance']<.03
    bundle,_=second_normal_tensor_mesh(4.);old,lo,hi,axes,n=bundle
    with np.load(inp,allow_pickle=False) as z:
        np.testing.assert_array_equal(z['vertices_m'],old.p);np.testing.assert_array_equal(z['tetrahedra'],old.t);np.testing.assert_array_equal(z['lo'],lo);np.testing.assert_array_equal(z['hi'],hi)
    original=quality(old,lo,hi);candidates=[];winner=None;winning_key=None
    for profile in PROFILES:
        for normal in NORMALS:
            for lateral in LATERALS:
                name=f'{profile}-{normal}-yz{lateral}';bundle=build(profile,normal,lateral);mesh,blo,bhi,baxes,macros=bundle;metrics=quality(mesh,blo,bhi);reasons=evaluate(original,metrics)
                c=dict(name=name,profile=profile,normal=normal,lateral_distance_m=lateral,axis_nodes_m=[a.tolist() for a in baxes],quality=metrics,geometry_accepted=not reasons,reasons=reasons,original_macro_partition_claimed=False,original_tetrahedron_partition_claimed=False,body_surface_triangles_preserved=False)
                candidates.append(c);print(json.dumps(dict(phase=name,**c)),flush=True);enforce_phase(name,resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,time.monotonic()-begin)
                if not reasons:
                    key=(metrics['bands']['edge-0.05']['weighted_mean_shape'],mesh.nelements,name)
                    if winner is None or key<winning_key:winner=(bundle,c);winning_key=key
    selected=None
    if winner is not None:
        (mesh,blo,bhi,baxes,macros),selected=winner
        with geometry_path.open('xb') as h:np.savez_compressed(h,vertices_m=mesh.p,tetrahedra=mesh.t,lo=blo,hi=bhi,axis0=baxes[0],axis1=baxes[1],axis2=baxes[2],macro_tetrahedra=macros)
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    return dict(diagnostic_accepted=True,numerically_accepted=False,numeric_factor_attempted=False,numerical_field_published=False,physical_accuracy_certified=False,input_receipt=str(input_receipt),input_receipt_sha256=sha(input_receipt),input_snapshot_sha256=sha(inp),original_saved_mesh_rebuilt_bitwise=True,original_quality=original,candidates=candidates,selected_case=selected,geometry_path=str(geometry_path) if winner is not None else None,geometry_sha256=sha(geometry_path) if winner is not None else None,wall_s=time.monotonic()-begin,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,scope='geometry-only prospective remesh; no old parent-partition, numerical field, force error bound or certification')
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--input-receipt',type=Path,required=True);ap.add_argument('--geometry',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.geometry.exists()
    try:row=run(a.input_receipt,a.geometry)
    except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,numerically_accepted=False,numeric_factor_attempted=False,numerical_field_published=False,physical_accuracy_certified=False,resource_phase_rejected=e.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(dict(diagnostic_accepted=row['diagnostic_accepted'],selected_case=row.get('selected_case'),wall_s=row.get('wall_s'))),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
