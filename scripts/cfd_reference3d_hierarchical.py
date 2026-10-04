"""Invertible cubic/quartic preconditioner coordinates with exact coupled blocks."""
import numpy as np
from scipy.sparse import csr_matrix,block_diag,triu
from skfem import Basis,MeshTet
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_cubic_coarse import scalar_macro_cubic_interpolation
from cfd_reference3d_shared_factor import SharedTriangleFactor,storage_sha
from cfd_reference3d_triangle import SymmetricTriangle


class HierarchicalCoordinates:
    def __init__(self,Z,pivots,entities,metadata=None):
        if not isinstance(Z,csr_matrix) or not 0<Z.shape[1]<Z.shape[0] or not np.all(np.isfinite(Z.data)):raise ValueError('invalid hierarchy interpolation')
        self.Z=Z;self.pivots=np.asarray(pivots,dtype=int);self.nv,self.nc=Z.shape
        if self.pivots.shape!=(self.nc,) or len(np.unique(self.pivots))!=self.nc or np.any(self.pivots<0) or np.any(self.pivots>=self.nv):raise ValueError('invalid hierarchy pivots')
        self.fine=np.setdiff1d(np.arange(self.nv),self.pivots)
        K=Z[self.pivots];category=np.full(self.nc,-1);owner=np.full(self.nc,-1);expected={}
        for group,(kind,ids) in enumerate(entities):
            ids=np.asarray(ids,dtype=int);level={'vertex':0,'edge':1,'face':2}.get(kind,-1)
            if level<0 or len(ids)!=(2 if kind=='edge' else 1) or np.any(ids<0) or np.any(ids>=self.nc) or np.any(category[ids]>=0):raise ValueError('invalid hierarchy entity certificate')
            category[ids]=level;owner[ids]=group
            expected[group]=(ids,np.array([[1.0546875,-.2109375],[-.2109375,1.0546875]]) if kind=='edge' else np.array([[1. if kind=='vertex' else .84375]]))
        if np.any(category<0):raise ValueError('incomplete hierarchy entity certificate')
        for row in range(self.nc):
            cols=K.indices[K.indptr[row]:K.indptr[row+1]];values=K.data[K.indptr[row]:K.indptr[row+1]]
            active=cols[values!=0]
            if np.any(category[active]>category[row]) or np.any((category[active]==category[row])&(owner[active]!=owner[row])):raise ValueError('pivot matrix is not entity block triangular')
        for ids,reference in expected.values():
            if not np.array_equal(K[ids][:,ids].toarray(),reference):raise ValueError('noninvertible or altered hierarchy pivot block')
        self.metadata=dict(metadata or {},coarse_pivot_dofs=self.nc,fine_complement_dofs=len(self.fine),entity_block_triangular_pivot_verified=True,
            minimum_entity_pivot_determinant=.84375,coordinate_scope='bijective cubic interpolation plus all complementary quartic unit carriers; only preconditioner coordinates change')
        self.sha256=storage_sha(Z.indptr,Z.indices,Z.data,self.pivots,self.fine)

    def pull(self,x):return np.r_[self.Z.T@x,x[self.fine]]

    def push(self,x):
        result=self.Z@x[:self.nc];result[self.fine]+=x[self.nc:];return result

    def input_unchanged(self):return self.sha256==storage_sha(self.Z.indptr,self.Z.indices,self.Z.data,self.pivots,self.fine)


