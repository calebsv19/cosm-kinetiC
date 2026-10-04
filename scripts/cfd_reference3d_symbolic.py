"""Exact scalar permutations and complete nodal graph symbolic diagnostics."""
import ctypes as ct
import hashlib
from pathlib import Path
import numpy as np
from scipy.sparse import coo_matrix
from cfd_reference3d_shared_factor import validate_csr,storage_sha
from cfd_reference3d_triangle import validate_upper


def velocity_graph(upper,nodes,mode):
    validate_csr(upper);validate_upper(upper)
    if not isinstance(nodes,int) or nodes<1 or upper.shape!=(3*nodes,3*nodes):raise ValueError('three-component trace-node layout required')
    if mode=='component_major':return upper,None,1
    entries=upper.tocoo(copy=False)
    if mode=='interleaved_scalar':
        # new coordinate i gathers old coordinate permutation[i].
        permutation=np.arange(3*nodes,dtype=np.int32).reshape(3,nodes).T.ravel()
        inverse=np.empty_like(permutation);inverse[permutation]=np.arange(3*nodes,dtype=np.int32)
        row=inverse[entries.row];col=inverse[entries.col]
        graph=coo_matrix((entries.data,(np.minimum(row,col),np.maximum(row,col))),shape=upper.shape).tocsr()
        graph.sort_indices();assert graph.nnz==upper.nnz
        return graph,permutation,1
    if mode=='vector_block':
        row=entries.row%nodes;col=entries.col%nodes
        graph=coo_matrix((np.ones(len(row)),(np.minimum(row,col),np.maximum(row,col))),shape=(nodes,nodes)).tocsr()
        graph.data[:]=1.;graph.sort_indices()
        return graph,None,3
    raise ValueError('invalid symbolic graph mode')


class SymbolicFactor:
    """Owned structural analysis only: no values, numerical inverse or SPD proof."""
    def __init__(self,graph,library,ordering='metis',block_size=1):
        self._handle=None
        validate_csr(graph);validate_upper(graph)
        if ordering not in ('amd','metis') or not isinstance(block_size,int) or block_size not in (1,3):raise ValueError('invalid symbolic options')
        self.owner=graph;self.starts=np.array(graph.indptr,dtype=np.int64,copy=True);self.rows=graph.indices
        self.input_sha256=storage_sha(self.starts,self.rows,graph.data)
        self.library=ct.CDLL(str(Path(library).resolve()))
        lp=ct.POINTER(ct.c_long);ip=ct.POINTER(ct.c_int)
        assert ct.sizeof(ct.c_long)==8
        create=self.library.cfd_reference_symbolic_create
        create.argtypes=(ct.c_int,lp,ip,ct.c_int,ct.c_int,ip);create.restype=ct.c_void_p
        for name in ('storage','workspace'):
            function=getattr(self.library,'cfd_reference_symbolic_'+name)
            function.argtypes=(ct.c_void_p,);function.restype=ct.c_size_t
        destroy=self.library.cfd_reference_symbolic_destroy
        destroy.argtypes=(ct.c_void_p,);destroy.restype=None
        status=ct.c_int(-100)
        self._handle=create(graph.shape[0],self.starts.ctypes.data_as(lp),self.rows.ctypes.data_as(ip),2 if ordering=='amd' else 3,block_size,ct.byref(status))
        if not self._handle or status.value!=0:raise ValueError(f'symbolic factor rejected graph: status {status.value}')
        if not self.input_unchanged():raise ValueError('symbolic analysis modified input')
        self.metadata=dict(status=status.value,ordering=ordering,block_size=block_size,graph_rows=graph.shape[0],represented_scalar_dofs=graph.shape[0]*block_size,
            graph_upper_nnz=graph.nnz,factor_storage_bytes=int(self.library.cfd_reference_symbolic_storage(self._handle)),
            numeric_workspace_bytes=int(self.library.cfd_reference_symbolic_workspace(self._handle)),
            library_sha256=hashlib.sha256(Path(library).read_bytes()).hexdigest(),input_preserved=True,
            scope='exact symbolic requirements for this graph; excludes symbolic/input/FE/solver allocations; no numerical inverse or physical field')
        self.metadata['factor_plus_workspace_bytes']=self.metadata['factor_storage_bytes']+self.metadata['numeric_workspace_bytes']

    def input_unchanged(self):return self.input_sha256==storage_sha(self.starts,self.rows,self.owner.data)

    def close(self):
        if self._handle:self.library.cfd_reference_symbolic_destroy(self._handle);self._handle=None

    def __del__(self):self.close()
