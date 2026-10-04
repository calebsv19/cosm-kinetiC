"""Complete block symmetric storage and owned shared-input exact factor.

All mixed coefficients remain in velocity, coupling and pressure blocks. The
factor sees the positive velocity triangle's existing arrays; only its int64
column starts are allocated. No C shim, equations or residual authority changes.
"""
import ctypes as ct
import hashlib
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.linalg import LinearOperator
from cfd_reference3d_triangle import validate_upper,SymmetricTriangle
from cfd_reference3d_accelerate import CholeskyFactor


def storage_sha(*arrays):
    h=hashlib.sha256()
    for a in arrays:
        if not a.flags.c_contiguous:raise ValueError('contiguous input required for preservation check')
        h.update(str((a.shape,a.dtype.str)).encode());h.update(memoryview(a).cast('B'))
    return h.hexdigest()


def validate_csr(matrix):
    if not isinstance(matrix,csr_matrix):raise ValueError('explicit CSR required')
    if not matrix.has_sorted_indices or not matrix.has_canonical_format:raise ValueError('sorted unique sparse indices required')
    if matrix.data.dtype!=np.dtype('float64') or matrix.indices.dtype!=np.dtype('int32'):raise ValueError('float64/int32 sparse arrays required')
    if not matrix.data.flags.c_contiguous or not matrix.indices.flags.c_contiguous:raise ValueError('contiguous sparse arrays required')
    if matrix.indptr.shape!=(matrix.shape[0]+1,) or matrix.indptr[0]!=0 or matrix.indptr[-1]!=len(matrix.data):raise ValueError('invalid sparse column pointers')
    if len(matrix.indices)!=len(matrix.data) or np.any(np.diff(matrix.indptr)<0):raise ValueError('invalid sparse array lengths')
    if np.any(matrix.indices<0) or np.any(matrix.indices>=matrix.shape[1]):raise ValueError('invalid sparse index bounds')
    if not np.all(np.isfinite(matrix.data)):raise ValueError('nonfinite physical sparse arrays')


class BlockTriangle(LinearOperator):
    def __init__(self,upper,prefix):
        validate_csr(upper);validate_upper(upper)
        if not isinstance(prefix,int) or not 1<=prefix<upper.shape[0]:raise ValueError('invalid mixed velocity prefix')
        self.nv=prefix
        self.velocity=SymmetricTriangle(upper[:prefix,:prefix])
        self.coupling=upper[:prefix,prefix:]
        self.pressure=SymmetricTriangle(upper[prefix:,prefix:])
        for matrix in (self.velocity.upper,self.coupling,self.pressure.upper):validate_csr(matrix)
        self.stored_nnz=sum(m.nnz for m in (self.velocity.upper,self.coupling,self.pressure.upper))
        assert self.stored_nnz==upper.nnz
        self.stored_bytes=sum(m.data.nbytes+m.indices.nbytes+m.indptr.nbytes for m in (self.velocity.upper,self.coupling,self.pressure.upper))
        super().__init__(dtype=np.dtype('float64'),shape=upper.shape)

    def _matvec(self,x):
        flat=np.asarray(x).reshape(-1);v,p=flat[:self.nv],flat[self.nv:]
        return np.r_[self.velocity@v+self.coupling@p,self.coupling.T@v+self.pressure@p]

    def _rmatvec(self,x):return self._matvec(x)

    def _matmat(self,x):
        v,p=x[:self.nv],x[self.nv:]
        return np.vstack((self.velocity@v+self.coupling@p,self.coupling.T@v+self.pressure@p))


class SharedTriangleFactor(CholeskyFactor):
    """Shared contiguous CSR upper rows interpreted as symmetric lower CSC.

    The matrix owner and row/value references stay alive through factor cleanup;
    solve/close reuse the unchanged factor lifecycle. No hidden row/value copies.
    """
    def __init__(self,triangle,library,ordering='metis'):
        self._handle=None
        validate_csr(triangle);validate_upper(triangle)
        if ordering not in ('amd','metis'):raise ValueError('invalid sparse factor ordering')
        self.n=triangle.shape[0]
        self.owner=triangle
        self.starts=np.array(triangle.indptr,dtype=np.int64,copy=True)
        self.rows=triangle.indices
        self.values=triangle.data
        self.library=ct.CDLL(str(Path(library).resolve()))
        lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int);dp=ct.POINTER(ct.c_double)
        assert ct.sizeof(ct.c_long)==8
        create=self.library.cfd_reference_factor_create
        create.argtypes=(ct.c_int,lp,ip,dp,ct.c_int,ip);create.restype=ct.c_void_p
        solve=self.library.cfd_reference_factor_solve
        solve.argtypes=(ct.c_void_p,dp,dp);solve.restype=ct.c_int
        for name in ('storage','workspace'):
            function=getattr(self.library,'cfd_reference_factor_'+name)
            function.argtypes=(ct.c_void_p,);function.restype=ct.c_size_t
        destroy=self.library.cfd_reference_factor_destroy
        destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        self.input_sha256=storage_sha(self.starts,self.rows,self.values)
        status=ct.c_int(-100)
        self._handle=create(self.n,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),
            self.values.ctypes.data_as(dp),2 if ordering=='amd' else 3,ct.byref(status))
        if not self._handle or status.value!=0:raise ValueError(f'sparse Cholesky rejected physical velocity block: status {status.value}')
        if not self.input_unchanged():raise ValueError('factor modified shared physical input')
        self.metadata=dict(shared_input_sha256=self.input_sha256,shared_input_preserved_after_factor=True,kind='coupled_cholesky',factor_status=status.value,ordering=ordering,scaling='none',
            symbolic_factor_storage_bytes=int(self.library.cfd_reference_factor_storage(self._handle)),
            solve_workspace_bytes=int(self.library.cfd_reference_factor_workspace(self._handle)),
            physical_prefix_dofs=self.n,lower_input_nnz=len(self.values),triangle_extraction='shared complete velocity CSR upper rows to symmetric lower CSC columns',factor_input_allocation_bytes=self.starts.nbytes,shared_row_value_bytes=self.rows.nbytes+self.values.nbytes,library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),
            scope='exact sparse Cholesky velocity inverse; one symmetric triangle, unchanged physical operator; macOS reference only')

    def input_unchanged(self):
        return self.input_sha256==storage_sha(self.starts,self.rows,self.values)
