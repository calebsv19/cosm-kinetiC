"""Distinct six-interval edge-strip redistribution with prospective quality gates."""
import numpy as np
from scipy.spatial import cKDTree
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_mesh import refined_octant,edge_distance
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_corner_local_mesh import shape_inverse
SPACINGS=(.0625,.06,.055)

def body_nodes(spacing):
    if spacing not in SPACINGS:raise ValueError('undeclared edge strip')
    return np.array([0.,spacing,.25,.5,.75,1-spacing,1.])

def build(length,spacing):
    if length not in (4.,8.):raise ValueError('undeclared domain length')
    old,lo,hi,axes,n=second_normal_tensor_mesh(length)[0];nodes=body_nodes(spacing);L=np.array([length,2.,2.])
    changed=[]
    for i,a in enumerate(axes):
        b=a.copy();mask=(a>=lo[i]-1e-12)&(a<=hi[i]+1e-12);assert mask.sum()==7;b[mask]=lo[i]+nodes
        assert np.all(np.diff(b)>0);np.testing.assert_array_equal(a[~mask],b[~mask]);changed.append(b)
    macro,_=refined_octant(changed,L,lo,hi,0);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert mesh.nelements==old.nelements and macro.nelements==n
    return mesh,lo,hi,changed,n

def symmetry(mesh,length):
    tree=cKDTree(mesh.p.T);keys={tuple(sorted(t)) for t in mesh.t.T}
    for axis in range(4):
        p=mesh.p.copy()
        if axis<3:p[axis]=(length,2.,2.)[axis]-p[axis]
        else:p=p[[0,2,1]]
        distance,ids=tree.query(p.T)
        if distance.max()>1e-11 or keys!={tuple(sorted(t)) for t in ids[mesh.t].T}:return False
    return True

def quality(mesh,lo,hi,length):
    det=np.abs(mesh.mapping().detA);assert det.min()>0;v=det/6;shape=shape_inverse(mesh);cond=np.linalg.cond(mesh.mapping().A.transpose(2,0,1));centers=mesh.p[:,mesh.t].mean(axis=1)
    def stats(mask):
        assert np.any(mask)
        return dict(cells=int(mask.sum()),worst_shape=float(shape[mask].max()),weighted_mean_shape=float(np.dot(v[mask],shape[mask])/v[mask].sum()),max_condition=float(cond[mask].max()),volume_m3=float(v[mask].sum()))
    outside=np.maximum(np.maximum(lo[:,None]-centers,centers-hi[:,None]),0);body_distance=np.linalg.norm(outside,axis=0)
    bands={f'edge-{r}':stats(edge_distance(centers,lo,hi)<r) for r in (.025,.05,.1)}
    bands.update({f'body-{r}':stats(body_distance<r) for r in (.05,.1,.2)})
    areas={}
    for name,facets in mesh.boundaries.items():
        p=mesh.p[:,mesh.facets[:,facets]];areas[name]=float(np.linalg.norm(np.cross((p[:,1]-p[:,0]).T,(p[:,2]-p[:,0]).T),axis=1).sum()/2)
    f=mesh.p[:,mesh.facets[:,mesh.boundaries['body']]]
    surface_edge=max(float(np.linalg.norm(f[:,i]-f[:,j],axis=0).max()) for i in range(3) for j in range(i))
    boundary_centers=mesh.p[:,mesh.facets[:,mesh.boundary_facets()]].mean(axis=1)
    exterior=np.any(np.isclose(boundary_centers,0)|np.isclose(boundary_centers,np.array([length,2.,2.])[:,None]),axis=0)
    body=np.all((boundary_centers>=lo[:,None]-1e-10)&(boundary_centers<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(boundary_centers,lo[:,None])|np.isclose(boundary_centers,hi[:,None]),axis=0)
    return dict(global_quality=stats(np.ones(mesh.nelements,dtype=bool)),bands=bands,boundary_areas_m2=areas,physical_boundary_planes_preserved=bool(np.all(exterior|body)),all_reflections_and_yz_exchange_preserved=symmetry(mesh,length),maximum_body_triangle_edge_m=surface_edge,body_triangles=len(mesh.boundaries['body']),tetrahedra=mesh.nelements)

def evaluate(old,new,length,spacing):
    reasons=[]
    if new['tetrahedra']!=old['tetrahedra'] or new['tetrahedra']>50000:reasons.append('mesh count/cap')
    if new['body_triangles']!=old['body_triangles']:reasons.append('body surface triangle count changed')
    if not new['all_reflections_and_yz_exchange_preserved']:reasons.append('symmetry')
    if not new['physical_boundary_planes_preserved']:reasons.append('false boundary plane')
    if abs(new['global_quality']['volume_m3']-(4*length-1))>1e-9:reasons.append('fluid volume')
    if any(abs(new['boundary_areas_m2'][k]-v)>1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items()):reasons.append('boundary area')
    for name,a,b in [('global',old['global_quality'],new['global_quality'])]+[(k,v,new['bands'][k]) for k,v in old['bands'].items()]:
        for k in ('worst_shape','weighted_mean_shape','max_condition'):
            if b[k]>a[k]*(1+1e-8):reasons.append(name+' '+k+' worsened')
    if new['bands']['edge-0.05']['weighted_mean_shape']>old['bands']['edge-0.05']['weighted_mean_shape']*.99:reasons.append('edge.05 weighted shape improvement below1%')
    original_first=.5*(1-np.cos(np.pi/6))
    if spacing>original_first*.95:reasons.append('edge strip improvement below5%')
    if abs(new['maximum_body_triangle_edge_m']-old['maximum_body_triangle_edge_m'])>1e-10:reasons.append('unexpected maximum surface edge change')
    return reasons
