"""Optional exact macOS sparse Cholesky reference inverse with owned lifetime."""
import ctypes as ct
import hashlib
from pathlib import Path
import numpy as np



class CholeskyFactor:
    def __init__(self,matrix,library,ordering='amd',prefix=None):
        self._handle=None
        if ordering not in ('amd','metis'):raise ValueError('invalid sparse factor ordering')
        if matrix.shape[0]!=matrix.shape[1] or matrix.shape[0]<1:
            raise ValueError('Cholesky requires a nonempty square matrix')
        if not np.all(np.isfinite(matrix.data)):raise ValueError('nonfinite physical matrix')
        delta=matrix-matrix.T
        if delta.nnz and np.max(np.abs(delta.data))>1e-12*max(np.max(np.abs(matrix.data)),1.):
            raise ValueError('Cholesky requires a symmetric physical matrix')
        del delta
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
            physical_prefix_dofs=self.n,lower_input_nnz=len(self.values),triangle_extraction='symmetric CSR upper rows to lower CSC columns',library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),
            scope='exact sparse Cholesky velocity inverse; one symmetric triangle, unchanged physical operator; macOS reference only')
    def solve(self,x):
        if not self._handle:raise ValueError('factor is closed')
        rhs=np.ascontiguousarray(x,dtype=np.float64)
        if rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid factor RHS')
        result=np.empty(self.n);dp=ct.POINTER(ct.c_double)
        status=self.library.cfd_reference_factor_solve(self._handle,rhs.ctypes.data_as(dp),result.ctypes.data_as(dp))
        if status!=0:raise ValueError(f'sparse Cholesky solve failed: status {status}')
        return result
    def close(self):
        if self._handle:self.library.cfd_reference_factor_destroy(self._handle);self._handle=None
    def __del__(self):self.close()
