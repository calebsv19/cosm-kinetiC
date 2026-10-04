"""Shared exact-prediction storage; unchanged approximate factor ABI/lifecycle."""
import ctypes as ct
import hashlib,time
from pathlib import Path
import numpy as np
from cfd_reference3d_shared_factor import SharedTriangleFactor,validate_csr,storage_sha
from cfd_reference3d_triangle import validate_upper
from cfd_reference3d_accuracy_graded_operator import EncodedTriangle
from cfd_reference3d_allocator_pressure import release_free_pages


class EncodedWorkspaceCholesky(SharedTriangleFactor):
    def __init__(self,triangle,library,ordering='metis',pressure_control=True,live_arrays=(),action=None,stage_callback=None):
        self._handle=None
        if not isinstance(triangle,EncodedTriangle) or not triangle.input_unchanged():raise ValueError('complete vector triangle required')
        if ordering not in ('amd','metis'):raise ValueError('invalid sparse ordering')
        self.n=triangle.shape[0];self.owner=triangle
        self.starts=triangle.starts;self.rows=triangle.rows;self.values=triangle.corrections
        self.rounded=triangle.predictor
        if not np.all(np.isfinite(self.rounded)) or storage_sha(self.rounded)!=triangle.encoding['predictor_float_sha256']:raise ValueError('invalid original shared Float preconditioner input')
        self.library=ct.CDLL(str(Path(library).resolve()));lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double);fp=ct.POINTER(ct.c_float)
        assert ct.sizeof(ct.c_long)==8
        create=self.library.cfd_reference_factor_create_symbolic
        create.argtypes=(ct.c_int,lp,ip,fp,ct.c_int,ct.c_int,ip);create.restype=ct.c_void_p
        numeric=self.library.cfd_reference_factor_numeric;numeric.argtypes=(ct.c_void_p,);numeric.restype=ct.c_int
        solve=self.library.cfd_reference_factor_solve;solve.argtypes=(ct.c_void_p,dp,dp);solve.restype=ct.c_int
        for name in ('storage','workspace','numeric_workspace'):
            f=getattr(self.library,'cfd_reference_factor_'+name);f.argtypes=(ct.c_void_p,);f.restype=ct.c_size_t
        user=self.library.cfd_reference_factor_user_storage;user.argtypes=(ct.c_void_p,);user.restype=ct.c_int
        destroy=self.library.cfd_reference_factor_destroy;destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        self.input_sha256=storage_sha(self.starts,self.rows,self.values,self.rounded);status=ct.c_int(-100);begin=time.monotonic()
        try:
            self._handle=create(triangle.nodes,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),self.rounded.ctypes.data_as(fp),2 if ordering=='amd' else 3,3,ct.byref(status))
            if not self._handle or status.value!=0:raise ValueError(f'workspace symbolic Cholesky failed: status {status.value}')
            if not self.input_unchanged():raise ValueError('symbolic stage modified physical input')
            symbolic_s=time.monotonic()-begin
            if stage_callback:stage_callback('workspace_symbolic_ready')
            pressure=release_free_pages((self.starts,self.rows,self.values,self.rounded,*live_arrays),action) if pressure_control else None
            self.pressure_record=pressure
            if stage_callback:stage_callback('workspace_pressure_complete')
            begin=time.monotonic();numeric_status=numeric(self._handle);numeric_s=time.monotonic()-begin
            if numeric_status!=0:raise ValueError(f'workspace numeric Cholesky rejected physical block: status {numeric_status}')
            if not user(self._handle):raise ValueError('numeric factor did not retain caller-owned storage')
            if not self.input_unchanged():raise ValueError('numeric stage modified physical input')
            if stage_callback:stage_callback('workspace_numeric_ready')
            self.metadata=dict(kind='mixed_workspace_coupled_cholesky',block_size=3,node_count=triangle.nodes,physical_coordinate_order='component_major',factor_coordinate_order='node_interleaved',ordering=ordering,scaling='none',factor_status=numeric_status,
                shared_input_sha256=self.input_sha256,shared_input_preserved_after_factor=True,
                symbolic_factor_storage_bytes=int(self.library.cfd_reference_factor_storage(self._handle)),
                numeric_workspace_bytes=int(self.library.cfd_reference_factor_numeric_workspace(self._handle)),numeric_workspace_retained_bytes=0,
                solve_workspace_bytes=int(self.library.cfd_reference_factor_workspace(self._handle)),user_factor_storage_verified=True,
                factor_input_allocation_bytes=0,shared_predictor_array_bytes=self.rounded.nbytes,exact_word_correction_array_bytes=self.values.nbytes,original_physical_vector_input_sha256=triangle.original_input_sha256,exact_encoding=triangle.encoding,preconditioner_value_dtype="float32",physical_operator_dtype="float64",rounded_values_sha256=storage_sha(self.rounded),shared_row_value_bytes=self.rows.nbytes+self.values.nbytes,
                lower_input_block_nnz=len(self.rows),lower_input_scalar_values_count=len(self.values),physical_prefix_dofs=self.n,symbolic_wall_s=symbolic_s,numeric_wall_s=numeric_s,
                pressure_control=pressure,library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),
                scope='approximate float32 coupled factor/solve; original float64 physical mixed action/fullFE authority preserved; flexible outer iteration required')
        except BaseException:self.close();raise

    def input_unchanged(self):
        return self.owner.input_unchanged() and self.input_sha256==storage_sha(self.starts,self.rows,self.values,self.rounded)

    def solve(self,x):
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid vector factor RHS')
        interleaved=np.ascontiguousarray(rhs.reshape(3,self.owner.nodes).T).ravel()
        result=super().solve(interleaved)
        return result.reshape(self.owner.nodes,3).T.ravel()


def numeric_stage_admission(factor,basis_reservation_bytes=0):
    """Predeclared exact factor/scratch plus current residency and 32-MiB reserve."""
    if not factor._handle or not factor.pressure_record:raise ValueError('live symbolic and pressure-stage measurement required')
    storage=int(factor.library.cfd_reference_factor_storage(factor._handle))
    scratch=int(factor.library.cfd_reference_factor_numeric_workspace(factor._handle))
    resident=factor.pressure_record['current_rss_after_bytes'];reserve=32*1024**2
    estimated=resident+storage+scratch+reserve+basis_reservation_bytes
    return dict(factor_storage_bytes=storage,numeric_workspace_bytes=scratch,current_rss_before_numeric_bytes=resident,
        reserve_bytes=reserve,basis_reservation_bytes=basis_reservation_bytes,estimated_numeric_stage_bytes=estimated,numeric_stage_admitted=estimated<=1800*1024**2)
