"""Explicit upper-triangle mixed operator and exact velocity-prefix inverse.

The input is a representation of a symmetric operator, not a full CSR matrix.
No pressure entries or velocity/pressure couplings are discarded.
"""
import ctypes as ct
import hashlib
from pathlib import Path
import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.linalg import LinearOperator
from cfd_reference3d_accelerate import CholeskyFactor


def validate_upper(triangle):
    if not isinstance(triangle,csr_matrix):raise ValueError('explicit upper CSR required')
    if triangle.shape[0]!=triangle.shape[1] or triangle.shape[0]<1:raise ValueError('nonempty square triangle required')
    if not triangle.has_sorted_indices or not triangle.has_canonical_format:raise ValueError('sorted unique triangle indices required')
    if not np.all(np.isfinite(triangle.data)):raise ValueError('nonfinite triangle')
    for row in range(triangle.shape[0]):
        a,b=triangle.indptr[row:row+2]
        if b>a and triangle.indices[a]<row:raise ValueError('lower entry in explicit upper triangle')


class SymmetricTriangle(LinearOperator):
    def __init__(self,upper):
        validate_upper(upper)
        self.upper=upper
        self.diagonal=upper.diagonal()
        super().__init__(dtype=np.dtype('float64'),shape=upper.shape)

    def _matvec(self,x):
        flat=np.asarray(x).reshape(-1)
        return self.upper@flat+self.upper.T@flat-self.diagonal*flat

    def _rmatvec(self,x):return self._matvec(x)

    def _matmat(self,x):
        return self.upper@x+self.upper.T@x-self.diagonal[:,None]*x


class TriangleCholeskyFactor(CholeskyFactor):
    """Factor the positive prefix represented by explicit upper CSR storage.

    Inherits the existing C factor solve and owned close lifecycle. The complete
    mixed operator remains indefinite; only the velocity prefix is factored.
    """
    def __init__(self,triangle,library,ordering='amd',prefix=None):
        self._handle=None
        validate_upper(triangle)
        matrix=triangle
        if ordering not in ('amd','metis'):raise ValueError('invalid sparse factor ordering')
        if matrix.shape[0]!=matrix.shape[1] or matrix.shape[0]<1:
            raise ValueError('Cholesky requires a nonempty square matrix')
        if not np.all(np.isfinite(matrix.data)):raise ValueError('nonfinite physical matrix')
        self.n=matrix.shape[0] if prefix is None else prefix
        if not isinstance(self.n,int) or not 1<=self.n<=matrix.shape[0]:raise ValueError('invalid physical velocity prefix')
        # Symmetric CSR upper rows are lower CSC columns. Avoid full COO
        # conversion and prefix-block duplication; copy only retained entries.
        csr=matrix.tocsr(copy=False)
        if not csr.has_sorted_indices:raise ValueError('factor requires sorted physical sparse indices')
        ranges=np.empty((self.n,2),dtype=np.int64);self.starts=np.zeros(self.n+1,dtype=np.int64)
        for j in range(self.n):
            a,b=csr.indptr[j:j+2];columns=csr.indices[a:b]
            first=a+np.searchsorted(columns,j);last=a+np.searchsorted(columns,self.n)
            ranges[j]=(first,last);self.starts[j+1]=self.starts[j]+last-first
        self.rows=np.empty(self.starts[-1],dtype=np.int32);self.values=np.empty(self.starts[-1],dtype=np.float64)
        for j,(first,last) in enumerate(ranges):
            dest=slice(self.starts[j],self.starts[j+1])
            self.rows[dest]=csr.indices[first:last];self.values[dest]=csr.data[first:last]
        del csr,ranges
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
        status=ct.c_int(-100)
        self._handle=create(self.n,self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),
            self.values.ctypes.data_as(dp),2 if ordering=='amd' else 3,ct.byref(status))
        if not self._handle or status.value!=0:raise ValueError(f'sparse Cholesky rejected physical velocity block: status {status.value}')
        self.metadata=dict(kind='coupled_cholesky',factor_status=status.value,ordering=ordering,scaling='none',
            symbolic_factor_storage_bytes=int(self.library.cfd_reference_factor_storage(self._handle)),
            solve_workspace_bytes=int(self.library.cfd_reference_factor_workspace(self._handle)),
            physical_prefix_dofs=self.n,lower_input_nnz=len(self.values),triangle_extraction='explicit upper CSR storage to symmetric lower CSC prefix',library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),
            scope='exact sparse Cholesky velocity inverse; one symmetric triangle, unchanged physical operator; macOS reference only')
