"""Cut the closest streamwise slab inside each original macro tetrahedron."""
import numpy as np
from skfem import MeshTet
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_adaptive_mesh import mirror
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_mesh import edge_distance
from cfd_reference3d_corner_local_mesh import shape_inverse,LocalGeometryRejected


def cut_macro_plane(old,plane):
    """Pulling triangulation of clipped tetrahedra; shared polygon faces agree."""
    points=[p.copy() for p in old.p.T];intersections={};cells=[];parents=[]
    def crossing(a,b):
        key=tuple(sorted((int(a),int(b))))
        if key not in intersections:
            a,b=key;t=(plane-old.p[0,a])/(old.p[0,b]-old.p[0,a])
            assert 0<t<1
            p=(1-t)*old.p[:,a]+t*old.p[:,b];p[0]=plane
            intersections[key]=len(points);points.append(p)
        return intersections[key]
    def clip(face,side):
        result=[]
        for a,b in zip(face,np.roll(face,-1)):
            ia=side*(old.p[0,a]-plane)>=0;ib=side*(old.p[0,b]-plane)>=0
            if ia:result.append(int(a))
            if ia!=ib:result.append(crossing(a,b))
        return result
    for parent,tet in enumerate(old.t.T):
        x=old.p[0,tet]-plane
        if np.all(x<0) or np.all(x>0):
            cells.append(tet.tolist());parents.append(parent);continue
        assert not np.any(x==0), 'cut must be strictly inside an existing slab'
        cut=[crossing(tet[i],tet[j]) for i in range(4) for j in range(i+1,4) if x[i]*x[j]<0]
        yz=np.array([points[i][1:] for i in cut]);c=yz.mean(axis=0)
        cut=[cut[i] for i in np.argsort(np.arctan2(yz[:,1]-c[1],yz[:,0]-c[0]))]
        for side in (-1,1):
            faces=[clip(np.delete(tet,j),side) for j in range(4)];faces=[f for f in faces if len(f)>=3]+[cut]
            anchor=min(i for face in faces for i in face)
            for face in faces:
                if anchor in face:continue
                k=face.index(min(face));polygon=face[k:]+face[:k]
                for j in range(1,len(polygon)-1):
                    cells.append([anchor,polygon[0],polygon[j],polygon[j+1]]);parents.append(parent)
    mesh=MeshTet(np.array(points).T,np.array(cells,dtype=np.int32).T)
    parents=np.array(parents,dtype=np.int32)
    np.testing.assert_array_equal(mesh.p[:,:old.nvertices],old.p)
    volumes=np.abs(mesh.mapping().detA)/6;oldvol=np.abs(old.mapping().detA)/6
    errors=np.abs(np.bincount(parents,weights=volumes,minlength=old.nelements)-oldvol)
    np.testing.assert_allclose(np.bincount(parents,weights=volumes,minlength=old.nelements),oldvol,rtol=1e-10,atol=1e-14)
    # Check every vertex against the original tetrahedron, not just total volume.
    for parent in np.flatnonzero(np.bincount(parents)>1):
        ids=np.unique(mesh.t[:,parents==parent]);v=old.p[:,old.t[:,parent]]
        b=np.linalg.solve(v[:,1:]-v[:,0,None],mesh.p[:,ids]-v[:,0,None]);b=np.vstack((1-b.sum(axis=0),b))
        assert b.min()>-1e-10 and b.max()<1+1e-10
    assert volumes.min()>0
    return mesh,parents,dict(maximum_parent_volume_error_m3=float(errors.max()),cut_parent_count=int((np.bincount(parents)>1).sum()),original_macros=old.nelements,refined_macros=mesh.nelements,new_edge_vertices=len(intersections),all_original_points_bitwise_preserved=True,each_child_inside_original_parent=True)


def facet_keys(mesh,indices):
    return {tuple(sorted(map(tuple,np.round(mesh.p[:,mesh.facets[:,i]].T,12)))) for i in indices}


