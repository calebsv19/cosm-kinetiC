"""Signed-force-ranked, conforming symmetric macro candidate with shape admission."""
import json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree
from skfem import MeshTet
from cfd_reference3d_adaptive_mesh import sha,mirror
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_preconditioner import array_sha
from cfd_reference3d_mesh import edge_distance
from cfd_reference3d_corner_local_mesh import edge_star_refined,shape_inverse,LocalGeometryRejected
from cfd_reference3d_graded_probe import numerical_failure_reasons


def force_ranked_input(diagnostic_path):
    diagnostic_path=Path(diagnostic_path).resolve()
    observer_path=diagnostic_path.with_name(diagnostic_path.stem+'-receipt.json')
    observer=json.loads(observer_path.read_text())
    assert observer['returncode']==0 and observer['stop_reason'] is None and observer['diagnostic_failure'] is None
    for p,h in observer['artifact_sha256'].items():assert sha(Path(p))==h
    for name,h in observer['source_sha256'].items():assert sha(observer_path.parent/'source'/name)==h
    assert observer['artifact_sha256'][str(diagnostic_path)]==sha(diagnostic_path)
    diagnostic=json.loads(diagnostic_path.read_text())
    assert diagnostic['diagnostic_accepted'] and diagnostic['input_complete_numerical_gates_passed']
    receipt_path=Path(diagnostic['input_receipt']);assert sha(receipt_path)==diagnostic['input_receipt_sha256']
    receipt=json.loads(receipt_path.read_text())
    assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for name,h in receipt['source_sha256'].items():assert sha(receipt_path.parent/'source'/name)==h
    result_path=Path(receipt['command'][receipt['command'].index('--output')+1]);snapshot=Path(diagnostic['input_snapshot'])
    assert sha(result_path)==diagnostic['input_result_sha256'] and sha(snapshot)==diagnostic['input_snapshot_sha256']
    original=json.loads(result_path.read_text());assert original['numerically_accepted'] and not numerical_failure_reasons(original)
    assert original['final_residual']['true_residual']<1e-10
    assert original['length']==8. and original['count']==6 and original['split_first_normal'] and original['outer_layers']==2 and original['domain_mesh_mode']=='held_l4'
    assert (original['velocity_degree'],original['pressure_degree'])==(4,3)
    with np.load(snapshot,allow_pickle=False) as s:
        saved=MeshTet(s['vertices_m'],s['tetrahedra']);lo=s['lo'];hi=s['hi']
    axes=[np.array(a) for a in original['axis_nodes_m']];lengths=np.array([8.,2.,2.])
    octant=MeshTet.init_tensor(*[a[a<=lengths[i]/2+1e-12] for i,a in enumerate(axes)])
    center=octant.p[:,octant.t].mean(axis=1)
    octant=octant.remove_elements(np.flatnonzero(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0)))
    rebuilt=alfeld_split(mirror(octant,lengths))
    assert array_sha(saved.p,saved.t)==original['identity']['mesh_sha256']
    np.testing.assert_allclose(rebuilt.p,saved.p,rtol=0,atol=1e-14);np.testing.assert_array_equal(rebuilt.t,saved.t)
    attribution=Path(diagnostic['signed_attribution_path']);assert sha(attribution)==diagnostic['signed_attribution_sha256']
    with np.load(attribution,allow_pickle=False) as s:
        net=s['cell_net_n'];assert net.shape==(2,2,3,saved.nelements) and np.all(np.isfinite(net))
        np.testing.assert_allclose(s['cell_centers_m'],saved.p[:,saved.t].mean(axis=1),rtol=0,atol=1e-14)
        # Sum signed children before magnitude; preserve pressure/viscous cancellation.
        macro_drag=net.sum(axis=1)[:,0].reshape(2,4,-1).sum(axis=1)
        macro_score=np.max(np.abs(macro_drag),axis=0)
    centers=saved.p[:,np.unique(saved.t.max(axis=0))]
    assert centers.shape[1]==len(macro_score)
    folded=np.minimum(centers,lengths[:,None]-centers)
    unique,inverse=np.unique(np.round(folded.T,12),axis=0,return_inverse=True)
    assert np.all(np.bincount(inverse)==8)
    orbit_scores=np.bincount(inverse,weights=macro_score)
    octant_centers=octant.p[:,octant.t].mean(axis=1).T
    distance,indices=cKDTree(unique).query(octant_centers)
    assert np.max(distance)<1e-11 and len(np.unique(indices))==octant.nelements
    scores=orbit_scores[indices]
    distance,swap=cKDTree(octant_centers).query(octant_centers[:,[0,2,1]])
    assert np.max(distance)<1e-11
    near=np.flatnonzero(edge_distance(octant_centers.T,lo,hi)<.15)
    pairs={tuple(sorted({int(i),int(swap[i])})) for i in near}
    pairs=sorted(pairs,key=lambda pair:(-float(scores[list(pair)].sum()),pair))
    assert pairs and scores.sum()>0
    binding=dict(observer_receipt_sha256=sha(observer_path),diagnostic_sha256=sha(diagnostic_path),input_receipt_sha256=sha(receipt_path),input_snapshot_sha256=sha(snapshot),signed_attribution_sha256=sha(attribution),
        original_mesh_sha256=original['identity']['mesh_sha256'],score_scope='maximum absolute sum of signed Alfeld-child drag loads across lifts, then sum eight symmetric orbit magnitudes; not an error bound')
    return saved,octant,lo,hi,axes,lengths,scores,pairs,binding


