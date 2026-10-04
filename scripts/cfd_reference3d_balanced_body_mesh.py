"""Prospective fixed-domain tensor body grid; original element equations unchanged."""
import numpy as np
from scipy.spatial import cKDTree
from cfd_reference3d_mesh import refined_octant,edge_distance
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_corner_local_mesh import shape_inverse

PROFILES=('cos8','cos7mid')
NORMALS=('relocate','retain')
LATERALS=(.0625,.046875)

def body_nodes(profile):
    if profile not in PROFILES:raise ValueError('unsupported declared body profile')
    n=8 if profile=='cos8' else 7
    a=.5*(1-np.cos(np.pi*np.linspace(0,1,n+1)))
    if profile=='cos7mid':a=np.sort(np.r_[a,.5])
    else:a[4]=.5
    assert len(a)==9 and np.all(np.diff(a)>0) and np.max(abs(a+a[::-1]-1))<1e-12
    return a

def build(profile,normal,lateral):
    if normal not in NORMALS or lateral not in LATERALS:raise ValueError('unsupported declared normal spacing')
    lengths=np.array([4.,2.,2.]);lo=np.array([1.5,.5,.5]);hi=lo+1;nodes=body_nodes(profile)
    xleft=[0.,.65625,1.3125]
    if normal=='retain':xleft.append(1.40625)
    xleft.append(1.453125)
    lefts=(np.array(xleft),np.array([0.,.5-lateral]),np.array([0.,.5-lateral]))
    axes=[np.r_[left,lo[i]+nodes,lengths[i]-left[::-1]] for i,left in enumerate(lefts)]
    for i,a in enumerate(axes):assert np.all(np.diff(a)>0) and abs(a[len(a)//2]-lengths[i]/2)<1e-12
    macro,_=refined_octant(axes,lengths,lo,hi,0);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],4),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    return mesh,lo,hi,axes,macro.nelements

def symmetry(mesh):
    tree=cKDTree(mesh.p.T);keys={tuple(sorted(t)) for t in mesh.t.T}
    for axis in range(4):
        p=mesh.p.copy()
        if axis<3:p[axis]=(4.,2.,2.)[axis]-p[axis]
        else:p=p[[0,2,1]]
        distance,ids=tree.query(p.T)
        if distance.max()>1e-11 or keys!={tuple(sorted(t)) for t in ids[mesh.t].T}:return False
    return True

def quality(mesh,lo,hi):
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
    exterior=np.any(np.isclose(boundary_centers,0)|np.isclose(boundary_centers,np.array([4.,2.,2.])[:,None]),axis=0)
    body=np.all((boundary_centers>=lo[:,None]-1e-10)&(boundary_centers<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(boundary_centers,lo[:,None])|np.isclose(boundary_centers,hi[:,None]),axis=0)
    return dict(global_quality=stats(np.ones(mesh.nelements,dtype=bool)),bands=bands,boundary_areas_m2=areas,physical_boundary_planes_preserved=bool(np.all(exterior|body)),all_reflections_and_yz_exchange_preserved=symmetry(mesh),maximum_body_triangle_edge_m=surface_edge,body_triangles=len(mesh.boundaries['body']),tetrahedra=mesh.nelements)

def evaluate(old,new):
    reasons=[]
    if new['tetrahedra']>50000:reasons.append('mesh cap')
    if not new['all_reflections_and_yz_exchange_preserved']:reasons.append('symmetry')
    if not new['physical_boundary_planes_preserved']:reasons.append('false boundary plane')
    if abs(new['global_quality']['volume_m3']-15)>1e-9:reasons.append('fluid volume')
    if any(abs(new['boundary_areas_m2'][k]-v)>1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=32.).items()):reasons.append('boundary area')
    for k in ('worst_shape','max_condition'):
        if new['global_quality'][k]>old['global_quality'][k]*(1+1e-8):reasons.append('global '+k+' worsened')
    for name,a in old['bands'].items():
        for k in ('worst_shape','weighted_mean_shape','max_condition'):
            if new['bands'][name][k]>a[k]*(1+1e-8):reasons.append(name+' '+k+' worsened')
    if new['bands']['edge-0.05']['weighted_mean_shape']>old['bands']['edge-0.05']['weighted_mean_shape']*.99:reasons.append('edge.05 weighted shape improvement below1%')
    if new['maximum_body_triangle_edge_m']>=old['maximum_body_triangle_edge_m']:reasons.append('maximum body surface edge not finer')
    if new['body_triangles']<=old['body_triangles']:reasons.append('body triangle count not increased')
    return reasons
