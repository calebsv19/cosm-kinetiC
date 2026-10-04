"""One optional cyclic collection protected by complete live-array/action hashes."""
import gc,time,resource
import numpy as np
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_allocator_pressure import current_rss_bytes

def collect_live(arrays,action,collector=gc.collect,rss_reader=current_rss_bytes):
    if not arrays or action is None:raise ValueError('complete live arrays and action required')
    for a in arrays:
        if not isinstance(a,np.ndarray) or not a.flags.c_contiguous or not np.all(np.isfinite(a)):raise ValueError('finite contiguous live arrays required')
    begin=time.monotonic();digest=storage_sha(*arrays);before=np.asarray(action()).copy()
    if not before.flags.c_contiguous or not np.all(np.isfinite(before)):raise ValueError('finite full action required')
    action_digest=storage_sha(before);enabled=gc.isenabled();stats=gc.get_stats();peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss;rss=rss_reader();call_begin=time.monotonic();count=collector();call_s=time.monotonic()-call_begin
    if gc.isenabled()!=enabled:raise ValueError('collection changed GC enabled state')
    if not isinstance(count,int) or count<0:raise ValueError('invalid collection count')
    if storage_sha(*arrays)!=digest:raise ValueError('collection changed live input bits')
    after=np.asarray(action())
    if not after.flags.c_contiguous or storage_sha(after)!=action_digest:raise ValueError('collection changed full mixed action bits')
    after_rss=rss_reader();after_peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if after_peak<peak:raise ValueError('owned high-water decreased')
    return dict(explicit_collection_performed=True,collected_objects=count,collection_wall_s=call_s,total_control_wall_s=time.monotonic()-begin,current_rss_before_bytes=rss,current_rss_after_validation_bytes=after_rss,owned_high_water_before_bytes=peak,owned_high_water_after_bytes=after_peak,gc_enabled_before=enabled,gc_enabled_after=gc.isenabled(),gc_stats_before=stats,gc_stats_after=gc.get_stats(),live_input_sha256=digest,full_action_sha256=action_digest,live_input_preserved_bitwise=True,full_action_preserved_bitwise=True,scope='unreachable cyclic objects only, no physical storage/modes change; RSS/object counts do not certify complete solve benefit')
