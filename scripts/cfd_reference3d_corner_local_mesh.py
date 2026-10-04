"""Prospective conforming corner macro refinement with a measured shape gate."""
import numpy as np
from skfem import MeshTet
from scipy.spatial import cKDTree
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_adaptive_mesh import mirror
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_mesh import edge_distance


class LocalGeometryRejected(ValueError):
    def __init__(self,metadata,reasons):
        self.metadata=metadata;self.reasons=reasons
        super().__init__('; '.join(reasons))


def octant_for(length=8.):
    mesh,lo,hi,axes,macros=domain_mesh(length,4,True,mode='held_l4')
    lengths=np.array([length,2.,2.])
    octant=MeshTet.init_tensor(*[a[a<=lengths[i]/2+1e-12] for i,a in enumerate(axes)])
    center=octant.p[:,octant.t].mean(axis=1)
    octant=octant.remove_elements(np.flatnonzero(np.all((center>lo[:,None])&(center<hi[:,None]),axis=0)))
    return mesh,octant,lo,hi,axes,lengths


def edge_star_refined(mesh,marked):
    # Split each selected original edge across its entire star.  Every incident
    # tetrahedron shares the same midpoint, preventing hanging face edges.
    from itertools import combinations
    edges={tuple(sorted(edge)) for i in marked for edge in combinations(mesh.t[:,i],2)}
    ordered=sorted(edges,key=lambda e:(-float(np.sum((mesh.p[:,e[0]]-mesh.p[:,e[1]])**2)),e))
    points=mesh.p.copy();cells=mesh.t.copy()
    for a,b in ordered:
        affected=np.flatnonzero(np.any(cells==a,axis=0)&np.any(cells==b,axis=0))
        if not len(affected):continue
        midpoint=points.shape[1];points=np.column_stack((points,(points[:,a]+points[:,b])/2))
        left=cells[:,affected].copy();right=left.copy()
        left[left==b]=midpoint;right[right==a]=midpoint
        cells[:,affected]=left;cells=np.column_stack((cells,right))
    return MeshTet(points,cells)


def shape_inverse(mesh):
    points=mesh.p[:,mesh.t]
    edge_squared=sum(np.sum((points[:,i]-points[:,j])**2,axis=0) for i in range(4) for j in range(i))
    determinant=np.abs(mesh.mapping().detA)
    # Equals one for a regular tetrahedron; independent of labels and scale.
    return edge_squared/(12*(determinant/2)**(2/3))


def local_corner_mesh(length=8.,selected=(0,),method='bisection'):
    if length not in (4.,8.):raise ValueError('unsupported length')
    old,octant,lo,hi,axes,lengths=octant_for(length)
    selected=np.array(selected,dtype=int)
    if len(selected) not in (1,2,4) or len(np.unique(selected))!=len(selected) or np.any(selected<0) or np.any(selected>=octant.nelements):raise ValueError('unsupported local mark set')
    original_centers=octant.p[:,octant.t].mean(axis=1).T
    assert np.all(edge_distance(original_centers[selected].T,lo,hi)<.15)
    assert np.all(original_centers[selected,0]>axes[0][1]), 'long outer macros excluded'
    if method not in ('bisection','edge_star'):raise ValueError('unsupported conforming refinement')
    refined=octant.refined(selected) if method=='bisection' else edge_star_refined(octant,selected)
    # Compare the same affected physical parents, independent of renumbering.
    def keys(m):
        return [tuple(sorted(map(tuple,np.round(m.p[:,vertices].T,12)))) for vertices in m.t.T]
    old_keys=keys(octant);new_keys=keys(refined);old_set=set(old_keys);new_set=set(new_keys)
    removed=np.array([i for i,k in enumerate(old_keys) if k not in new_set]);added=np.array([i for i,k in enumerate(new_keys) if k not in old_set])
    assert len(removed)>0 and len(added)>len(removed)
    old_local=alfeld_split(octant);new_local=alfeld_split(refined)
    old_shape=shape_inverse(old_local);new_shape=shape_inverse(new_local)
    old_condition=np.linalg.cond(old_local.mapping().A.transpose(2,0,1))
    new_condition=np.linalg.cond(new_local.mapping().A.transpose(2,0,1))
    old_changed=np.concatenate([removed+k*octant.nelements for k in range(4)])
    new_changed=np.concatenate([added+k*refined.nelements for k in range(4)])
    macro=mirror(refined,lengths);mesh=alfeld_split(macro)
    assert mesh.nelements<=50000 and np.min(np.abs(mesh.mapping().detA))>0
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    exterior=np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)
    assert np.all(exterior|body(mid)), 'false wall after local corner refinement'
    assert abs(np.abs(mesh.mapping().detA).sum()/6-(4*length-1))<1e-9
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    full_condition=np.linalg.cond(mesh.mapping().A.transpose(2,0,1));original_condition=np.linalg.cond(old.mapping().A.transpose(2,0,1))
    metadata=dict(refinement_method=method,selected_octant_macro_indices=selected.tolist(),selected_octant_centers_m=original_centers[selected].tolist(),
        selected_edge_distance_m=edge_distance(original_centers[selected].T,lo,hi).tolist(),
        affected_original_macros_per_octant=len(removed),affected_refined_macros_per_octant=len(added),
        original_tetrahedra=old.nelements,refined_tetrahedra=mesh.nelements,
        original_affected_worst_shape=float(old_shape[old_changed].max()),refined_affected_worst_shape=float(new_shape[new_changed].max()),
        original_affected_mean_shape=float(old_shape[old_changed].mean()),refined_affected_mean_shape=float(new_shape[new_changed].mean()),
        original_affected_max_condition=float(old_condition[old_changed].max()),refined_affected_max_condition=float(new_condition[new_changed].max()),
        original_affected_mean_condition=float(old_condition[old_changed].mean()),refined_affected_mean_condition=float(new_condition[new_changed].mean()),
        original_global_max_condition=float(original_condition.max()),refined_global_max_condition=float(full_condition.max()),
        original_global_worst_shape=float(shape_inverse(old).max()),refined_global_worst_shape=float(shape_inverse(mesh).max()),
        original_macro_indices_affected=removed.tolist(),near_body_axes_preserved=True,body_shape_preserved=True,
        scope='one conforming corner macro refinement, shape-gated before solve; no force-error bound')
    reasons=[]
    if metadata['refined_affected_max_condition']>=metadata['original_affected_max_condition']*(1-1e-8):reasons.append('affected worst shape did not improve')
    if metadata['refined_global_max_condition']>metadata['original_global_max_condition']*(1+1e-8):reasons.append('global worst shape worsened')
    if reasons:raise LocalGeometryRejected(metadata,reasons)
    return (mesh,lo,hi,axes,macro.nelements),metadata


def paired_candidates(length=8.):
    _,octant,lo,hi,axes,_=octant_for(length)
    centers=octant.p[:,octant.t].mean(axis=1).T
    distance,swap=cKDTree(centers).query(centers[:,[0,2,1]])
    assert np.max(distance)<1e-10
    edge=edge_distance(centers.T,lo,hi)
    near=np.flatnonzero((edge<.15)&(centers[:,0]>axes[0][1]))
    pairs={tuple(sorted((int(i),int(swap[i])))) for i in near}
    return sorted(pairs,key=lambda pair:(float(edge[list(pair)].max()),pair))
