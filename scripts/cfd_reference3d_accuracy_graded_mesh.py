"""One graded side-normal profile retaining cube surface and physical geometry."""
import numpy as np
from cfd_reference3d_accuracy_side_layer_survey import build as side_build
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_second_normal_mesh import facet_keys
from cfd_reference3d_edge_redistribution_mesh import quality

def build(length):
    if length not in (4.,8.):raise ValueError('unsupported graded domain')
    side,original=side_build(length,'redistribute');_,lo,hi,axes,_=side
    changed=[]
    for a in axes:
        additions=[(left+right)/2 for left,right in zip(a[:-1],a[1:]) if right-left>.4]
        changed.append(np.sort(np.r_[a,additions]))
    macro,_=refined_octant(changed,np.array([length,2.,2.]),lo,hi,0);m=alfeld_split(macro)
    def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
    m=m.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
    assert m.nelements==(81792 if length==4. else 100608) and facet_keys(m,m.boundaries['body'])==facet_keys(original[0],original[0].boundaries['body'])
    return (m,lo,hi,changed,macro.nelements),original

def check(bundle,original,length):
    m,lo,hi,axes,_=bundle;old=quality(original[0],lo,hi,length);new=quality(m,lo,hi,length);reasons=[]
    if new['tetrahedra']>120000:reasons.append('declared mesh cap')
    if not new['all_reflections_and_yz_exchange_preserved']:reasons.append('actual cell symmetry')
    if not new['physical_boundary_planes_preserved']:reasons.append('physical boundary plane')
    if abs(new['global_quality']['volume_m3']-(4*length-1))>1e-9:reasons.append('fluid volume')
    for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items():
        if abs(new['boundary_areas_m2'][k]-v)>1e-9:reasons.append('area '+k)
    for k in ('worst_shape','max_condition'):
        if new['global_quality'][k]>old['global_quality'][k]*(1+1e-8):reasons.append('global '+k+' worsened')
    return old,new,reasons

def translated_inner_keys(bundle):
    m,lo,hi,_,_=bundle;coordinates=m.p[:,m.t];mask=np.all((coordinates[0]>=lo[0]-.1875-1e-12)&(coordinates[0]<=hi[0]+.1875+1e-12),axis=0)
    points=np.round((coordinates[:, :,mask]-np.array([lo[0],0.,0.])[:,None,None]).transpose(2,1,0),12)
    return {tuple(sorted(tuple(v) for v in t)) for t in points}
