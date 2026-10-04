"""Complete bounded node-block conversion and exact symmetric physical action."""
import ctypes as ct
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.linalg import LinearOperator
from cfd_reference3d_shared_factor import validate_csr,storage_sha
from cfd_reference3d_triangle import validate_upper


class VectorTriangle(LinearOperator):
    def __init__(self,upper,library,batch_rows=256):
        validate_csr(upper);validate_upper(upper)
        if upper.shape[0]%3 or not isinstance(batch_rows,int) or not 1<=batch_rows<=1024:raise ValueError('invalid node-block conversion')
        self.nodes=upper.shape[0]//3
        if self.nodes<1:raise ValueError('empty vector block operator')
        self.source_sha256=storage_sha(upper.indptr,upper.indices,upper.data)
        levels=[]
        def push(leaf):
            level=0
            while level<len(levels) and levels[level] is not None:
                old=levels[level];leaf=old+leaf;levels[level]=None;level+=1
            if level==len(levels):levels.append(None)
            levels[level]=leaf
        def chunks():
            for first in range(0,upper.shape[0],batch_rows):
                last=min(first+batch_rows,upper.shape[0]);begin,end=upper.indptr[[first,last]]
                rr=np.repeat(np.arange(first,last,dtype=np.int64),np.diff(upper.indptr[first:last+1]));cc=upper.indices[begin:end]
                rn,cn=rr%self.nodes,cc%self.nodes
                yield rr,cc,np.minimum(rn,cn),np.maximum(rn,cn),upper.data[begin:end],rn,cn
        for rr,cc,lo,hi,values,rn,cn in chunks():
            leaf=coo_matrix((np.ones(len(lo),dtype=bool),(lo,hi)),shape=(self.nodes,self.nodes)).tocsr();push(leaf)
        graph=None
        for leaf in levels:
            if leaf is not None:graph=leaf if graph is None else graph+leaf
        del levels,leaf
        graph.sort_indices();self.starts=np.array(graph.indptr,dtype=np.int64);self.rows=graph.indices
        node_rows=np.repeat(np.arange(self.nodes,dtype=np.int64),np.diff(self.starts));keys=node_rows*self.nodes+self.rows
        del node_rows,graph
        self.values=np.zeros(9*len(self.rows));assigned=np.zeros(len(self.values),dtype=bool)
        maximum_batch_entries=0
        for rr,cc,lo,hi,values,rn,cn in chunks():
            positions=np.searchsorted(keys,lo*self.nodes+hi)
            if np.any(positions>=len(keys)) or not np.array_equal(keys[positions],lo*self.nodes+hi):raise ValueError('incomplete vector graph')
            a,b=rr//self.nodes,cc//self.nodes;flip=rn>cn
            aa,bb=np.where(flip,b,a),np.where(flip,a,b);offsets=9*positions+3*aa+bb
            if np.any(assigned[offsets]):raise ValueError('duplicate node coefficient mapping')
            self.values[offsets]=values;assigned[offsets]=True
            diagonal=(rn==cn)&(a!=b);mirror=9*positions[diagonal]+3*bb[diagonal]+aa[diagonal]
            if np.any(assigned[mirror]):raise ValueError('duplicate diagonal block mirror')
            self.values[mirror]=values[diagonal];assigned[mirror]=True
            maximum_batch_entries=max(maximum_batch_entries,len(values))
        padding=int(np.count_nonzero(~assigned));del assigned
        for rr,cc,lo,hi,values,rn,cn in chunks():
            positions=np.searchsorted(keys,lo*self.nodes+hi);a,b=rr//self.nodes,cc//self.nodes;flip=rn>cn
            offsets=9*positions+3*np.where(flip,b,a)+np.where(flip,a,b)
            if not np.array_equal(self.values[offsets],values):raise ValueError('vector conversion altered physical coefficient')
        diagonal_positions=np.searchsorted(keys,np.arange(self.nodes,dtype=np.int64)*(self.nodes+1))
        if np.any(diagonal_positions>=len(keys)) or not np.array_equal(keys[diagonal_positions],np.arange(self.nodes,dtype=np.int64)*(self.nodes+1)):raise ValueError('missing node diagonal')
        blocks=self.values[(9*diagonal_positions[:,None]+np.arange(9)).ravel()].reshape(-1,3,3)
        if not np.array_equal(blocks,blocks.transpose(0,2,1)):raise ValueError('nonsymmetric node diagonal')
        if storage_sha(upper.indptr,upper.indices,upper.data)!=self.source_sha256:raise ValueError('conversion altered original CSR')
        del blocks,keys,diagonal_positions
        self.library=ct.CDLL(str(Path(library).resolve()));f=self.library.cfd_reference_vector_action
        f.argtypes=(ct.c_int,ct.POINTER(ct.c_long),ct.POINTER(ct.c_int),ct.POINTER(ct.c_double),ct.POINTER(ct.c_double),ct.POINTER(ct.c_double));f.restype=ct.c_int
        self.input_sha256=storage_sha(self.starts,self.rows,self.values)
        self.stored_bytes=self.starts.nbytes+self.rows.nbytes+self.values.nbytes
        self.metadata=dict(node_count=self.nodes,block_size=3,upper_node_blocks=len(self.rows),dense_block_values=len(self.values),structural_padding_values=padding,
            source_scalar_upper_nnz=upper.nnz,source_input_sha256=self.source_sha256,source_coefficients_preserved_bitwise=True,symmetric_diagonal_blocks_verified=True,
            stored_array_bytes=self.stored_bytes,scalar_source_array_bytes=upper.indptr.nbytes+upper.indices.nbytes+upper.data.nbytes,
            maximum_conversion_batch_entries=maximum_batch_entries,conversion_batch_rows=batch_rows,
            representation='complete upper node-pair 3x3 row-major blocks; full diagonal blocks and both symmetric directions; original component-major physical coordinates')
        super().__init__(dtype=np.dtype('float64'),shape=upper.shape)

    def _matvec(self,x):
        rhs=np.ascontiguousarray(np.asarray(x,dtype=float).reshape(-1))
        if rhs.shape!=self.shape[:1]:raise ValueError('invalid vector operator RHS')
        result=np.zeros(self.shape[0]);dp=ct.POINTER(ct.c_double)
        status=self.library.cfd_reference_vector_action(self.nodes,self.starts.ctypes.data_as(ct.POINTER(ct.c_long)),self.rows.ctypes.data_as(ct.POINTER(ct.c_int)),self.values.ctypes.data_as(dp),rhs.ctypes.data_as(dp),result.ctypes.data_as(dp))
        if status!=0:raise ValueError('nonfinite or invalid vector action')
        return result

    def _rmatvec(self,x):return self._matvec(x)

    def _matmat(self,x):return np.column_stack([self._matvec(col) for col in x.T])

    def input_unchanged(self):return self.input_sha256==storage_sha(self.starts,self.rows,self.values)
