"""Verify completed workspace retirement without collecting or changing FE authority."""
import weakref,time,resource
import numpy as np
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_allocator_pressure import current_rss_bytes

def capture(arrays,info,true_metric):
    if info!=0 or not np.isfinite(true_metric) or not 0<=true_metric<=1e-11:raise ValueError('strict converged retained solution required')
    if not isinstance(arrays,dict) or not arrays:raise ValueError('named completed workspace arrays required')
    refs={};records={};unique={}
    for name,a in arrays.items():
        if not isinstance(name,str) or not isinstance(a,np.ndarray) or a.dtype.hasobject or not a.flags.c_contiguous or not np.all(np.isfinite(a)):raise ValueError('finite contiguous completed workspace required')
        base=a
        while isinstance(base.base,np.ndarray):base=base.base
        if base.base is not None:raise ValueError('known owned backing required')
        refs[name]=(weakref.ref(a),weakref.ref(base));unique[id(base)]=base.nbytes
        records[name]=dict(shape=list(a.shape),dtype=a.dtype.str,logical_bytes=a.nbytes,backing_bytes=base.nbytes,sha256=storage_sha(a))
    return refs,dict(arrays=records,unique_backing_bytes=sum(unique.values()),retained_info=int(info),retained_true_metric=float(true_metric),capture_current_rss_bytes=current_rss_bytes(),capture_owned_high_water_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,capture_wall_clock=time.monotonic())

def verify(refs,record,authority_arrays,authority_sha256):
    if not refs or not isinstance(authority_arrays,tuple) or not authority_arrays:raise ValueError('retirement and remaining authority required')
    alive=[name for name,pair in refs.items() if any(r() is not None for r in pair)]
    if alive:raise ValueError('completed workspace buffer still live: '+','.join(alive))
    if storage_sha(*authority_arrays)!=authority_sha256:raise ValueError('retirement changed FE/load/retained solution authority')
    for a in authority_arrays:
        if a.dtype.hasobject or not np.all(np.isfinite(a)):raise ValueError('invalid remaining authority array')
    peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if peak<record['capture_owned_high_water_bytes']:raise ValueError('owned high water decreased')
    return dict(record,verified_wall_s=time.monotonic()-record['capture_wall_clock'],all_completed_buffers_and_backing_owners_released=True,remaining_authority_sha256=authority_sha256,remaining_authority_preserved_bitwise=True,current_rss_after_bytes=current_rss_bytes(),owned_high_water_after_bytes=peak,explicit_collection_performed=False,allocator_relief_performed=False,scope='completed condensed operator and work residency only; original full FE reconstruction and physical diagnostics still required')
