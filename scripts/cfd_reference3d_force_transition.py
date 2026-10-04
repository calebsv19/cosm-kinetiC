"""Bounded original-edge relocation for conforming, mirrored macro transitions."""
import numpy as np
from scipy.spatial import cKDTree
from skfem import MeshTet
from skfem.refdom import RefTet
from cfd_reference3d_force_local_mesh import force_ranked_input
from cfd_reference3d_corner_local_mesh import shape_inverse,LocalGeometryRejected
from cfd_reference3d_adaptive_mesh import mirror
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_mesh import edge_distance
FRACTIONS=(3/8,7/16,1/2,9/16,5/8)


def alfeld_shape(points,cells):
    vertices=points[:,cells];center=vertices.mean(axis=1,keepdims=True)
    children=np.concatenate([np.concatenate((vertices[:,face],center),axis=1) for face in RefTet.facets],axis=2)
    determinant=np.abs(np.linalg.det((children[:,1:]-children[:,:1]).transpose(2,0,1)))
    if np.any(determinant<=0) or not np.all(np.isfinite(determinant)):raise ValueError('invalid prospective child mapping')
    edge_squared=sum(np.sum((children[:,i]-children[:,j])**2,axis=0) for i in range(4) for j in range(i))
    return edge_squared/(12*(determinant/2)**(2/3))


def changed_macros(old,new):
    def keys(mesh):return [tuple(sorted(map(tuple,np.round(mesh.p[:,v].T,12)))) for v in mesh.t.T]
    old_keys,new_keys=keys(old),keys(new);old_set,new_set=set(old_keys),set(new_keys)
    removed=np.array([i for i,k in enumerate(old_keys) if k not in new_set],dtype=int)
    added=np.array([i for i,k in enumerate(new_keys) if k not in old_set],dtype=int)
    if not len(removed) or len(added)<=len(removed):raise ValueError('missing conforming macro subdivision')
    return removed,added


def edge_groups(old,new):
    np.testing.assert_array_equal(new.p[:,:old.nvertices],old.p)
    edges=old.edges;midpoints=old.p[:,edges].mean(axis=1).T
    ids=np.arange(old.nvertices,new.nvertices)
    if not len(ids):raise ValueError('no inserted edge points')
    distance,indices=cKDTree(midpoints).query(new.p[:,ids].T)
    if np.max(distance)>1e-12 or len(np.unique(indices))!=len(ids):raise ValueError('inserted point is not a unique original midpoint')
    points=new.p[:,ids].T;distance,swapped=cKDTree(points).query(points[:,[0,2,1]])
    if np.max(distance)>1e-12:raise ValueError('inserted edge set does not preserve Y/Z symmetry')
    groups=[];visited=set()
    for i,vertex in enumerate(ids):
        if i in visited:continue
        orbit=sorted({i,int(swapped[i])});visited.update(orbit)
        representative=edges[:,indices[i]];a,b=old.p[:,representative].T
        entries=[];fixed=False
        for j in orbit:
            edge=edges[:,indices[j]];c,d=old.p[:,edge].T
            if j==i:reverse=False
            elif np.allclose(a[[0,2,1]],c,rtol=0,atol=1e-12) and np.allclose(b[[0,2,1]],d,rtol=0,atol=1e-12):reverse=False
            elif np.allclose(a[[0,2,1]],d,rtol=0,atol=1e-12) and np.allclose(b[[0,2,1]],c,rtol=0,atol=1e-12):reverse=True
            else:raise ValueError('Y/Z partner endpoints do not match')
            if j==i and len(orbit)==1 and np.allclose(a[[0,2,1]],b,rtol=0,atol=1e-12):fixed=True
            entries.append(dict(vertex=int(ids[j]),edge=edge.tolist(),reverse=reverse))
        groups.append(dict(entries=entries,fixed_midpoint=fixed))
    return groups


def apply_fraction(points,original_points,group,fraction):
    if fraction not in FRACTIONS or (group['fixed_midpoint'] and fraction!=.5):raise ValueError('unsupported original-edge position')
    for entry in group['entries']:
        t=1-fraction if entry['reverse'] else fraction;a,b=entry['edge']
        points[:,entry['vertex']]=(1-t)*original_points[:,a]+t*original_points[:,b]


def partition_check(old,new,removed,added):
    # Every replacement vertex stays inside exactly one removed parent; compare
    # per-parent volumes, not only total volume which can hide overlap/gaps.
    parent_points=old.p[:,old.t[:,removed]]
    inverses=np.linalg.inv((parent_points[:,1:]-parent_points[:,:1]).transpose(2,0,1))
    assignment=[];volumes=np.abs(new.mapping().detA)/6
    for child in added:
        vertices=new.p[:,new.t[:,child]].T
        local=np.einsum('pij,pvj->pvi',inverses,vertices[None]-parent_points[:,0].T[:,None])
        bary=np.concatenate((1-local.sum(axis=2,keepdims=True),local),axis=2)
        inside=np.all(bary>=-1e-11,axis=(1,2))&np.all(bary<=1+1e-11,axis=(1,2))
        parents=np.flatnonzero(inside)
        if len(parents)!=1:raise ValueError('replacement not contained in one original parent')
        assignment.append(int(parents[0]))
    total=np.bincount(assignment,weights=volumes[added],minlength=len(removed))
    np.testing.assert_allclose(total,np.abs(old.mapping().detA)[removed]/6,rtol=1e-10,atol=1e-13)
    return float(np.max(np.abs(total-np.abs(old.mapping().detA)[removed]/6)))


