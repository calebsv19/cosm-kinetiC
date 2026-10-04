"""Nested macro-linear Galerkin velocity correction with fixed balanced SPD action."""
import numpy as np
from scipy.sparse import csr_matrix,block_diag,triu,eye
from cfd_reference3d_shared_factor import SharedTriangleFactor,storage_sha
from cfd_reference3d_block_cholesky import RepeatedBlockCholesky


def scalar_macro_interpolation(system):
    rows={};vertices=set();maximum_geometry_error=0.
    for macro,(J,globalu,_,_) in enumerate(system.records):
        cells=macro+system.nmacro*np.arange(4)
        corners=np.unique(system.mesh.t[:,cells])[:-1];vertices.update(map(int,corners))
        ids=globalu[system.ref.trace]
        xyz=system.ub.doflocs[:,ids]
        bary123=np.linalg.solve(J,xyz-system.mesh.p[:,corners[0],None])
        bary=np.vstack((1-bary123.sum(axis=0),bary123))
        quarters=np.rint(4*bary).astype(int)
        error=float(np.max(np.abs(bary-quarters/4)))
        maximum_geometry_error=max(maximum_geometry_error,error)
        if error>1e-11 or np.any(quarters<0) or np.any(quarters.sum(axis=0)!=4):raise ValueError('non-nested affine P4 macro trace geometry')
        for i,gid in enumerate(ids):
            row=int(system.trace_inverse[gid]);entry=tuple((int(corners[j]),int(quarters[j,i])) for j in range(4) if quarters[j,i]!=0)
            if row in rows and rows[row]!=entry:raise ValueError('inconsistent shared macro-linear trace')
            rows[row]=entry
    if set(rows)!=set(range(system.nt)):raise ValueError('incomplete macro-linear trace interpolation')
    vertices=np.array(sorted(vertices),dtype=int);columns={int(v):i for i,v in enumerate(vertices)}
    rr=[];cc=[];vv=[]
    for row,entry in rows.items():
        for vertex,quarter in entry:rr.append(row);cc.append(columns[vertex]);vv.append(quarter/4)
    P=csr_matrix((vv,(rr,cc)),shape=(system.nt,len(vertices)))
    vertex_rows=system.trace_inverse[system.ub.nodal_dofs[0,vertices]]
    if np.any(vertex_rows<0):raise ValueError('macro vertex removed from retained trace')
    delta=P[vertex_rows]-eye(len(vertices),format='csr')
    if delta.nnz and np.max(np.abs(delta.data))!=0:raise ValueError('coarse vertex injection is not identity')
    return P,vertices,dict(maximum_affine_geometry_error=maximum_geometry_error,shared_trace_consistency_verified=True,vertex_injection_identity_verified=True,
        scalar_trace_rows=system.nt,macro_vertex_count=len(vertices),interpolation_scope='exact affine macro P1 functions on original P4 trace nodes; quarter-grid weights verified before essential projection')


def macro_linear_interpolation(system,nv):
    if nv%3 or nv!=len(system.retained_free)-system.nmacro:raise ValueError('invalid retained velocity count')
    nf=nv//3;free=system.retained_free[:nf]
    for component in range(3):np.testing.assert_array_equal(system.retained_free[component*nf:(component+1)*nf],free+component*system.nt)
    raw,vertices,metadata=scalar_macro_interpolation(system)
    vertex_rows=system.trace_inverse[system.ub.nodal_dofs[0,vertices]]
    keep=np.flatnonzero(np.isin(vertex_rows,free));scalar=raw[free][:,keep]
    if not len(keep):raise ValueError('macro-linear coarse space has no free vertex')
    fine_rows=np.searchsorted(free,vertex_rows[keep]);delta=scalar[fine_rows]-eye(len(keep),format='csr')
    if delta.nnz and np.max(np.abs(delta.data))!=0:raise ValueError('projected coarse injection lost rank')
    Z=block_diag((scalar,scalar,scalar),format='csr');Z.sort_indices()
    metadata.update(coarse_velocity_dofs=Z.shape[1],boundary_excluded_vertex_count=len(vertices)-len(keep),
        essential_projection_injective=True,interpolation_nnz=Z.nnz,interpolation_array_bytes=Z.indptr.nbytes+Z.indices.nbytes+Z.data.nbytes,
        interpolation_sha256=storage_sha(Z.indptr,Z.indices,Z.data))
    return Z,metadata


class BalancedCoarseVelocity:
    def __init__(self,velocity,Z,library,cycles=1,ordering='metis',grouping='u_vw',interpolation_metadata=None):
        self.local=None;self.coarse=None;self.closed=False
        if not isinstance(Z,csr_matrix) or Z.shape[0]!=velocity.shape[0] or not 0<Z.shape[1]<Z.shape[0] or not np.all(np.isfinite(Z.data)):raise ValueError('invalid coarse interpolation')
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
            self.local=RepeatedBlockCholesky(velocity,library,cycles,ordering,grouping)
        except Exception:self.close();raise
        self.metadata=dict(kind='balanced_coarse_velocity',fixed_sweep_count=cycles,grouping=grouping,coarse_dofs=Z.shape[1],
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
