"""Nested macro P3 trace interpolation and explicit sparse evaluation left inverse."""
import numpy as np
from scipy.sparse import csr_matrix,block_diag,eye,triu
from skfem import Basis,MeshTet
from cfd_reference3d_p3 import ElementTetP3,require_sorted
from cfd_reference3d_p4 import ElementTetP4
from cfd_reference3d_shared_factor import SharedTriangleFactor,storage_sha
from cfd_reference3d_block_cholesky import RepeatedBlockCholesky
from cfd_reference3d_component_cholesky import RepeatedComponentCholesky
from cfd_reference3d_coarse_velocity import BalancedCoarseVelocity


def scalar_macro_cubic_interpolation(system):
    macros=[np.unique(system.mesh.t[:,m+system.nmacro*np.arange(4)])[:-1] for m in range(system.nmacro)]
    vertices=np.unique(np.concatenate(macros));indices=np.full(system.mesh.nvertices,-1,dtype=int);indices[vertices]=np.arange(len(vertices))
    parent=MeshTet(system.mesh.p[:,vertices],indices[np.array(macros).T]);require_sorted(parent)
    elem=ElementTetP3();quartic=ElementTetP4();coarse=Basis(parent,elem,quadrature=system.ub.quadrature,elements=np.array([0]))
    X=system.ref.ub.doflocs[:,system.ref.trace]
    if np.max(np.abs(4*X-np.rint(4*X)))>1e-11:raise ValueError('non-nested P4 macro trace')
    X=np.rint(4*X)/4
    local=np.array([elem.lbasis(X,i)[0] for i in range(20)]).T
    outer_ids=[]
    for point in X.T:
        matches=np.flatnonzero(np.all(quartic.doflocs==point,axis=1))
        if len(matches)!=1:raise ValueError('invalid outer quartic trace node')
        outer_ids.append(int(matches[0]))
    evaluate=np.array([quartic.lbasis(elem.doflocs.T,i)[0] for i in outer_ids]).T
    if np.max(np.abs(evaluate@local-np.eye(20)))>1e-12:raise ValueError('local cubic trace left inverse failed')
    rows={};evaluations={};maximum_geometry_error=0.
    for macro,(J,globalu,_,_) in enumerate(system.records):
        fine_ids=system.trace_inverse[globalu[system.ref.trace]];coarse_ids=coarse.dofs.element_dofs[:,macro]
        actual=np.linalg.solve(J,system.ub.doflocs[:,globalu[system.ref.trace]]-system.mesh.p[:,macros[macro][0],None])
        error=float(np.max(np.abs(actual-X)));maximum_geometry_error=max(maximum_geometry_error,error)
        if error>1e-11:raise ValueError('inconsistent affine macro trace orientation')
        for i,fine_id in enumerate(fine_ids):
            entry=tuple(sorted((int(coarse_ids[j]),float(local[i,j])) for j in range(20) if local[i,j]!=0))
            row=int(fine_id)
            if row in rows and rows[row]!=entry:raise ValueError('inconsistent shared cubic edge/face interpolation')
            rows[row]=entry
        for j,coarse_id in enumerate(coarse_ids):
            row=int(coarse_id)
            if row not in evaluations:evaluations[row]=tuple((int(fine_ids[i]),float(evaluate[j,i])) for i in range(len(fine_ids)) if evaluate[j,i]!=0)
    if set(rows)!=set(range(system.nt)) or set(evaluations)!=set(range(coarse.N)):raise ValueError('incomplete cubic trace maps')
    def matrix(entries,shape):
        rr=[];cc=[];values=[]
        for row,entry in entries.items():
            for col,value in entry:rr.append(row);cc.append(col);values.append(value)
        result=csr_matrix((values,(rr,cc)),shape=shape);result.sort_indices();return result
    P=matrix(rows,(system.nt,coarse.N));T=matrix(evaluations,(coarse.N,system.nt))
    delta=T@P-eye(coarse.N,format='csr');error=float(np.max(np.abs(delta.data))) if delta.nnz else 0.
    if error>1e-11:raise ValueError('global cubic trace left inverse failed')
    return P,T,coarse.doflocs,dict(coarse_degree=3,maximum_affine_geometry_error=maximum_geometry_error,shared_trace_orientation_verified=True,
        sparse_left_inverse_verified=True,maximum_left_inverse_error=error,coarse_scalar_dofs=int(coarse.N),macro_vertex_count=len(vertices),
        interpolation_scope='continuous macro P3 edge/face functions at original P4 trace; sparse quartic evaluation proves injectivity')