def optimize_edges(old,initial,passes=4):
    if not isinstance(passes,int) or not 1<=passes<=4:raise ValueError('unsupported optimization bound')
    removed,added=changed_macros(old,initial);groups=edge_groups(old,initial)
    points=initial.p.copy();fractions=[.5]*len(groups);history=[]
    def objective(p):
        values=alfeld_shape(p,initial.t[:,added]);return (float(values.max()),float(values.mean()))
    initial_objective=objective(points);evaluations=1
    for cycle in range(passes):
        changed=False
        for index,group in enumerate(groups):
            if group['fixed_midpoint']:continue
            best=objective(points);evaluations+=1;selected=fractions[index];candidate_points=points.copy()
            for fraction in FRACTIONS:
                trial=points.copy();apply_fraction(trial,old.p,group,fraction)
                value=objective(trial);evaluations+=1
                better=value[0]<best[0]-1e-12 or (abs(value[0]-best[0])<=1e-12 and value[1]<best[1]-1e-12)
                if better:best=value;selected=fraction;candidate_points=trial
            if selected!=fractions[index]:changed=True
            points=candidate_points;fractions[index]=selected
        history.append(dict(pass_index=cycle+1,objective=list(objective(points)),fractions=fractions.copy()))
        evaluations+=1
        if not changed:break
    result=MeshTet(points,initial.t.copy())
    np.testing.assert_array_equal(result.p[:,:old.nvertices],old.p)
    partition_error=partition_check(old,result,removed,added)
    # Face incidence and boundary geometry survive the shared original-edge move.
    np.testing.assert_array_equal(result.facets,initial.facets)
    np.testing.assert_array_equal(result.f2t,initial.f2t)
    distance,_=cKDTree(result.p.T).query(result.p.T[:,[0,2,1]])
    if np.max(distance)>1e-12:raise ValueError('relocation broke Y/Z point symmetry')
    return result,dict(groups=groups,fractions=fractions,history=history,passes=len(history),evaluations=evaluations,initial_objective=list(initial_objective),optimized_objective=list(objective(points)),maximum_parent_volume_partition_error_m3=partition_error),removed,added


def force_transition_mesh(diagnostic_path,rank=0):
    if not isinstance(rank,int) or not 0<=rank<8:raise ValueError('unsupported paired rank')
    old,octant,lo,hi,axes,lengths,scores,pairs,binding=force_ranked_input(diagnostic_path)
    selected=np.array(pairs[rank]);initial=octant.refined(selected)
    refined,optimization,removed,added=optimize_edges(octant,initial)
    old_shape=alfeld_shape(octant.p,octant.t[:,removed]);new_shape=alfeld_shape(refined.p,refined.t[:,added])
    macro=mirror(refined,lengths);mesh=alfeld_split(macro)
    assert mesh.nelements<=50000 and np.min(np.abs(mesh.mapping().detA))>0
    assert abs(np.abs(mesh.mapping().detA).sum()/6-31.)<1e-9
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    assert np.all(np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)|body(mid)), 'false interior wall'
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
    meta=dict(**binding,rank=rank,optimization=optimization,selected_octant_macro_indices=selected.tolist(),selected_octant_centers_m=octant.p[:,octant.t[:,selected]].mean(axis=1).T.tolist(),
        captured_score_fraction=float(scores[selected].sum()/scores.sum()),original_tetrahedra=old.nelements,refined_tetrahedra=mesh.nelements,
        affected_original_macros_per_octant=len(removed),affected_refined_macros_per_octant=len(added),original_affected_worst_shape=float(old_shape.max()),refined_affected_worst_shape=float(new_shape.max()),
        original_affected_mean_shape=float(old_shape.mean()),refined_affected_mean_shape=float(new_shape.mean()),original_global_max_condition=float(np.linalg.cond(old.mapping().A.transpose(2,0,1)).max()),refined_global_max_condition=float(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max()),
        boundary_areas_m2=areas,original_vertices_preserved=True,shared_original_edge_constraints_preserved=True,scope='constrained transition geometry optimization; unchanged PDE forms; geometry admission is not a force certificate')
    reasons=[]
    if meta['refined_affected_worst_shape']>meta['original_affected_worst_shape']*(1+1e-8):reasons.append('affected intrinsic worst shape worsened')
    if meta['refined_affected_mean_shape']>=meta['original_affected_mean_shape']*(1-1e-8):reasons.append('affected intrinsic mean shape did not improve')
    if meta['refined_global_max_condition']>meta['original_global_max_condition']*(1+1e-8):reasons.append('global Jacobian conditioning worsened')
    if reasons:raise LocalGeometryRejected(meta,reasons)
    return (mesh,lo,hi,axes,macro.nelements),meta
