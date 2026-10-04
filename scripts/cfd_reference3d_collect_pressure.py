"""Local explicit Python cycle collection with complete live-array/action guards."""
import gc,resource,time
import numpy as np
from cfd_reference3d_allocator_pressure import release_free_pages as allocator_pressure,current_rss_bytes
from cfd_reference3d_shared_factor import storage_sha

def release_free_pages(arrays,action=None):
    if not arrays:raise ValueError('live arrays required')
    digest=storage_sha(*arrays);before=None if action is None else np.asarray(action()).copy()
    if before is not None and not np.all(np.isfinite(before)):raise ValueError('nonfinite pre-collection action')
    policy=(gc.isenabled(),gc.get_threshold());before_count=gc.get_count();before_stats=gc.get_stats()
    before_rss=current_rss_bytes();before_peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    begin=time.monotonic();collected=gc.collect(2);elapsed=time.monotonic()-begin
    after_rss=current_rss_bytes();after_peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if storage_sha(*arrays)!=digest:raise ValueError('collection modified live input')
    if action is not None and not np.array_equal(before,np.asarray(action())):raise ValueError('collection modified physical action')
    if policy!=(gc.isenabled(),gc.get_threshold()):raise ValueError('collection changed global GC policy')
    assert after_peak>=before_peak
    row=allocator_pressure(arrays,action)
    row['python_collection']=dict(generation=2,collected_object_count=collected,wall_s=elapsed,current_rss_before_bytes=before_rss,current_rss_after_bytes=after_rss,
        owned_high_water_before_bytes=before_peak,owned_high_water_after_bytes=after_peak,counts_before=before_count,counts_after=gc.get_count(),stats_before=before_stats,stats_after=gc.get_stats(),
        live_input_sha256=digest,live_input_preserved=True,action_preserved=action is not None,global_gc_policy_preserved=True,
        scope='explicit unreachable-object collection; object count is not reclaimed bytes; observed current RSS/owned peak and completed numerical resource acceptance remain separate')
    return row
