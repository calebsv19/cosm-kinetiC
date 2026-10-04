"""Symmetric affine macro-cell spacing controls, preserving cube topology/boundaries."""
import numpy as np
from skfem import MeshTet
from cfd_fem_reference3d_solenoidal import mesh_for_case
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split


def graded_mesh(length=4., count=2, split=False, exponent=1, outer_layers=1):
    if exponent not in (1,2,3) or length not in (4.,8.) or count not in (2,4,6):
        raise ValueError('unsupported bounded grading')
    if outer_layers not in (1,2,3) or (outer_layers!=1 and exponent!=3):
        raise ValueError('outer subdivision requires preserved original grading')
    old,lo,hi,axes,macros=mesh_for_case(length,True,count,split)
    if outer_layers!=1:
        lengths=np.array([length,2.,2.])
        added=np.linspace(axes[0][0],axes[0][1],outer_layers+1)[1:-1]
        axes=[np.unique(np.r_[axes[0],added,length-added]),*axes[1:]]
        macro,_=refined_octant(axes,lengths,lo,hi,0)
        mesh=alfeld_split(macro)
        assert mesh.nelements<=50000
        def surface(x):
            return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
        mesh=mesh.with_boundaries({'inlet':lambda x:np.isclose(x[0],0),'outlet':lambda x:np.isclose(x[0],length),
            'walls':lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2),'body':surface})
        assert abs(np.sum(np.abs(mesh.mapping().detA))/6-(4*length-1))<1e-9
        return mesh,lo,hi,axes,macro.nelements
    if exponent==3:return old,lo,hi,axes,macros
    lengths=np.array([length,2.,2.]);changed=[]
    for axis,L in enumerate(lengths):
        interior=axes[axis][(axes[axis]>=lo[axis])&(axes[axis]<=hi[axis])]
        left=lo[axis]-lo[axis]*np.linspace(1,0,3)**exponent
        right=hi[axis]+(L-hi[axis])*np.linspace(0,1,3)**exponent
        if split and axis==0:
            left=np.sort(np.r_[left,lo[axis]-(lo[axis]-left[-2])/2])
            right=np.sort(np.r_[right,hi[axis]+(right[1]-hi[axis])/2])
        nodes=np.r_[left[:-1],interior,right[1:]]
        assert len(nodes)==len(axes[axis]) and np.all(np.diff(nodes)>0)
        changed.append(nodes)
    vertices=old.p.copy()
    for axis in range(3):vertices[axis]=np.interp(vertices[axis],axes[axis],changed[axis])
    mesh=MeshTet(vertices,old.t.copy(),_boundaries=old.boundaries)
    assert np.min(np.abs(mesh.mapping().detA))>0
    assert abs(np.sum(np.abs(mesh.mapping().detA))/6-(4*length-1))<1e-9
    # The coordinate map is affine on each original macro, including its center.
    for center in np.unique(mesh.t.max(axis=0)):
        cells=mesh.t[:,np.any(mesh.t==center,axis=0)];corners=np.unique(cells);corners=corners[corners!=center]
        assert len(corners)==4
        np.testing.assert_allclose(mesh.p[:,center],mesh.p[:,corners].mean(axis=1),atol=1e-12)
    return mesh,lo,hi,changed,macros
