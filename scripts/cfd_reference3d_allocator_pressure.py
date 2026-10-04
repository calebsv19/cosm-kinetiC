"""Explicit optional macOS free-page pressure control with live-input proof.

Owned process high-water is never reset. Reported release bytes and current RSS
are separate observations; only the completed solver establishes resource benefit.
"""
import ctypes as ct
import os
import resource
import subprocess
import sys
import time
import numpy as np
from cfd_reference3d_shared_factor import storage_sha


def current_rss_bytes():
    row=subprocess.run(['/bin/ps','-o','rss=','-p',str(os.getpid())],capture_output=True,text=True,check=True).stdout.strip()
    if not row or not row.isdecimal():raise RuntimeError('current process RSS unavailable')
    return int(row)*1024


def pressure_api():
    if sys.platform!='darwin':raise RuntimeError('allocator pressure control requires macOS')
    library=ct.CDLL(None)
    try:function=library.malloc_zone_pressure_relief
    except AttributeError as error:raise RuntimeError('malloc pressure API unavailable') from error
    function.argtypes=(ct.c_void_p,ct.c_size_t);function.restype=ct.c_size_t
    return library,function


def release_free_pages(arrays,action=None):
    control_begin=time.monotonic()
    # Validate/hash first; no hidden copy of a strided or nonfinite physical input.
    if not arrays:raise ValueError('live arrays required')
    for a in arrays:
        if not isinstance(a,np.ndarray) or not a.flags.c_contiguous:raise ValueError('contiguous live NumPy arrays required')
        if not np.all(np.isfinite(a)):raise ValueError('nonfinite live input')
    digest=storage_sha(*arrays)
    library,function=pressure_api()
    before_action=None if action is None else np.asarray(action()).copy()
    if before_action is not None and not np.all(np.isfinite(before_action)):raise ValueError('nonfinite pre-pressure action')
    before_rss=current_rss_bytes();before_peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    begin=time.monotonic();reported=int(function(None,0));elapsed=time.monotonic()-begin
    after_rss=current_rss_bytes();after_peak=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if storage_sha(*arrays)!=digest:raise ValueError('allocator pressure modified live physical input')
    action_error=None
    if action is not None:
        after_action=np.asarray(action())
        if not np.array_equal(before_action,after_action):raise ValueError('allocator pressure modified physical operator action')
        action_error=0.
    assert after_peak>=before_peak
    return dict(total_control_wall_s=time.monotonic()-control_begin,api='malloc_zone_pressure_relief(NULL,0)',reported_released_bytes=reported,pressure_call_wall_s=elapsed,
        current_rss_before_bytes=before_rss,current_rss_after_bytes=after_rss,
        owned_high_water_before_bytes=before_peak,owned_high_water_after_bytes=after_peak,
        live_input_sha256=digest,live_input_preserved=True,action_preserved=action is not None,action_max_absolute_change=action_error,
        scope='best-effort free-page control; reported bytes/current RSS do not replace owned peak or completed-run resource authority')