def macro_hierarchy(system,nv):
    if nv%3 or nv!=len(system.retained_free)-system.nmacro:raise ValueError('invalid hierarchy velocity count')
    nf=nv//3;free=system.retained_free[:nf]
    for c in range(3):np.testing.assert_array_equal(system.retained_free[c*nf:(c+1)*nf],free+c*system.nt)
    raw,T,positions,meta=scalar_macro_cubic_interpolation(system)
    macros=[np.unique(system.mesh.t[:,m+system.nmacro*np.arange(4)])[:-1] for m in range(system.nmacro)]
    vertices=np.unique(np.concatenate(macros));mapping=np.full(system.mesh.nvertices,-1,dtype=int);mapping[vertices]=np.arange(len(vertices))
    parent=MeshTet(system.mesh.p[:,vertices],mapping[np.array(macros).T]);coarse=Basis(parent,ElementTetP3(),quadrature=system.ub.quadrature,elements=np.array([0]))
    pivots=np.full(coarse.N,-1,dtype=int);X=np.rint(4*system.ref.ub.doflocs[:,system.ref.trace]).astype(int)
    for macro,(_,globalu,_,_) in enumerate(system.records):
        fine_ids=system.trace_inverse[globalu[system.ref.trace]]
        for j,alpha in enumerate(ElementTetP3._alpha):
            quarter=alpha.copy();quarter[np.argmax(alpha)]+=1
            if np.count_nonzero(alpha)==3:quarter=alpha.copy();quarter[np.flatnonzero(alpha)[0]]+=1
            matches=np.flatnonzero(np.all(X==quarter[1:,None],axis=0))
            if len(matches)!=1:raise ValueError('missing cubic/quartic pivot carrier')
            cid=int(coarse.dofs.element_dofs[j,macro]);fid=int(fine_ids[matches[0]])
            if pivots[cid]>=0 and pivots[cid]!=fid:raise ValueError('inconsistent shared pivot orientation')
            pivots[cid]=fid
    if np.any(pivots<0):raise ValueError('incomplete hierarchy pivots')
    fixed=np.setdiff1d(np.arange(system.nt),free);keep=np.setdiff1d(np.arange(raw.shape[1]),np.unique(raw[fixed].indices))
    if not len(keep) or np.any(~np.isin(pivots[keep],free)):raise ValueError('coarse pivot excluded by essential constraints')
    scalar=raw[free][:,keep];boundary=raw[fixed][:,keep]
    if boundary.nnz and np.any(boundary.data!=0):raise ValueError('hierarchy violates essential boundary')
    delta=T[keep][:,free]@scalar-csr_matrix((np.ones(len(keep)),(np.arange(len(keep)),np.arange(len(keep)))),shape=(len(keep),len(keep)))
    error=float(np.max(np.abs(delta.data))) if delta.nnz else 0.
    if error>1e-11:raise ValueError('projected cubic left inverse failed')
    scalar_pivots=np.searchsorted(free,pivots[keep]);lookup=np.full(coarse.N,-1,dtype=int);lookup[keep]=np.arange(len(keep));entities=[]
    for ids in coarse.dofs.nodal_dofs.T:
        if lookup[ids[0]]>=0:entities.append(('vertex',[int(lookup[ids[0]])]))
    for ids in coarse.dofs.edge_dofs.T:
        mapped=lookup[ids]
        if np.any(mapped>=0):
            if np.any(mapped<0):raise ValueError('partial essential cubic edge group')
            entities.append(('edge',mapped.tolist()))
    for ids in coarse.dofs.facet_dofs.T:
        if lookup[ids[0]]>=0:entities.append(('face',[int(lookup[ids[0]])]))
    Z=block_diag((scalar,scalar,scalar),format='csr');Z.sort_indices();nc=len(keep)
    vector_pivots=np.concatenate([scalar_pivots+c*nf for c in range(3)])
    vector_entities=[(kind,[i+c*nc for i in ids]) for c in range(3) for kind,ids in entities]
    meta.update(essential_boundary_vanishing_verified=True,essential_projection_injective=True,maximum_projected_left_inverse_error=error,
        interpolation_array_bytes=Z.indptr.nbytes+Z.indices.nbytes+Z.data.nbytes,interpolation_sha256=storage_sha(Z.indptr,Z.indices,Z.data))
    return HierarchicalCoordinates(Z,vector_pivots,vector_entities,meta)


