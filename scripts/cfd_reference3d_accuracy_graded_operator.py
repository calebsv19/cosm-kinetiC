"""Lossless original Float64 action; Float32 predictor shared with PC only."""
import ctypes as ct
from pathlib import Path
import numpy as np
from scipy.sparse.linalg import LinearOperator
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_accuracy_graded_encoding import encode
from cfd_reference3d_shared_factor import storage_sha

class EncodedTriangle(LinearOperator):
    def __init__(self,triangle,library,check=None):
        if not isinstance(triangle,VectorTriangle) or not triangle.input_unchanged():raise ValueError('unchanged complete original triangle required')
        self.nodes=triangle.nodes;self.starts=triangle.starts;self.rows=triangle.rows
        self.source_sha256=triangle.source_sha256;self.original_input_sha256=triangle.input_sha256
        self.predictor,self.corrections,self.encoding=encode(triangle.values,check=check)
        self.input_sha256=storage_sha(self.starts,self.rows,self.predictor,self.corrections)
        self.stored_bytes=sum(a.nbytes for a in (self.starts,self.rows,self.predictor,self.corrections))
        self.library=ct.CDLL(str(Path(library).resolve()));f=self.library.cfd_reference_encoded_vector_action
        f.argtypes=(ct.c_int,ct.POINTER(ct.c_long),ct.POINTER(ct.c_int),ct.POINTER(ct.c_float),ct.POINTER(ct.c_int32),ct.POINTER(ct.c_double),ct.POINTER(ct.c_double));f.restype=ct.c_int
        self.metadata=dict(triangle.metadata,stored_array_bytes=self.stored_bytes,original_vector_storage_sha256=self.original_input_sha256,encoded_storage_sha256=self.input_sha256,encoding=self.encoding,physical_action_dtype='original float64 restored bits, original multiply/add order',preconditioner_shared_predictor=True)
        super().__init__(dtype=np.dtype('float64'),shape=triangle.shape)

    def _matvec(self,x):
        rhs=np.ascontiguousarray(np.asarray(x,dtype=np.float64).reshape(-1))
        if rhs.shape!=self.shape[:1]:raise ValueError('invalid encoded operator RHS')
        result=np.zeros(self.shape[0]);dp=ct.POINTER(ct.c_double)
        status=self.library.cfd_reference_encoded_vector_action(self.nodes,self.starts.ctypes.data_as(ct.POINTER(ct.c_long)),self.rows.ctypes.data_as(ct.POINTER(ct.c_int)),self.predictor.ctypes.data_as(ct.POINTER(ct.c_float)),self.corrections.ctypes.data_as(ct.POINTER(ct.c_int32)),rhs.ctypes.data_as(dp),result.ctypes.data_as(dp))
        if status:raise ValueError('nonfinite or invalid encoded action')
        return result

    def _rmatvec(self,x):return self._matvec(x)
    def _matmat(self,x):return np.column_stack([self._matvec(col) for col in x.T])
    def input_unchanged(self):return self.input_sha256==storage_sha(self.starts,self.rows,self.predictor,self.corrections)
