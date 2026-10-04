"""Native execution of the unchanged bounded original physical CG8 action."""
import ctypes as ct
import numpy as np

def native_inner8(factor,x):
    rhs=np.ascontiguousarray(x,dtype=np.float64)
    if not factor._handle or rhs.shape!=(factor.n,) or not np.all(np.isfinite(rhs)):raise ValueError('live factor and finite native inner RHS required')
    lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);fp=ct.POINTER(ct.c_float)
    fn=factor.library.cfd_reference_inner_cg8
    fn.argtypes=(ct.c_void_p,ct.c_int,lp,ip,dp,fp,ip,ip,dp,dp,dp,ct.c_size_t,ct.c_int,ip);fn.restype=ct.c_int
    owner=factor.owner;encoded=hasattr(owner,'corrections');output=np.empty(factor.n);work=np.empty(7*factor.n);steps=ct.c_int(-1)
    status=fn(factor._handle,owner.nodes,owner.starts.ctypes.data_as(lp),owner.rows.ctypes.data_as(ip),None if encoded else owner.values.ctypes.data_as(dp),owner.predictor.ctypes.data_as(fp) if encoded else None,owner.corrections.ctypes.data_as(ip) if encoded else None,factor.perm.ctypes.data_as(ip),rhs.ctypes.data_as(dp),output.ctypes.data_as(dp),work.ctypes.data_as(dp),len(work),int(encoded),ct.byref(steps))
    if status or not 0<=steps.value<=8:raise ValueError('native inner action rejected: '+str(status))
    return output