def second_normal_mesh(length=4.):
    if length not in (4.,8.):raise ValueError('unsupported second-normal domain')
    outer=1 if length==4. else 2
    old,lo,hi,axes,_=domain_mesh(length,6,True,3,outer,'held_l4')
    lengths=np.array([length,2.,2.]);octant=MeshTet.init_tensor(*[a[a<=lengths[i]/2+1e-12] for i,a in enumerate(axes)])
    centers=octant.p[:,octant.t].mean(axis=1)
    octant=octant.remove_elements(np.flatnonzero(np.all((centers>lo[:,None])&(centers<hi[:,None]),axis=0)))
    closest=axes[0][axes[0]<lo[0]-1e-12][-1];plane=(closest+lo[0])/2
    refined,parents,partition=cut_macro_plane(octant,plane)
    macro=mirror(refined,lengths);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    mid=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    assert np.all(np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)|body(mid)), 'false internal wall'
    assert facet_keys(old,old.boundaries['body'])==facet_keys(mesh,mesh.boundaries['body']), 'cube triangles changed'
    assert abs(np.abs(mesh.mapping().detA).sum()/6-(4*length-1))<1e-9
    areas={}
    for name,facets in mesh.boundaries.items():
        v=mesh.p[:,mesh.facets[:,facets]];areas[name]=float(np.linalg.norm(np.cross((v[:,1]-v[:,0]).T,(v[:,2]-v[:,0]).T),axis=1).sum()/2)
        assert abs(areas[name]-dict(body=6.,inlet=4.,outlet=4.,walls=8*length)[name])<1e-9
    for axis,L in enumerate(lengths):
        p=mesh.p.copy();p[axis]=L-p[axis]
        reflected=MeshTet(p,mesh.t.copy())
        def cell_keys(m):return {tuple(sorted(map(tuple,np.round(m.p[:,v].T,11)))) for v in m.t.T}
        assert cell_keys(mesh)==cell_keys(reflected), 'tetrahedral reflection symmetry lost'
    old_shape,new_shape=shape_inverse(old),shape_inverse(mesh)
    meta=dict(family='original_macro_streamwise_plane_cut',length=length,count=6,outer_layers=outer,normal_distance_m=float(lo[0]-plane),previous_normal_distance_m=float(lo[0]-closest),partition=partition,original_tetrahedra=old.nelements,refined_tetrahedra=mesh.nelements,body_surface_triangles_preserved=True,boundary_areas_m2=areas,
        original_global_worst_shape=float(old_shape.max()),refined_global_worst_shape=float(new_shape.max()),original_global_max_condition=float(np.linalg.cond(old.mapping().A.transpose(2,0,1)).max()),refined_global_max_condition=float(np.linalg.cond(mesh.mapping().A.transpose(2,0,1)).max()),centroid_edge_regions=[])
    for radius in (.025,.05,.1):
        a=edge_distance(old.p[:,old.t].mean(axis=1),lo,hi)<radius;b=edge_distance(mesh.p[:,mesh.t].mean(axis=1),lo,hi)<radius
        meta['centroid_edge_regions'].append(dict(radius_m=radius,original_cells=int(a.sum()),refined_cells=int(b.sum()),original_worst=float(old_shape[a].max()),refined_worst=float(new_shape[b].max()),original_mean=float(old_shape[a].mean()),refined_mean=float(new_shape[b].mean())))
    reasons=[]
    if mesh.nelements>50000:reasons.append('mesh cap')
    if meta['refined_global_worst_shape']>meta['original_global_worst_shape']*(1+1e-8):reasons.append('global intrinsic worst shape worsened')
    if meta['refined_global_max_condition']>meta['original_global_max_condition']*(1+1e-8):reasons.append('global Jacobian conditioning worsened')
    if reasons:raise LocalGeometryRejected(meta,reasons)
    changed=[np.unique(np.r_[axes[0],plane,length-plane]),axes[1],axes[2]]
    return (mesh,lo,hi,changed,macro.nelements),meta
