"""One bounded outer macro refinement from an immutable P4 stress observation."""
import json
from pathlib import Path
import numpy as np
from skfem import MeshTet
from scipy.spatial import cKDTree
from cfd_reference3d_adaptive_mesh import sha,mirror
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_preconditioner import array_sha
from cfd_reference3d_graded_probe import numerical_failure_reasons


def selective_mesh(diagnostic_path,marks=1):
    if marks not in (1,2,4,8):raise ValueError('unsupported bounded outer mark count')
    diagnostic_path=Path(diagnostic_path).resolve()
    observer_receipt=diagnostic_path.with_name(diagnostic_path.stem+'-receipt.json')
    observer=json.loads(observer_receipt.read_text())
    assert observer['returncode']==0 and observer['diagnostic_failure'] is None and observer['stop_reason'] is None
    assert observer['artifact_sha256'][str(diagnostic_path)]==sha(diagnostic_path)
    for path,digest in observer['artifact_sha256'].items():assert sha(Path(path))==digest
    for name,digest in observer['source_sha256'].items():assert sha(observer_receipt.parent/'source'/name)==digest
    diagnostic=json.loads(diagnostic_path.read_text())
    assert diagnostic['diagnostic_accepted'] and diagnostic['input_complete_numerical_gates_passed']
    receipt_path=Path(diagnostic['input_receipt']);assert sha(receipt_path)==diagnostic['input_receipt_sha256']
    receipt=json.loads(receipt_path.read_text());assert receipt['returncode']==0 and receipt['diagnostic_failure'] is None and receipt['stop_reason'] is None
    for path,digest in receipt['artifact_sha256'].items():assert sha(Path(path))==digest
    for name,digest in receipt['source_sha256'].items():assert sha(receipt_path.parent/'source'/name)==digest
    command=receipt['command'];result_path=Path(command[command.index('--output')+1]);snapshot=Path(command[command.index('--snapshot')+1])
    assert sha(result_path)==diagnostic['input_result_sha256']
    assert str(snapshot)==diagnostic['input_snapshot'] and sha(snapshot)==diagnostic['input_snapshot_sha256']
    original=json.loads(result_path.read_text());assert original['numerically_accepted'] and not numerical_failure_reasons(original)
    assert original['length']==8. and original['count']==4 and original['body'] and original['split_first_normal']
    assert original['domain_mesh_mode']=='held_l4' and original['outer_layers']==1
    assert (original['velocity_degree'],original['pressure_degree'])==(4,3)
    axes=[np.array(a) for a in original['axis_nodes_m']];lengths=np.array([8.,2.,2.])
    with np.load(snapshot,allow_pickle=False) as fields:
        saved=MeshTet(fields['vertices_m'],fields['tetrahedra']);lo=fields['lo'];hi=fields['hi']
    octant=MeshTet.init_tensor(*[a[a<=lengths[i]/2+1e-12] for i,a in enumerate(axes)])
    center=octant.p[:,octant.t].mean(axis=1)
    octant=octant.remove_elements(np.flatnonzero(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0)))
    rebuilt=alfeld_split(mirror(octant,lengths))
    assert array_sha(saved.p,saved.t)==original['identity']['mesh_sha256']
    # Recomputing affine macro barycenters changes floating summation order.
    np.testing.assert_allclose(rebuilt.p,saved.p,rtol=0,atol=1e-14)
    np.testing.assert_array_equal(rebuilt.t,saved.t)
    score=np.array(diagnostic['equilibrium_indicator_squared_per_tet'])
    assert score.shape==(saved.nelements,) and np.all(np.isfinite(score)) and np.all(score>=0)
    # Alfeld children are four batches in original sorted macro order.
    macro_scores=score.reshape(4,-1).sum(axis=0)
    centers=saved.p[:,np.unique(saved.t.max(axis=0))]
    assert centers.shape[1]==len(macro_scores)
    folded=np.minimum(centers,lengths[:,None]-centers)
    unique,inverse=np.unique(np.round(folded.T,12),axis=0,return_inverse=True)
    assert np.all(np.bincount(inverse)==8)
    orbit_scores=np.bincount(inverse,weights=macro_scores)
    octant_centers=octant.p[:,octant.t].mean(axis=1).T
    distance,indices=cKDTree(unique).query(octant_centers)
    assert np.max(distance)<1e-11
    assert len(np.unique(indices))==octant.nelements
    local_scores=orbit_scores[indices]
    outer=np.flatnonzero(octant_centers[:,0]<axes[0][1])
    assert len(outer)>=marks and local_scores.sum()>0
    selected=outer[np.lexsort((outer,-local_scores[outer]))[:marks]]
    refined=octant.refined(selected);macro=mirror(refined,lengths);mesh=alfeld_split(macro)
    assert mesh.nelements<=50000 and np.min(np.abs(mesh.mapping().detA))>0
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    exterior=np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)
    assert np.all(exterior|body(mid)), 'false internal wall after selective mirror'
    assert abs(np.abs(mesh.mapping().detA).sum()/6-31.)<1e-9
    def body_triangles(m):
        boundary=m.boundary_facets();coords=m.p[:,m.facets[:,boundary[body(m.p[:,m.facets[:,boundary]].mean(axis=1))]]]
        return {tuple(sorted(map(tuple,np.round(coords[:,:,i].T,12)))) for i in range(coords.shape[2])}
    assert body_triangles(mesh)==body_triangles(saved), 'body surface triangulation changed'
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],8),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    metadata=dict(observer_receipt_sha256=sha(observer_receipt),diagnostic_sha256=sha(diagnostic_path),input_receipt_sha256=sha(receipt_path),input_snapshot_sha256=sha(snapshot),
        macro_center_match_max_m=float(np.max(distance)),marked_macros_per_octant=marks,selected_octant_macro_indices=selected.tolist(),selected_octant_centers_m=octant_centers[selected].tolist(),
        captured_total_indicator_fraction=float(local_scores[selected].sum()/local_scores.sum()),
        captured_outer_indicator_fraction=float(local_scores[selected].sum()/local_scores[outer].sum()),
        original_macros_per_octant=octant.nelements,refined_macros_per_octant=refined.nelements,
        original_tetrahedra=saved.nelements,refined_tetrahedra=mesh.nelements,near_body_axes_preserved=True,body_facets_unchanged=True,
        scope='one symmetric conforming outer macro refinement; indicator is not a force-error bound')
    return (mesh,lo,hi,axes,macro.nelements),metadata
