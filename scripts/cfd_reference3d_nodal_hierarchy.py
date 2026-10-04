"""Bijective cubic/nodal-vanishing quartic coordinates; physical operator unchanged."""
import numpy as np
from scipy.sparse import csr_matrix,block_diag,eye,triu
from cfd_reference3d_cubic_coarse import scalar_macro_cubic_interpolation
from cfd_reference3d_hierarchical import HierarchicalCoordinates,macro_hierarchy,HierarchyCholesky
from cfd_reference3d_shared_factor import SharedTriangleFactor,storage_sha
from cfd_reference3d_triangle import SymmetricTriangle


def maximum(matrix):return float(np.max(np.abs(matrix.data))) if matrix.nnz else 0.

class NodalCoordinates(HierarchicalCoordinates):
    def __init__(self,unit,T):
        if not isinstance(unit,HierarchicalCoordinates) or not unit.input_unchanged():raise ValueError('unverified unit coordinate certificate')
        if not isinstance(T,csr_matrix) or T.shape!=(unit.nc,unit.nv) or not np.all(np.isfinite(T.data)):raise ValueError('invalid cubic nodal evaluation')
        error=maximum(T@unit.Z-eye(unit.nc,format='csr'))
        if error>1e-11:raise ValueError('cubic evaluation is not an interpolation left inverse')
        self.unit=unit;self.Z=unit.Z;self.pivots=unit.pivots;self.fine=unit.fine;self.nv=unit.nv;self.nc=unit.nc;self.T=T
        self.W=T[:,self.fine].tocsr();self.W.sort_indices()
        E=csr_matrix((np.ones(len(self.fine)),(self.fine,np.arange(len(self.fine)))),shape=(self.nv,len(self.fine)))
        self.Q=(E-self.Z@self.W).tocsr();self.Q.eliminate_zeros();self.Q.sort_indices()
        vanished=maximum(T@self.Q)
        if vanished>1e-11:raise ValueError('quartic complement does not vanish at cubic nodes')
        self.metadata=dict(unit.metadata,nodal_left_inverse_verified=True,maximum_left_inverse_error=error,nodal_complement_vanishing_verified=True,maximum_nodal_complement_error=vanished,
            exact_triangular_coordinate_change_verified=True,coordinate_scope='bijective [Z,(I-ZT)E]=[Z,E][I,-TE;0,I]; certified unit pivots plus nonsingular triangular coordinate change; all physical variables retained',
            complement_nnz=self.Q.nnz,evaluation_nnz=T.nnz,complement_array_bytes=self.Q.indptr.nbytes+self.Q.indices.nbytes+self.Q.data.nbytes)
        self.sha256=self.digest()

    def digest(self):return storage_sha(*(a for m in (self.Z,self.T,self.W,self.Q) for a in (m.indptr,m.indices,m.data)),self.pivots,self.fine)
    def pull(self,x):return np.r_[self.Z.T@x,self.Q.T@x]
    def push(self,x):return self.Z@x[:self.nc]+self.Q@x[self.nc:]
    def input_unchanged(self):return self.unit.input_unchanged() and self.sha256==self.digest()


def macro_nodal_hierarchy(system,nv):
    unit=macro_hierarchy(system,nv);raw,T,positions,meta=scalar_macro_cubic_interpolation(system)
    nf=nv//3;free=system.retained_free[:nf];fixed=np.setdiff1d(np.arange(system.nt),free)
    keep=np.setdiff1d(np.arange(raw.shape[1]),np.unique(raw[fixed].indices))
    scalar=raw[free][:,keep];Z=block_diag((scalar,scalar,scalar),format='csr');Z.sort_indices()
    if Z.shape!=unit.Z.shape or maximum(Z-unit.Z)>1e-12:raise ValueError('nodal and pivot interpolation spaces disagree')
    evaluate=block_diag((T[keep][:,free],)*3,format='csr');evaluate.sort_indices()
    return NodalCoordinates(unit,evaluate)


class NodalHierarchyCholesky(HierarchyCholesky):
    def __init__(self,velocity,coordinates,library,cycles=1,ordering='metis'):
        self.coarse=None;self.fine_factor=None;self.closed=False
        if not isinstance(velocity,SymmetricTriangle) or not isinstance(coordinates,NodalCoordinates) or coordinates.nv!=velocity.shape[0]:raise ValueError('invalid exact nodal hierarchy action')
        if not isinstance(cycles,int) or cycles not in (1,2,4,8):raise ValueError('invalid fixed nodal hierarchy sweeps')
        self.velocity=velocity;self.H=coordinates;self.cycles=cycles;A=velocity.upper;Z=coordinates.Z;Q=coordinates.Q
        self.original_sha256=storage_sha(A.indptr,A.indices,A.data)
        AZ=A@Z+A.T@Z-Z.multiply(A.diagonal()[:,None]);Ac=(Z.T@AZ).tocsr();delta=Ac-Ac.T
        asymmetry=float(np.linalg.norm(delta.data)/max(np.linalg.norm(Ac.data),1e-30))
        if asymmetry>1e-10:raise ValueError('nodal coarse Galerkin asymmetry')
        self.coarse_upper=triu((Ac+Ac.T)*.5,format='csr');self.coarse_upper.sort_indices()
        self.coupling=(AZ.T@Q).tocsr();self.coupling.sort_indices();del AZ,Ac,delta
        AQ=A@Q+A.T@Q-Q.multiply(A.diagonal()[:,None]);Af=(Q.T@AQ).tocsr();delta=Af-Af.T
        fine_asymmetry=float(np.linalg.norm(delta.data)/max(np.linalg.norm(Af.data),1e-30))
        if fine_asymmetry>1e-10:raise ValueError('nodal fine Galerkin asymmetry')
        self.fine_upper=triu((Af+Af.T)*.5,format='csr');self.fine_upper.sort_indices();del AQ,Af,delta
        self.coupling_sha256=storage_sha(self.coupling.indptr,self.coupling.indices,self.coupling.data)
        try:
            self.coarse=SharedTriangleFactor(self.coarse_upper,library,ordering)
            self.fine_factor=SharedTriangleFactor(self.fine_upper,library,ordering)
        except Exception:self.close();raise
        self.metadata=dict(kind='nodal_hierarchy_cholesky',fixed_sweep_count=cycles,coarse_dofs=coordinates.nc,fine_dofs=len(coordinates.fine),hierarchy=coordinates.metadata,
            coarse_galerkin_relative_asymmetry=asymmetry,fine_galerkin_relative_asymmetry=fine_asymmetry,coarse_factor=self.coarse.metadata,fine_factor=self.fine_factor.metadata,
            factor_storage_bytes=self.coarse.metadata['symbolic_factor_storage_bytes']+self.fine_factor.metadata['symbolic_factor_storage_bytes'],scaling='none',
            hierarchy_array_bytes=sum(m.indptr.nbytes+m.indices.nbytes+m.data.nbytes for m in (Z,Q,coordinates.T,coordinates.W,self.coarse_upper,self.coupling,self.fine_upper)),input_preserved_after_factor=self.input_unchanged(),
            scope='fixed SPD exact coupled cubic/nodal-vanishing quartic sweep in bijective preconditioner coordinates; original mixed operator and full FE authority unchanged')
