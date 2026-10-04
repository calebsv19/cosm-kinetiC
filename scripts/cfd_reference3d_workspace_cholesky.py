"""Exact shared-input Cholesky with explicit symbolic/numeric storage stages."""
import ctypes as ct
import hashlib,time
from pathlib import Path
import numpy as np
from cfd_reference3d_shared_factor import SharedTriangleFactor,validate_csr,storage_sha
from cfd_reference3d_triangle import validate_upper
from cfd_reference3d_allocator_pressure import release_free_pages


class WorkspaceCholesky(SharedTriangleFactor):
    def __init__(self,triangle,library,ordering='metis',pressure_control=True,live_arrays=(),action=None,stage_callback=None):
        self._handle=None;validate_csr(triangle);validate_upper(triangle)
        if ordering not in ('amd','metis'):raise ValueError('invalid sparse ordering')
        self.n=triangle.shape[0];self.owner=triangle
        self.starts=np.array(triangle.indptr,dtype=np.int64,copy=True);self.rows=triangle.indices;self.values=triangle.data
        self.library=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double)
        assert ct.sizeof(ct.c_long)==8
        create=self.library.cfd_reference_factor_create_symbolic
        create.argtypes=(ct.c_int,lp,ip,dp,ct.c_int,ip);create.restype=ct.c_void_p
        numeric=self.library.cfd_reference_factor_numeric;numeric.argtypes=(ct.c_void_p,);numeric.restype=ct.c_int
        solve=self.library.cfd_reference_factor_solve;solve.argtypes=(ct.c_void_p,dp,dp);solve.restype=ct.c_int
        for name in ('storage','workspace','numeric_workspace'):
            f=getattr(self.library,'cfd_reference_factor_'+name);f.argtypes=(ct.c_void_p,);f.restype=ct.c_size_t
        user=self.library.cfd_reference_factor_user_storage;user.argtypes=(ct.c_void_p,);user.restype=ct.c_int
        destroy=self.library.cfd_reference_factor_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        self.input_sha256=storage_sha(self.starts,self.rows,self.values);status=ct.c_int(-100);begin=time.monotonic()
        try:
            self._handle=create(self.n,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),self.values.ctypes.data_as(dp),2 if ordering=='amd' else 3,ct.byref(status))
            if not self._handle or status.value!=0:raise ValueError(f'workspace symbolic Cholesky failed: status {status.value}')
            if not self.input_unchanged():raise ValueError('symbolic stage modified physical input')
            symbolic_s=time.monotonic()-begin
            if stage_callback:stage_callback('workspace_symbolic_ready')
            pressure=release_free_pages((self.starts,self.rows,self.values,*live_arrays),action) if pressure_control else None
            if stage_callback:stage_callback('workspace_pressure_complete')
            begin=time.monotonic();numeric_status=numeric(self._handle);numeric_s=time.monotonic()-begin
            if numeric_status!=0:raise ValueError(f'workspace numeric Cholesky rejected physical block: status {numeric_status}')
            if not user(self._handle):raise ValueError('numeric factor did not retain caller-owned storage')
            if not self.input_unchanged():raise ValueError('numeric stage modified physical input')
            if stage_callback:stage_callback('workspace_numeric_ready')
            self.metadata=dict(kind='workspace_coupled_cholesky',ordering=ordering,scaling='none',factor_status=numeric_status,
                shared_input_sha256=self.input_sha256,shared_input_preserved_after_factor=True,
                symbolic_factor_storage_bytes=int(self.library.cfd_reference_factor_storage(self._handle)),
                numeric_workspace_bytes=int(self.library.cfd_reference_factor_numeric_workspace(self._handle)),numeric_workspace_retained_bytes=0,
                solve_workspace_bytes=int(self.library.cfd_reference_factor_workspace(self._handle)),user_factor_storage_verified=True,
                factor_input_allocation_bytes=self.starts.nbytes,shared_row_value_bytes=self.rows.nbytes+self.values.nbytes,
                lower_input_nnz=len(self.values),physical_prefix_dofs=self.n,symbolic_wall_s=symbolic_s,numeric_wall_s=numeric_s,
                pressure_control=pressure,library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),
                scope='exact float64 coupled velocity Cholesky; caller-owned factor/scratch buffers and stage boundaries; unchanged mixed/full FE authority')
        except BaseException:self.close();raise
