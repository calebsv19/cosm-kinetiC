"""Balanced piecewise eight-interval cube plus short outer streamwise slabs."""
import numpy as np
from cfd_reference3d_edge_redistribution_mesh import quality,symmetry
from cfd_reference3d_second_normal_tensor_mesh import second_normal_tensor_mesh
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split
SPACINGS=(.05,.055)
def body_nodes(spacing):
    if spacing not in SPACINGS:raise ValueError('undeclared body spacing')
    return np.array([0.,spacing,.15,.30,.50,.70,.85,1-spacing,1.])
def build(length,spacing):
    if length not in (4.,8.):raise ValueError('undeclared domain length')
    old,lo,hi,axes,n=second_normal_tensor_mesh(length)[0];L=np.array([length,2.,2.]);nodes=body_nodes(spacing)
    left=axes[0][axes[0]<lo[0]-1e-12];assert abs(lo[0]-left[-1]-.046875)<1e-12 and abs(lo[0]-left[-2]-.09375)<1e-12 and abs(lo[0]-left[-3]-.1875)<1e-12
    slabs=2 if length==4. else 3;xleft=np.r_[np.linspace(0.,left[-3],slabs+1),left[-1]]
    outside=[xleft]+[a[a<lo[i]-1e-12] for i,a in enumerate(axes[1:],1)]
    changed=[np.r_[a,lo[i]+nodes,L[i]-a[::-1]] for i,a in enumerate(outside)]
    for i,a in enumerate(changed):assert np.all(np.diff(a)>0) and np.max(abs(a+a[::-1]-L[i]))<1e-12
    macro,_=refined_octant(changed,L,lo,hi,0);mesh=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert mesh.nelements==(43008 if length==4. else 49920)
    return mesh,lo,hi,changed,macro.nelements

def evaluate(old,new,length,spacing):
    reasons=[]
    if new['tetrahedra']>50000:reasons.append('mesh cap')
    if not new['all_reflections_and_yz_exchange_preserved']:reasons.append('symmetry')
    if not new['physical_boundary_planes_preserved']:reasons.append('false boundary plane')
    if abs(new['global_quality']['volume_m3']-(4*length-1))>1e-9:reasons.append('fluid volume')
    if any(abs(new['boundary_areas_m2'][k]-v)>1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items()):reasons.append('boundary area')
    for name,a,b in [('global',old['global_quality'],new['global_quality'])]+[(k,v,new['bands'][k]) for k,v in old['bands'].items()]:
        for k in ('worst_shape','weighted_mean_shape','max_condition'):
            if b[k]>a[k]*(1+1e-8):reasons.append(name+' '+k+' worsened')
    if new['bands']['edge-0.05']['weighted_mean_shape']>old['bands']['edge-0.05']['weighted_mean_shape']*.99:reasons.append('edge.05 weighted shape improvement below1%')
    if new['maximum_body_triangle_edge_m']>=old['maximum_body_triangle_edge_m']:reasons.append('maximum body surface edge not finer')
    if new['body_triangles']<=old['body_triangles']:reasons.append('body triangle count not increased')
    return reasons
