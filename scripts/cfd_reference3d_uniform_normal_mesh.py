"""Uniform body spacing with balanced normal distance; original mixed FE split."""
import numpy as np
from cfd_fem_reference3d_solenoidal import mesh_for_case
from cfd_reference3d_mesh import refined_octant,edge_distance
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_corner_local_mesh import shape_inverse,LocalGeometryRejected


def uniform_normal_mesh(length=4.,count=6,normal_factor=1.):
    if length!=4. or count not in (6,8) or normal_factor!=1.:raise ValueError('unsupported bounded normal mesh')
    old,lo,hi,axes,_=mesh_for_case(length,True,count,False)
    h=1/16;changed=[];lengths=np.array([length,2.,2.])
    for axis,L in enumerate(lengths):
        nodes=np.r_[axes[axis][axes[axis]<lo[axis]],np.linspace(lo[axis],hi[axis],count+1),axes[axis][axes[axis]>hi[axis]]];left=np.flatnonzero(nodes<lo[axis]-1e-12)[-1];right=np.flatnonzero(nodes>hi[axis]+1e-12)[0]
        nodes[left]=lo[axis]-h;nodes[right]=hi[axis]+h
        if axis==0:nodes=np.unique(np.r_[nodes,nodes[1]/2,L-nodes[1]/2])
        assert np.all(np.diff(nodes)>0);changed.append(nodes)
    macro,_=refined_octant(changed,lengths,lo,hi,0);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),
        'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert mesh.nelements<=50000 and np.min(np.abs(mesh.mapping().detA))>0
    assert abs(np.abs(mesh.mapping().detA).sum()/6-15.)<1e-9
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    assert np.all(np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)|body(mid)), 'false wall'
    areas={}
    for name,facets in mesh.boundaries.items():
        v=mesh.p[:,mesh.facets[:,facets]]
        areas[name]=float(np.linalg.norm(np.cross((v[:,1]-v[:,0]).T,(v[:,2]-v[:,0]).T),axis=1).sum()/2)
        assert abs(areas[name]-{'body':6.,'inlet':4.,'outlet':4.,'walls':32.}[name])<1e-9
    for axis,L in enumerate(lengths):
        reflected=mesh.p.copy();reflected[axis]=L-reflected[axis]
        assert set(map(tuple,np.round(mesh.p.T,11)))==set(map(tuple,np.round(reflected.T,11)))
        np.testing.assert_allclose(changed[axis][(changed[axis]>=lo[axis])&(changed[axis]<=hi[axis])],np.linspace(lo[axis],hi[axis],count+1),rtol=0,atol=1e-14)
    old_near=edge_distance(old.p[:,old.t].mean(axis=1),lo,hi)<.025
    new_near=edge_distance(mesh.p[:,mesh.t].mean(axis=1),lo,hi)<.025
    assert np.any(old_near) and np.any(new_near)
    old_shape,new_shape=shape_inverse(old),shape_inverse(mesh)
    meta=dict(family='uniform_body_balanced_normal_tensor',length=length,count=count,normal_factor=normal_factor,common_normal_distance_m=h,
        original_tetrahedra=old.nelements,refined_tetrahedra=mesh.nelements,original_near_edge_cells=int(old_near.sum()),refined_near_edge_cells=int(new_near.sum()),
        original_near_worst_shape=float(old_shape[old_near].max()),refined_near_worst_shape=float(new_shape[new_near].max()),original_near_mean_shape=float(old_shape[old_near].mean()),refined_near_mean_shape=float(new_shape[new_near].mean()),
        original_global_worst_shape=float(old_shape.max()),refined_global_worst_shape=float(new_shape.max()),original_global_max_condition=float(np.linalg.cond(old.mapping().A.transpose(2,0,1)).max()),refined_global_max_condition=float(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max()),boundary_areas_m2=areas,
        physical_body_and_domain_preserved=True,body_planes_preserved=False,uniform_body_interval_m=1/count,original_first_cosine_interval_m=float(axes[0][np.flatnonzero(np.isclose(axes[0],lo[0]))[0]+1]-lo[0]),scope='prospective geometry and centroid-region measures, not force convergence or a same-parent error bound')
    reasons=[]
    if meta['refined_global_worst_shape']>meta['original_global_worst_shape']*(1+1e-8):reasons.append('global intrinsic worst shape worsened')
    if meta['refined_global_max_condition']>meta['original_global_max_condition']*(1+1e-8):reasons.append('global Jacobian conditioning worsened')
    if meta['refined_near_worst_shape']>=meta['original_near_worst_shape']*(1-1e-8):reasons.append('near-edge intrinsic worst shape did not improve')
    if meta['refined_near_mean_shape']>=meta['original_near_mean_shape']*(1-1e-8):reasons.append('near-edge intrinsic mean shape did not improve')
    if reasons:raise LocalGeometryRejected(meta,reasons)
    return (mesh,lo,hi,changed,macro.nelements),meta