class HierarchyCholesky:
    def __init__(self,velocity,coordinates,library,cycles=1,ordering='metis'):
        self.coarse=None;self.fine_factor=None;self.closed=False
        if not isinstance(velocity,SymmetricTriangle) or not isinstance(coordinates,HierarchicalCoordinates) or coordinates.nv!=velocity.shape[0]:raise ValueError('invalid exact hierarchy action')
        if not isinstance(cycles,int) or cycles not in (1,2,4,8):raise ValueError('invalid fixed hierarchy sweeps')
        self.velocity=velocity;self.H=coordinates;self.cycles=cycles;A=velocity.upper;Z=coordinates.Z
        self.original_sha256=storage_sha(A.indptr,A.indices,A.data)
        AZ=A@Z+A.T@Z-Z.multiply(A.diagonal()[:,None]);Ac=(Z.T@AZ).tocsr();delta=Ac-Ac.T
        asymmetry=float(np.linalg.norm(delta.data)/max(np.linalg.norm(Ac.data),1e-30))
        if asymmetry>1e-10:raise ValueError('hierarchy Galerkin asymmetry')
        self.coarse_upper=triu((Ac+Ac.T)*.5,format='csr');self.coarse_upper.sort_indices()
        self.coupling=AZ[coordinates.fine].T.tocsr();self.coupling.sort_indices()
        self.fine_upper=A[coordinates.fine][:,coordinates.fine];self.fine_upper.sort_indices()
        del AZ,Ac,delta
        self.coupling_sha256=storage_sha(self.coupling.indptr,self.coupling.indices,self.coupling.data)
        try:
            self.coarse=SharedTriangleFactor(self.coarse_upper,library,ordering)
            self.fine_factor=SharedTriangleFactor(self.fine_upper,library,ordering)
        except Exception:self.close();raise
        self.metadata=dict(kind='hierarchical_cholesky',fixed_sweep_count=cycles,coarse_dofs=coordinates.nc,fine_dofs=len(coordinates.fine),
            hierarchy=coordinates.metadata,coarse_galerkin_relative_asymmetry=asymmetry,coarse_factor=self.coarse.metadata,fine_factor=self.fine_factor.metadata,
            factor_storage_bytes=self.coarse.metadata['symbolic_factor_storage_bytes']+self.fine_factor.metadata['symbolic_factor_storage_bytes'],scaling='none',
            hierarchy_array_bytes=sum(m.indptr.nbytes+m.indices.nbytes+m.data.nbytes for m in (Z,self.coarse_upper,self.coupling,self.fine_upper)),input_preserved_after_factor=self.input_unchanged(),
            scope='fixed SPD exact coupled cubic/quartic block sweep in bijective preconditioner coordinates; original physical mixed equations/full FE authority unchanged')

    def sweep(self,x):
        rhs=self.H.pull(x);nc=self.H.nc
        a=self.coarse.solve(rhs[:nc]);b=self.fine_factor.solve(rhs[nc:]-self.coupling.T@a)
        a-=self.coarse.solve(self.coupling@b)
        return self.H.push(np.r_[a,b])

    def solve(self,x):
        if self.closed:raise ValueError('hierarchy inverse is closed')
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=self.velocity.shape[:1] or not np.all(np.isfinite(rhs)):raise ValueError('invalid hierarchy RHS')
        z=self.sweep(rhs)
        for _ in range(self.cycles-1):z+=self.sweep(rhs-self.velocity@z)
        if not np.all(np.isfinite(z)):raise ValueError('nonfinite hierarchy action')
        return z

    def input_unchanged(self):
        A=self.velocity.upper;B=self.coupling
        return self.original_sha256==storage_sha(A.indptr,A.indices,A.data) and self.H.input_unchanged() and self.coupling_sha256==storage_sha(B.indptr,B.indices,B.data) and self.coarse.input_unchanged() and self.fine_factor.input_unchanged()

    def close(self):
        if self.coarse is not None:self.coarse.close()
        if self.fine_factor is not None:self.fine_factor.close()
        self.closed=True

    def __del__(self):self.close()