def macro_cubic_interpolation(system,nv):
    if nv%3 or nv!=len(system.retained_free)-system.nmacro:raise ValueError('invalid retained velocity count')
    nf=nv//3;free=system.retained_free[:nf]
    for component in range(3):np.testing.assert_array_equal(system.retained_free[component*nf:(component+1)*nf],free+component*system.nt)
    raw,T,positions,meta=scalar_macro_cubic_interpolation(system)
    fixed=np.setdiff1d(np.arange(system.nt),free)
    keep=np.setdiff1d(np.arange(raw.shape[1]),np.unique(raw[fixed].indices))
    if not len(keep):raise ValueError('no free cubic coarse node')
    scalar=raw[free][:,keep];boundary=raw[fixed][:,keep]
    if boundary.nnz and np.any(boundary.data!=0):raise ValueError('cubic coarse space violates essential boundary')
    delta=T[keep][:,free]@scalar-eye(len(keep),format='csr');error=float(np.max(np.abs(delta.data))) if delta.nnz else 0.
    if error>1e-11:raise ValueError('projected cubic left inverse lost rank')
    Z=block_diag((scalar,scalar,scalar),format='csr');Z.sort_indices()
    meta.update(coarse_velocity_dofs=Z.shape[1],boundary_excluded_nodal_count=raw.shape[1]-len(keep),essential_boundary_vanishing_verified=True,
        essential_projection_injective=True,maximum_projected_left_inverse_error=error,interpolation_nnz=Z.nnz,
        interpolation_array_bytes=Z.indptr.nbytes+Z.indices.nbytes+Z.data.nbytes,interpolation_sha256=storage_sha(Z.indptr,Z.indices,Z.data))
    return Z,meta


class BalancedCubicVelocity(BalancedCoarseVelocity):
    def __init__(self,velocity,Z,library,cycles=1,ordering='metis',grouping='u_vw',interpolation_metadata=None,local_kind='components'):
        self.local=None;self.coarse=None;self.closed=False
        if not isinstance(Z,csr_matrix) or Z.shape[0]!=velocity.shape[0] or not 0<Z.shape[1]<Z.shape[0] or not np.all(np.isfinite(Z.data)):raise ValueError('invalid coarse interpolation')
        if local_kind not in ('components','two_block'):raise ValueError('invalid local sweep kind')
        self.velocity=velocity;self.Z=Z
        self.original_sha256=storage_sha(velocity.upper.indptr,velocity.upper.indices,velocity.upper.data)
        self.interpolation_sha256=storage_sha(Z.indptr,Z.indices,Z.data)
        upper=velocity.upper
        AZ=upper@Z+upper.T@Z-Z.multiply(upper.diagonal()[:,None])
        Ac=(Z.T@AZ).tocsr();asymmetry=Ac-Ac.T
        relative=float(np.linalg.norm(asymmetry.data)/max(np.linalg.norm(Ac.data),1e-30))
        if relative>1e-10:raise ValueError('coarse Galerkin asymmetry')
        # Only restore floating-point symmetry of the exact Galerkin form.
        self.coarse_upper=triu((Ac+Ac.T)*.5,format='csr');self.coarse_upper.sort_indices()
        del AZ,Ac,asymmetry
        try:
            self.coarse=SharedTriangleFactor(self.coarse_upper,library,ordering)
            self.local=RepeatedComponentCholesky(velocity,library,cycles,ordering) if local_kind=='components' else RepeatedBlockCholesky(velocity,library,cycles,ordering,grouping)
        except Exception:self.close();raise
        self.metadata=dict(kind='balanced_cubic_velocity',local_kind=local_kind,fixed_sweep_count=cycles,grouping=grouping,coarse_dofs=Z.shape[1],
            interpolation=interpolation_metadata or {},coarse_galerkin_relative_asymmetry=relative,coarse_factor=self.coarse.metadata,local_sweep=self.local.metadata,
            factor_storage_bytes=self.coarse.metadata['symbolic_factor_storage_bytes']+self.local.metadata['factor_storage_bytes'],
            scaling='none',input_preserved_after_factor=self.input_unchanged(),
            scope='fixed balanced SPD coarse Galerkin correction and exact coupled principal block sweep; unchanged physical mixed operator and full FE authority')

    def coarse_action(self,x):return self.Z@self.coarse.solve(self.Z.T@x)

    def solve(self,x):
        if self.closed:raise ValueError('balanced inverse is closed')
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=self.velocity.shape[:1] or not np.all(np.isfinite(rhs)):raise ValueError('invalid balanced RHS')
        coarse=self.coarse_action(rhs)
        local=self.local.solve(rhs-self.velocity@coarse)
        z=coarse+local-self.coarse_action(self.velocity@local)
        if not np.all(np.isfinite(z)):raise ValueError('nonfinite balanced inverse')
        return z

    def input_unchanged(self):
        a=self.velocity.upper
        return self.original_sha256==storage_sha(a.indptr,a.indices,a.data) and self.interpolation_sha256==storage_sha(self.Z.indptr,self.Z.indices,self.Z.data) and self.local.input_unchanged() and self.coarse.input_unchanged()

    def close(self):
        if self.local is not None:self.local.close()
        if self.coarse is not None:self.coarse.close()
        self.closed=True

    def __del__(self):self.close()
