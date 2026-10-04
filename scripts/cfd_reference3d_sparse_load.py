"""Lossless indexed residency for a full FE load, restored after factor release."""
import numpy as np
from cfd_reference3d_shared_factor import storage_sha

class SparseLoad:
    def __init__(self,dense):
        if not isinstance(dense,np.ndarray) or dense.ndim!=1 or dense.dtype!=np.dtype('float64') or not dense.flags.c_contiguous or not 1<=len(dense)<=np.iinfo(np.int32).max or not np.all(np.isfinite(dense)):
            raise ValueError('finite contiguous float64 vector required')
        self.size=len(dense);self.dense_sha256=storage_sha(dense)
        self.indices=np.flatnonzero((dense!=0)|np.signbit(dense)).astype(np.int32)
        self.values=dense[self.indices].copy();self.indices.flags.writeable=False;self.values.flags.writeable=False
        self.input_sha256=storage_sha(self.indices,self.values)
        self.metadata=dict(full_dofs=self.size,retained_entries=len(self.indices),dense_array_bytes=dense.nbytes,indexed_array_bytes=self.indices.nbytes+self.values.nbytes,
            dense_sha256=self.dense_sha256,indexed_sha256=self.input_sha256,scope='lossless full original FE load; all nonzero and signed-zero entries retained without tolerance; no equation or RHS change')
        restored=self.materialize();assert storage_sha(restored)==self.dense_sha256
    def unchanged(self):return storage_sha(self.indices,self.values)==self.input_sha256
    def materialize(self):
        if not self.unchanged():raise ValueError('indexed full load modified')
        dense=np.zeros(self.size);dense[self.indices]=self.values
        if storage_sha(dense)!=self.dense_sha256:raise ValueError('full load restoration changed original bits')
        return dense
