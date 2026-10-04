"""Affine fixed-topology corner-spacing control for the bounded quartic cube."""
import numpy as np
from skfem import MeshTet
from cfd_reference3d_domain_mesh import domain_mesh
from cfd_reference3d_mesh import edge_distance


class CornerGeometryRejected(ValueError):
    def __init__(self,metadata,reasons):
        self.metadata=metadata;self.reasons=reasons
        super().__init__('; '.join(reasons))


def corner_mesh(length=8.,edge=.1):
    if length not in (4.,8.) or edge not in (.1,.08,.0625):raise ValueError('unsupported bounded corner spacing')
    old,lo,hi,axes,macros=domain_mesh(length,4,True,mode='held_l4')
    changed=[]
    for axis,nodes in enumerate(axes):
        a=nodes.copy();inside=np.flatnonzero((nodes>=lo[axis]-1e-12)&(nodes<=hi[axis]+1e-12))
        assert len(inside)==5
        a[inside]=lo[axis]+np.array([0.,edge,.5,1-edge,1.])
        assert np.all(np.diff(a)>0);changed.append(a)
    vertices=old.p.copy()
    for axis in range(3):vertices[axis]=np.interp(vertices[axis],axes[axis],changed[axis])
    mesh=MeshTet(vertices,old.t.copy(),_boundaries=old.boundaries)
    assert mesh.nelements==13824 and np.min(np.abs(mesh.mapping().detA))>0
    assert abs(np.abs(mesh.mapping().detA).sum()/6-(4*length-1))<1e-9
    for parent in range(macros):
        children=mesh.t[:,parent+macros*np.arange(4)];ids=np.unique(children);assert len(ids)==5
        np.testing.assert_allclose(mesh.p[:,ids[-1]],mesh.p[:,ids[:-1]].mean(axis=1),rtol=0,atol=1e-12)
    condition=np.linalg.cond(mesh.mapping().A.transpose(2,0,1));old_condition=np.linalg.cond(old.mapping().A.transpose(2,0,1))
    # Fixed original-parent selection compares the same changed corner cells.
    macro_centers=old.p[:,np.unique(old.t.max(axis=0))]
    selected=edge_distance(macro_centers,lo,hi)<.15
    assert np.any(selected)
    corner=np.tile(selected,4)
    metadata=dict(edge_interval_m=edge,original_edge_interval_m=float(axes[0][4]-axes[0][3]),
        corner_macro_count=int(selected.sum()),original_tetrahedra=old.nelements,controlled_tetrahedra=mesh.nelements,
        original_global_max_condition=float(old_condition.max()),controlled_global_max_condition=float(condition.max()),
        original_corner_max_condition=float(old_condition[corner].max()),controlled_corner_max_condition=float(condition[corner].max()),
        original_corner_mean_condition=float(old_condition[corner].mean()),controlled_corner_mean_condition=float(condition[corner].mean()),
        body_shape_preserved=True,outer_and_normal_axis_planes_preserved=True,macro_connectivity_preserved=True,
        scope='fixed-cost corner-focused node redistribution; not uniform or nested refinement and not a force-error bound')
    reasons=[]
    if metadata['controlled_corner_max_condition']>metadata['original_corner_max_condition']*(1+1e-12):reasons.append('corner geometry worsened')
    if metadata['controlled_corner_mean_condition']>=metadata['original_corner_mean_condition']:reasons.append('corner geometry did not improve')
    if metadata['controlled_global_max_condition']>metadata['original_global_max_condition']*(1+1e-12):reasons.append('global worst shape worsened')
    if reasons:raise CornerGeometryRejected(metadata,reasons)
    return (mesh,lo,hi,changed,macros),metadata
