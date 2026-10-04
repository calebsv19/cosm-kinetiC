"""Topology-preserving front/back normal grading of the existing Alfeld cube mesh."""
import numpy as np
from skfem import MeshTet
from cfd_fem_reference3d_solenoidal import mesh_for_case
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split


def spatial_mesh(length=4.,body=True,count=2,split=False,n=4,normal_spacing=None,insert_normal=False,edge_passes=0,edge_radius=.12):
    mesh,lo,hi,axes,macro_count=mesh_for_case(length,body,count,False if insert_normal else split,n)
    if edge_passes not in (0,1) or not np.isfinite(edge_radius) or not 0<edge_radius<.5:
        raise ValueError('unsupported edge refinement')
    if normal_spacing is None and not edge_passes:return mesh,lo,hi,axes,macro_count
    if not body:raise ValueError('edge/normal refinement requires body')
    if normal_spacing is not None and (not np.isfinite(normal_spacing) or not 0<normal_spacing<lo[0]/8):
        raise ValueError('unsupported normal spacing')
    if edge_passes and normal_spacing is not None and not insert_normal:
        raise ValueError('edge refinement with normal grading requires inserted planes')
    if insert_normal or edge_passes:
        if normal_spacing is not None:
            distances=[normal_spacing/2,normal_spacing] if split else [normal_spacing]
            added=np.r_[lo[0]-np.array(distances),hi[0]+np.array(distances)]
            axes=[np.unique(np.r_[axes[0],added]),*axes[1:]]
        macro,_=refined_octant(axes,np.array([length,2.,2.]),lo,hi,edge_passes,radii=[edge_radius])
        mesh=alfeld_split(macro)
        if mesh.nelements>50000:raise ValueError(('reference_mesh_cap',mesh.nelements))
        def surface(x):
            return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
        mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),
            'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':surface})
        return mesh,lo,hi,axes,macro.nelements
    original=axes[0];changed=original.copy()
    front=np.flatnonzero(original<lo[0]-1e-12)
    back=np.flatnonzero(original>hi[0]+1e-12)
    if split:
        changed[front[-2:]]=[lo[0]-normal_spacing,lo[0]-normal_spacing/2]
        changed[back[:2]]=[hi[0]+normal_spacing/2,hi[0]+normal_spacing]
    else:
        changed[front[-1]]=lo[0]-normal_spacing
        changed[back[0]]=hi[0]+normal_spacing
    assert np.all(np.diff(changed)>0)
    coordinates=mesh.p.copy();coordinates[0]=np.interp(coordinates[0],original,changed)
    warped=MeshTet(coordinates,mesh.t.copy(),_boundaries=mesh.boundaries)
    assert np.min(np.abs(warped.mapping().detA))>0
    axes=[changed,*axes[1:]]
    return warped,lo,hi,axes,macro_count
