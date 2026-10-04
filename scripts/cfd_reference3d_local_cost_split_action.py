"""Caller-owned native CG8 timing; exact original physical action and recurrence."""
import ctypes as ct
import numpy as np

def profiled_inner8(factor,x):
    rhs=np.ascontiguousarray(x,dtype=np.float64)
    if not factor._handle or rhs.shape!=(factor.n,) or not np.all(np.isfinite(rhs)):raise ValueError('live factor and finite profiled RHS required')
    lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);fp=ct.POINTER(ct.c_float)
    fn=factor.library.cfd_reference_inner_cg8_profile;fn.argtypes=(ct.c_void_p,ct.c_int,lp,ip,dp,fp,ip,ip,dp,dp,dp,ct.c_size_t,ct.c_int,ip,dp);fn.restype=ct.c_int
    owner=factor.owner;encoded=hasattr(owner,'corrections');output=np.empty(factor.n);work=np.empty(7*factor.n);steps=ct.c_int(-1);stats=np.empty(5)
    status=fn(factor._handle,owner.nodes,owner.starts.ctypes.data_as(lp),owner.rows.ctypes.data_as(ip),None if encoded else owner.values.ctypes.data_as(dp),owner.predictor.ctypes.data_as(fp) if encoded else None,owner.corrections.ctypes.data_as(ip) if encoded else None,factor.perm.ctypes.data_as(ip),rhs.ctypes.data_as(dp),output.ctypes.data_as(dp),work.ctypes.data_as(dp),len(work),int(encoded),ct.byref(steps),stats.ctypes.data_as(dp))
    if status or not 0<=steps.value<=8 or not np.all(np.isfinite(stats)) or min(stats)<0 or stats[3]!=steps.value or stats[4]!=steps.value:raise ValueError('profiled inner action rejected: '+str(status))
    return output,dict(packed_solve_s=float(stats[0]),physical_action_s=float(stats[1]),native_total_s=float(stats[2]),packed_solve_calls=int(stats[3]),physical_action_calls=int(stats[4]),iterations=steps.value)
