"""Read-only owned-array attribution and current, post-validation stage admission."""
import types,weakref
import numpy as np
from cfd_reference3d_allocator_pressure import current_rss_bytes
from cfd_reference3d_mixed_workspace import numeric_stage_admission

def array_owners(roots):
    seen=set();buffers={};groups={};opaque=0
    for name,root in roots.items():
        g=dict(unique_arrays=0,logical_array_bytes=0,attributed_backing_bytes=0,attributed_backing_owners=0);groups[name]=g;stack=[root]
        while stack:
            o=stack.pop();oid=id(o)
            if oid in seen:continue
            seen.add(oid)
            if isinstance(o,np.ndarray):
                if o.dtype.hasobject:continue
                g['unique_arrays']+=1;g['logical_array_bytes']+=o.nbytes;base=o
                while isinstance(base.base,np.ndarray):base=base.base
                bid=id(base)
                if bid not in buffers:
                    buffers[bid]=int(base.nbytes);g['attributed_backing_bytes']+=base.nbytes;g['attributed_backing_owners']+=1
                    if base.base is not None:opaque+=1
            elif isinstance(o,dict):stack.extend(o.values())
            elif isinstance(o,(tuple,list,set)):stack.extend(o)
            elif isinstance(o,(types.ModuleType,type,weakref.ReferenceType)) or callable(o):continue
            elif hasattr(o,'__dict__'):stack.extend(vars(o).values())
    return dict(groups=groups,known_unique_backing_bytes=sum(buffers.values()),known_unique_backing_owners=len(buffers),arrays_with_external_backing_owner=opaque,scope='reachable numeric NumPy backing owners; aliases attributed once to first root; excludes Python/C allocation and unknown external backing sizes; not RSS or resident floor')

def fresh_admission(factor,outer_bytes,coarse_bytes,rss_reader=current_rss_bytes):
    if not isinstance(outer_bytes,int) or not isinstance(coarse_bytes,int) or min(outer_bytes,coarse_bytes)<0:raise ValueError('invalid reservations')
    a=numeric_stage_admission(factor,outer_bytes+coarse_bytes)
    earlier=a['current_rss_before_numeric_bytes'];fresh=rss_reader()
    if not isinstance(fresh,int) or fresh<=0:raise ValueError('invalid current RSS')
    a.update(earlier_post_relief_rss_bytes=earlier,earlier_post_relief_estimate_bytes=a['estimated_numeric_stage_bytes'],current_rss_before_numeric_bytes=fresh,outer_basis_reservation_bytes=outer_bytes,coarse_pressure_reservation_bytes=coarse_bytes,residency_measurement='fresh at stage admission after all post-relief validation and inventory')
    a['estimated_numeric_stage_bytes']=sum(a[k] for k in ('current_rss_before_numeric_bytes','factor_storage_bytes','numeric_workspace_bytes','reserve_bytes','basis_reservation_bytes'))
    a['numeric_stage_admitted']=a['estimated_numeric_stage_bytes']<=1800*2**20
    return a
