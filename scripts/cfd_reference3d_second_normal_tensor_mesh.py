"""Non-nested tensor remesh of the two normal bricks; cube triangles held."""
import numpy as np
from scipy.spatial import cKDTree
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_mesh import refined_octant,edge_distance
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_second_normal_mesh import facet_keys
from cfd_reference3d_corner_local_mesh import shape_inverse,LocalGeometryRejected


def second_normal_tensor_mesh(length=4.):
    if length not in (4.,8.):raise ValueError('unsupported tensor normal domain')
    outer=1 if length==4. else 2
    old,lo,hi,axes,_=domain_mesh(length,6,True,3,outer,'held_l4');L=np.array([length,2.,2.])
    closest=axes[0][axes[0]<lo[0]-1e-12][-1];plane=(closest+lo[0])/2
    changed=[np.unique(np.r_[axes[0],plane,length-plane]),axes[1],axes[2]]
    macro,_=refined_octant(changed,L,lo,hi,0);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert facet_keys(mesh,mesh.boundaries['body'])==facet_keys(old,old.boundaries['body'])
    assert abs(np.abs(mesh.mapping().detA).sum()/6-(4*length-1))<1e-9 and np.abs(mesh.mapping().detA).min()>0
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    assert np.all(np.any(np.isclose(mid,0)|np.isclose(mid,L[:,None]),axis=0)|body(mid))
    areas={}
    for name,facets in mesh.boundaries.items():
        v=mesh.p[:,mesh.facets[:,facets]];areas[name]=float(np.linalg.norm(np.cross((v[:,1]-v[:,0]).T,(v[:,2]-v[:,0]).T),axis=1).sum()/2)
        assert abs(areas[name]-dict(body=6.,inlet=4.,outlet=4.,walls=8*length)[name])<1e-9
    for a,b in zip(axes,changed):assert set(a).issubset(set(b))
    for axis,d in enumerate(L):
        p=mesh.p.copy();p[axis]=d-p[axis];distance,ids=cKDTree(mesh.p.T).query(p.T)
        assert distance.max()<1e-11
        oldkeys={tuple(sorted(v)) for v in mesh.t.T};newkeys={tuple(sorted(v)) for v in ids[mesh.t].T};assert oldkeys==newkeys
    old_shape,new_shape=shape_inverse(old),shape_inverse(mesh)
    meta=dict(family='non_nested_same_surface_tensor_normal',length=length,count=6,outer_layers=outer,normal_distance_m=float(lo[0]-plane),previous_normal_distance_m=float(lo[0]-closest),original_tetrahedra=old.nelements,refined_tetrahedra=mesh.nelements,body_surface_triangles_preserved=True,original_tetrahedron_partition_claimed=False,body_and_physical_domain_preserved=True,boundary_areas_m2=areas,
        original_global_worst_shape=float(old_shape.max()),refined_global_worst_shape=float(new_shape.max()),original_global_max_condition=float(np.linalg.cond(old.mapping().A.transpose(2,0,1)).max()),refined_global_max_condition=float(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max()),centroid_edge_regions=[])
    for radius in (.025,.05,.1):
        a=edge_distance(old.p[:,old.t].mean(axis=1),lo,hi)<radius;b=edge_distance(mesh.p[:,mesh.t].mean(axis=1),lo,hi)<radius
        meta['centroid_edge_regions'].append(dict(radius_m=radius,original_cells=int(a.sum()),refined_cells=int(b.sum()),original_worst=float(old_shape[a].max()),refined_worst=float(new_shape[b].max()),original_mean=float(old_shape[a].mean()),refined_mean=float(new_shape[b].mean())))
    reasons=[]
    if mesh.nelements>50000:reasons.append('mesh cap')
    if meta['refined_global_worst_shape']>meta['original_global_worst_shape']*(1+1e-8):reasons.append('global intrinsic worst shape worsened')
    if meta['refined_global_max_condition']>meta['original_global_max_condition']*(1+1e-8):reasons.append('global Jacobian conditioning worsened')
    if reasons:raise LocalGeometryRejected(meta,reasons)
    return (mesh,lo,hi,changed,macro.nelements),meta
