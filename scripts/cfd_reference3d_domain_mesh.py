"""Conforming domain-length control with optional fixed L4 near-body planes."""
import numpy as np
from skfem import MeshTet
from cfd_reference3d_graded_mesh import graded_mesh
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split


def domain_mesh(length=4.,count=2,split=False,exponent=3,outer_layers=1,mode='original'):
    if mode not in ('original','held_l4'):raise ValueError('unsupported domain mesh mode')
    old,lo,hi,axes,macros=graded_mesh(length,count,split,exponent,1 if mode=='held_l4' and length==8. else outer_layers)
    if mode=='original' or length==4.:return old,lo,hi,axes,macros
    if exponent!=3:raise ValueError('held near-body planes require original outer grading')
    base,_,_,base_axes,_=graded_mesh(4.,count,split,3,1)
    assert len(base_axes[0])==len(axes[0]) and np.array_equal(base.t,old.t)
    changed=[np.r_[0.,base_axes[0][1:-1]+(length-4.)/2,length],axes[1],axes[2]]
    assert np.all(np.diff(changed[0])>0)
    if outer_layers!=1:
        if outer_layers not in (2,3):raise ValueError('unsupported outer subdivision')
        added=np.linspace(0.,changed[0][1],outer_layers+1)[1:-1]
        changed[0]=np.unique(np.r_[changed[0],added,length-added])
        macro,_=refined_octant(changed,np.array([length,2.,2.]),lo,hi,0)
        mesh=alfeld_split(macro);assert mesh.nelements<=50000
        def body(x):return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
        mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),
            'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':body})
        assert abs(np.abs(mesh.mapping().detA).sum()/6-(4*length-1))<1e-9
        return mesh,lo,hi,changed,macro.nelements
    vertices=old.p.copy();vertices[0]=np.interp(vertices[0],axes[0],changed[0])
    mesh=MeshTet(vertices,old.t.copy(),_boundaries=old.boundaries)
    assert np.min(np.abs(mesh.mapping().detA))>0
    assert abs(np.abs(mesh.mapping().detA).sum()/6-(4*length-1))<1e-9
    return mesh,lo,hi,changed,macros
