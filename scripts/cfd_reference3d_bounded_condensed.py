"""Bounded exact mixed macro assembly directly on free retained variables.

Reuses original local elimination, reduction/reconstruction and full FE authority.
Hierarchical CSR sums replace all-entry COO and full-matrix row/column slicing.
No pressure mode, coefficient, penalty or numerical threshold is changed.
"""
import numpy as np
from scipy.sparse import coo_matrix,csr_matrix
from skfem import Basis,ElementDG
from cfd_reference3d_condensed import CondensedSystem,ReferenceMacro
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_p4 import ElementTetP4,require_sorted
from cfd_reference3d_quartic_pair import quartic_quadrature


class BoundedCondensedSystem(CondensedSystem):

    def __init__(self,mesh,mu,cache_cap_bytes=0,fixed_boundaries=(),assembly_batch=64):
        if not isinstance(assembly_batch,int) or not 1<=assembly_batch<=256:raise ValueError('invalid macro assembly batch')
        require_sorted(mesh);assert 0<=cache_cap_bytes<=64*1024**2
        self.mesh=mesh;self.mu=mu;self.nmacro=mesh.nelements//4
        assert mesh.nelements==4*self.nmacro and mesh.nelements<=50000
        self.ref=ReferenceMacro()
        self.ub=Basis(mesh,ElementTetP4(),quadrature=quartic_quadrature(),elements=np.array([0]))
        self.pb=Basis(mesh,ElementDG(ElementTetP3()),quadrature=self.ub.quadrature,elements=np.array([0]))
        self.records=[];inside=[];self.cache={};max_center_error=0.
        for macro in range(self.nmacro):
            cells=macro+self.nmacro*np.arange(4);vertices=np.unique(mesh.t[:,cells]);assert len(vertices)==5
            center=vertices[-1];corners=vertices[:-1];points=mesh.p[:,corners]
            J=points[:,1:]-points[:,0,None]
            max_center_error=max(max_center_error,float(np.max(np.abs(mesh.p[:,center]-points.mean(axis=1)))))
            mapping={int(v):i for i,v in enumerate(vertices)}
            local=np.array([[mapping[int(v)] for v in mesh.t[:,c]] for c in cells]).T
            order=[]
            for face in self.ref.mesh.t.T:
                found=np.flatnonzero(np.all(local==face[:,None],axis=0));assert len(found)==1;order.append(int(found[0]))
            ordered=cells[order]
            globalu=np.empty(69,dtype=np.int32)
            globalu[self.ref.ub.dofs.element_dofs.ravel()]=self.ub.dofs.element_dofs[:,ordered].ravel()
            assert np.array_equal(globalu[self.ref.ub.dofs.element_dofs],self.ub.dofs.element_dofs[:,ordered])
            globalp=self.pb.dofs.element_dofs[:,ordered].T.ravel()
            bubble=globalu[self.ref.bubble];inside.extend(bubble)
            key=J.tobytes()
            self.records.append((J,globalu,globalp,key))
        assert max_center_error<1e-12
        assert len(np.unique(inside))==35*self.nmacro
        self.trace=np.setdiff1d(np.arange(self.ub.N),inside);self.nt=len(self.trace)
        self.trace_inverse=np.full(self.ub.N,-1,dtype=np.int32);self.trace_inverse[self.trace]=np.arange(self.nt)
        self.shape=(3*self.nt+self.nmacro,)*2
        fixed=self.ub.get_dofs(list(fixed_boundaries)).all() if fixed_boundaries else np.array([],dtype=int)
        fixed_trace=self.trace_inverse[fixed];assert np.all(fixed_trace>=0)
        trace_free=np.setdiff1d(np.arange(self.nt),fixed_trace)
        self.retained_free=np.r_[np.concatenate([trace_free+a*self.nt for a in range(3)]),np.arange(self.nmacro)+3*self.nt]
        rowmap=np.full(self.shape[0],-1,dtype=np.int32);rowmap[self.retained_free]=np.arange(len(self.retained_free))
        reduced_shape=(len(self.retained_free),)*2
        capacity=assembly_batch*103**2
        rr=np.empty(capacity,dtype=np.int32);cc=np.empty(capacity,dtype=np.int32);values=np.empty(capacity)
        used=0;levels=[];errors=[];asymmetries=[];volumes=[];assembly_cache={};cache_bytes=0;max_live_csr=0;batch_count=0
        def csr_bytes(matrix):return matrix.data.nbytes+matrix.indices.nbytes+matrix.indptr.nbytes
        def push(leaf):
            nonlocal max_live_csr
            level=0
            while level<len(levels) and levels[level] is not None:
                old=levels[level];merged=old+leaf
                max_live_csr=max(max_live_csr,sum(csr_bytes(m) for m in levels if m is not None)+csr_bytes(leaf)+csr_bytes(merged))
                levels[level]=None;leaf=merged;del old,merged;level+=1
            if level==len(levels):levels.append(None)
            levels[level]=leaf
            max_live_csr=max(max_live_csr,sum(csr_bytes(m) for m in levels if m is not None))
        for macro,(J,globalu,globalp,key) in enumerate(self.records):
            if key in assembly_cache:S,error,asymmetry=assembly_cache[key]
            else:
                S,R,error,asymmetry=self.ref.eliminated(J,mu)
                if cache_bytes+R.nbytes<=cache_cap_bytes:
                    self.cache[key]=(R,);cache_bytes+=R.nbytes;assembly_cache[key]=(S,error,asymmetry)
            errors.append(error);asymmetries.append(asymmetry);volumes.append(abs(np.linalg.det(J))/6)
            full_ids=self._local_indices(macro,globalu);mapped=rowmap[full_ids];keep=np.flatnonzero(mapped>=0);ids=mapped[keep]
            n=len(ids);end=used+n*n
            assert end<=capacity
            rr[used:end]=np.repeat(ids,n);cc[used:end]=np.tile(ids,n);values[used:end]=S[np.ix_(keep,keep)].ravel();used=end
            if (macro+1)%assembly_batch==0 or macro+1==self.nmacro:
                leaf=coo_matrix((values[:used],(rr[:used],cc[:used])),shape=reduced_shape).tocsr()
                push(leaf);del leaf;used=0;batch_count+=1
        matrix=None
        for level in range(len(levels)-1,-1,-1):
            if levels[level] is None:continue
            old=levels[level]
            if matrix is None:matrix=old
            else:
                merged=matrix+old
                max_live_csr=max(max_live_csr,sum(csr_bytes(m) for m in levels if m is not None)+csr_bytes(matrix)+csr_bytes(merged))
                matrix=merged;del merged
            levels[level]=None;del old
        assert matrix is not None
        matrix.eliminate_zeros();matrix.sort_indices();self.reduced_matrix=matrix;self.volumes=np.array(volumes)
        self.metadata=dict(full_velocity_dofs=int(3*self.ub.N),full_pressure_dofs=int(self.pb.N),
            condensed_velocity_dofs=3*self.nt,condensed_pressure_dofs=self.nmacro,reduced_free_dofs=len(self.retained_free),
            local_eliminated_unknowns=184,local_retained_unknowns=103,
            geometry_classes=len({r[3] for r in self.records}),reconstruction_cache_classes=len(self.cache),reconstruction_cache_cap_bytes=cache_cap_bytes,
            reconstruction_cache_bytes=sum(r[0].nbytes for r in self.cache.values()),maximum_local_elimination_residual=max(errors),maximum_local_schur_asymmetry=max(asymmetries),
            maximum_alfeld_center_error_m=max_center_error,condensed_matrix_nnz=int(matrix.nnz),
            assembly=dict(kind='bounded macro COO batches and hierarchical CSR sums on free variables',macro_batch_cap=assembly_batch,batches=batch_count,
                coo_batch_allocation_bytes=rr.nbytes+cc.nbytes+values.nbytes,legacy_all_entry_coo_bytes=self.nmacro*103**2*16,
                maximum_live_csr_bytes_estimate=max_live_csr,csr_estimate_scope='owned arrays in live merge operands/results; excludes sparse-library workspaces and other solver allocations'),
            condensation_scope='original exact local mixed elimination and full-field reconstruction; only global sparse construction changes')
