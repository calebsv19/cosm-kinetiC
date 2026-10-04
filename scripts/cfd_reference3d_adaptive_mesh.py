"""One symmetry-preserving macro refinement guided by immutable equilibrium scores."""
import hashlib
import json
from pathlib import Path
import numpy as np
from skfem import MeshTet
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_preconditioner import array_sha


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def mirror(octant,lengths):
    points=[];cells=[]
    for side in range(8):
        x=octant.p.copy()
        for axis in range(3):
            if side & (1<<axis):x[axis]=lengths[axis]-x[axis]
        cells.append(octant.t+len(points)*octant.nvertices);points.append(x)
    p,t=np.concatenate(points,axis=1),np.concatenate(cells,axis=1)
    unique,inverse=np.unique(np.round(p.T,12),axis=0,return_inverse=True)
    return MeshTet(unique.T,inverse[t])


def adaptive_mesh(diagnostic_path,marks=32):
    assert 1<=marks<=64
    diagnostic=json.loads(diagnostic_path.read_text());assert diagnostic['diagnostic_accepted']
    receipt_path=Path(diagnostic['input_receipt']);assert sha(receipt_path)==diagnostic['input_receipt_sha256']
    receipt=json.loads(receipt_path.read_text())
    for p,digest in receipt['artifact_sha256'].items():assert sha(Path(p))==digest
    original_path=next(Path(p) for p in receipt['artifact_sha256'] if p.endswith('.json'))
    original=json.loads(original_path.read_text())
    assert original['numerically_accepted'] and original['body'] and original['count']==6
    assert not original['split_first_normal'] and not original.get('edge_passes',0)
    axes=[np.array(a) for a in original['axis_nodes_m']];lengths=np.array([original['length'],2.,2.])
    snapshot=Path(diagnostic['input_snapshot']);assert sha(snapshot)==diagnostic['input_snapshot_sha256']
    with np.load(snapshot,allow_pickle=False) as fields:
        saved=MeshTet(fields['vertices_m'],fields['tetrahedra']);lo=fields['lo'];hi=fields['hi']
    octant=MeshTet.init_tensor(*[a[a<=lengths[i]/2+1e-12] for i,a in enumerate(axes)])
    center=octant.p[:,octant.t].mean(axis=1)
    octant=octant.remove_elements(np.flatnonzero(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0)))
    rebuilt=alfeld_split(mirror(octant,lengths))
    assert array_sha(rebuilt.p,rebuilt.t)==original['identity']['mesh_sha256']
    assert np.array_equal(rebuilt.p,saved.p) and np.array_equal(rebuilt.t,saved.t)
    score=np.array(diagnostic['equilibrium_indicator_squared_per_tet'])
    assert score.shape==(saved.nelements,) and np.all(np.isfinite(score)) and np.all(score>=0)
    macro_scores=score.reshape(4,-1).sum(axis=0)
    centers=saved.p[:,np.unique(saved.t.max(axis=0))]
    folded=np.minimum(centers,lengths[:,None]-centers)
    unique,inverse=np.unique(np.round(folded.T,12),axis=0,return_inverse=True)
    assert np.all(np.bincount(inverse)==8)
    orbit_scores=np.bincount(inverse,weights=macro_scores)
    lookup={tuple(c):i for i,c in enumerate(unique)}
    octant_centers=octant.p[:,octant.t].mean(axis=1).T
    orbit_indices=np.array([lookup[tuple(c)] for c in np.round(octant_centers,12)])
    assert len(np.unique(orbit_indices))==octant.nelements
    local_scores=orbit_scores[orbit_indices]
    selected=np.lexsort((np.arange(octant.nelements),-local_scores))[:marks]
    refined=octant.refined(selected);macro=mirror(refined,lengths);mesh=alfeld_split(macro)
    assert mesh.nelements<=50000
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    exterior=np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)
    assert np.all(exterior|body(mid)), 'false internal wall from adaptive mirror'
    assert abs(np.abs(mesh.mapping().detA).sum()/6-(np.prod(lengths)-1))<1e-9
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],lengths[0]),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    metadata={'diagnostic_sha256':sha(diagnostic_path),'input_snapshot_sha256':sha(snapshot),
        'marked_macros_per_octant':marks,'selected_octant_macro_indices':selected.tolist(),
        'selected_octant_centers_m':octant_centers[selected].tolist(),
        'captured_indicator_fraction':float(local_scores[selected].sum()/local_scores.sum()),
        'original_tetrahedra':saved.nelements,'refined_tetrahedra':mesh.nelements,
        'background_axes_preserved':True,'scope':'one symmetric macro refinement from diagnostic scores; not a force-error bound'}
    return (mesh,lo,hi,axes,macro.nelements),metadata