def force_local_mesh(diagnostic_path,rank=0,method='bisection'):
    if not isinstance(rank,int) or not 0<=rank<8 or method not in ('bisection','edge_star'):raise ValueError('unsupported force-local geometry request')
    old,octant,lo,hi,axes,lengths,scores,pairs,binding=force_ranked_input(diagnostic_path)
    if rank>=len(pairs):raise ValueError('missing paired candidate')
    selected=np.array(pairs[rank]);centers=octant.p[:,octant.t].mean(axis=1).T
    refined=octant.refined(selected) if method=='bisection' else edge_star_refined(octant,selected)
    def keys(m):return [tuple(sorted(map(tuple,np.round(m.p[:,v].T,12)))) for v in m.t.T]
    old_keys,new_keys=keys(octant),keys(refined);old_set,new_set=set(old_keys),set(new_keys)
    removed=np.array([i for i,k in enumerate(old_keys) if k not in new_set]);added=np.array([i for i,k in enumerate(new_keys) if k not in old_set])
    assert len(removed)>0 and len(added)>len(removed)
    old_local,new_local=alfeld_split(octant),alfeld_split(refined)
    oi=np.concatenate([removed+k*octant.nelements for k in range(4)]);ni=np.concatenate([added+k*refined.nelements for k in range(4)])
    old_shape,new_shape=shape_inverse(old_local),shape_inverse(new_local)
    macro=mirror(refined,lengths);mesh=alfeld_split(macro)
    assert mesh.nelements<=50000 and np.min(np.abs(mesh.mapping().detA))>0
    assert abs(np.abs(mesh.mapping().detA).sum()/6-31.)<1e-9
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    assert np.all(np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)|body(mid)), 'false internal wall'
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],8),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    areas={}
    for name,facets in mesh.boundaries.items():
        v=mesh.p[:,mesh.facets[:,facets]]
        areas[name]=float(np.linalg.norm(np.cross((v[:,1]-v[:,0]).T,(v[:,2]-v[:,0]).T),axis=1).sum()/2)
        assert abs(areas[name]-{'body':6.,'inlet':4.,'outlet':4.,'walls':64.}[name])<1e-9
    for axis,L in enumerate(lengths):
        reflected=mesh.p.copy();reflected[axis]=L-reflected[axis]
        assert set(map(tuple,np.round(mesh.p.T,11)))==set(map(tuple,np.round(reflected.T,11)))
    meta=dict(**binding,rank=rank,method=method,selected_octant_macro_indices=selected.tolist(),selected_octant_centers_m=centers[selected].tolist(),
        selected_edge_distance_m=edge_distance(centers[selected].T,lo,hi).tolist(),captured_score_fraction=float(scores[selected].sum()/scores.sum()),
        original_tetrahedra=old.nelements,refined_tetrahedra=mesh.nelements,affected_original_macros_per_octant=len(removed),affected_refined_macros_per_octant=len(added),
        original_affected_worst_shape=float(old_shape[oi].max()),refined_affected_worst_shape=float(new_shape[ni].max()),original_affected_mean_shape=float(old_shape[oi].mean()),refined_affected_mean_shape=float(new_shape[ni].mean()),
        original_global_max_condition=float(np.linalg.cond(old.mapping().A.transpose(2,0,1)).max()),refined_global_max_condition=float(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max()),
        boundary_areas_m2=areas,scope='force-ranked prospective conforming symmetric local refinement; geometry admission does not imply force convergence')
    reasons=[]
    if meta['refined_affected_worst_shape']>meta['original_affected_worst_shape']*(1+1e-8):reasons.append('affected intrinsic worst shape worsened')
    if meta['refined_affected_mean_shape']>=meta['original_affected_mean_shape']*(1-1e-8):reasons.append('affected intrinsic mean shape did not improve')
    if meta['refined_global_max_condition']>meta['original_global_max_condition']*(1+1e-8):reasons.append('global Jacobian conditioning worsened')
    if reasons:raise LocalGeometryRejected(meta,reasons)
    return (mesh,lo,hi,axes,macro.nelements),meta
