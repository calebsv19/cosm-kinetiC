"""Identical Float coefficients with scalar CSC ordering; physical blocks retained."""
import ctypes as ct
import hashlib,time
from pathlib import Path
import numpy as np
from cfd_reference3d_shared_factor import SharedTriangleFactor,storage_sha
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_allocator_pressure import release_free_pages


def scalar_input(triangle):
    if not isinstance(triangle,VectorTriangle) or not triangle.input_unchanged():raise ValueError('complete vector triangle required')
    n=triangle.nodes;capacity=len(triangle.values)
    rows=np.empty(capacity,dtype=np.int32);values=np.empty(capacity,dtype=np.float32);starts=np.empty(3*n+1,dtype=np.int64)
    cursor=0;omitted=0;diagonal_mirrors=0
    for i in range(n):
        first,last=triangle.starts[i:i+2];nodes=triangle.rows[first:last];blocks=triangle.values[9*first:9*last].reshape(-1,3,3)
        for a in range(3):
            starts[3*i+a]=cursor
            rr=(3*nodes[:,None]+np.arange(3)).ravel();v=blocks[:,a,:].ravel()
            lower=rr>=3*i+a;diagonal_mirrors+=int((~lower).sum());omitted+=int(np.count_nonzero((v==0)&lower));keep=lower&(v!=0)
            rr=rr[keep];v=v[keep]
            with np.errstate(over='ignore',under='ignore'):rounded=v.astype(np.float32)
            if not np.all(np.isfinite(rounded)) or np.any(rounded==0):raise ValueError('Float coefficient overflow/underflow')
            k=len(v);rows[cursor:cursor+k]=rr;values[cursor:cursor+k]=rounded;cursor+=k
    starts[-1]=cursor
    # Views retain the full capacity; metadata counts these owned allocations.
    return starts,rows[:cursor],values[:cursor],dict(capacity_scalar_entries=capacity,scalar_lower_nnz=cursor,exact_zero_coefficients_omitted=omitted,symmetric_diagonal_mirrors_omitted=diagonal_mirrors,owned_input_allocation_bytes=starts.nbytes+rows.nbytes+values.nbytes,threshold=0.,all_nonzero_coefficients_rounded_identically=True)


class ScalarWorkspaceCholesky(SharedTriangleFactor):
    def __init__(self,triangle,library,ordering='metis',pressure_control=True,live_arrays=(),action=None,stage_callback=None):
        self._handle=None
        if ordering not in ('amd','metis'):raise ValueError('invalid sparse ordering')
        self.n=triangle.shape[0];self.owner=triangle
        self.starts,self.rows,self.rounded,self.conversion=scalar_input(triangle)
        self.values=self.rounded
        self.library=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);fp=ct.POINTER(ct.c_float)
        assert ct.sizeof(ct.c_long)==8
        create=self.library.cfd_reference_factor_create_symbolic;create.argtypes=(ct.c_int,lp,ip,fp,ct.c_int,ct.c_int,ip);create.restype=ct.c_void_p
        numeric=self.library.cfd_reference_factor_numeric;numeric.argtypes=(ct.c_void_p,);numeric.restype=ct.c_int
        solve=self.library.cfd_reference_factor_solve;solve.argtypes=(ct.c_void_p,dp,dp);solve.restype=ct.c_int
        for name in ('storage','workspace','numeric_workspace'):
            f=getattr(self.library,'cfd_reference_factor_'+name);f.argtypes=(ct.c_void_p,);f.restype=ct.c_size_t
        user=self.library.cfd_reference_factor_user_storage;user.argtypes=(ct.c_void_p,);user.restype=ct.c_int
        destroy=self.library.cfd_reference_factor_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        self.input_sha256=storage_sha(self.starts,self.rows,self.rounded);status=ct.c_int(-100);begin=time.monotonic()
        try:
            self._handle=create(self.n,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),self.rounded.ctypes.data_as(fp),2 if ordering=='amd' else 3,1,ct.byref(status))
            if not self._handle or status.value!=0:raise ValueError(f'scalar symbolic Cholesky failed: status {status.value}')
            if not self.input_unchanged():raise ValueError('symbolic stage modified input')
            symbolic_s=time.monotonic()-begin
            if stage_callback:stage_callback('workspace_symbolic_ready')
            self.pressure_record=release_free_pages((self.starts,self.rows,self.rounded,triangle.starts,triangle.rows,triangle.values,*live_arrays),action) if pressure_control else None
            if stage_callback:stage_callback('workspace_pressure_complete')
            begin=time.monotonic();numeric_status=numeric(self._handle);numeric_s=time.monotonic()-begin
            if numeric_status!=0:raise ValueError(f'scalar numeric Cholesky rejected: status {numeric_status}')
            if not user(self._handle) or not self.input_unchanged():raise ValueError('factor ownership/input changed')
            if stage_callback:stage_callback('workspace_numeric_ready')
            self.metadata=dict(kind='mixed_workspace_scalar_graph_cholesky',block_size=1,node_count=triangle.nodes,physical_coordinate_order='component_major',factor_coordinate_order='node_interleaved',ordering=ordering,scaling='none',factor_status=numeric_status,conversion=self.conversion,
                shared_input_sha256=self.input_sha256,shared_input_preserved_after_factor=True,symbolic_factor_storage_bytes=int(self.library.cfd_reference_factor_storage(self._handle)),numeric_workspace_bytes=int(self.library.cfd_reference_factor_numeric_workspace(self._handle)),numeric_workspace_retained_bytes=0,solve_workspace_bytes=int(self.library.cfd_reference_factor_workspace(self._handle)),user_factor_storage_verified=True,factor_input_allocation_bytes=self.conversion['owned_input_allocation_bytes'],preconditioner_value_dtype='float32',physical_operator_dtype='float64',rounded_values_sha256=storage_sha(self.rounded),physical_prefix_dofs=self.n,symbolic_wall_s=symbolic_s,numeric_wall_s=numeric_s,pressure_control=self.pressure_record,library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),scope='identical rounded Float coefficients with scalar graph ordering; original Float64 equations and fullFE authority preserved')
        except BaseException:self.close();raise

    def input_unchanged(self):return self.owner.input_unchanged() and self.input_sha256==storage_sha(self.starts,self.rows,self.rounded)

    def solve(self,x):
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid factor RHS')
        interleaved=np.ascontiguousarray(rhs.reshape(3,self.owner.nodes).T).ravel()
        return super().solve(interleaved).reshape(self.owner.nodes,3).T.ravel()
