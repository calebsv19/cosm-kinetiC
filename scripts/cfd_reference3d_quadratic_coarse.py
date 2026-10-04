"""Nested macro P2 velocity interpolation; unchanged balanced Galerkin inverse."""
from itertools import combinations
import numpy as np
from scipy.sparse import csr_matrix,block_diag,eye
from cfd_reference3d_coarse_velocity import scalar_macro_interpolation,BalancedCoarseVelocity
from cfd_reference3d_shared_factor import storage_sha


def scalar_macro_quadratic_interpolation(system):
    raw,vertices,linear_meta=scalar_macro_interpolation(system)
    edges=set()
    for macro in range(system.nmacro):
        corners=np.unique(system.mesh.t[:,macro+system.nmacro*np.arange(4)])[:-1]
        edges.update(combinations(map(int,corners),2))
    edges=sorted(edges);edge_columns={edge:len(vertices)+i for i,edge in enumerate(edges)}
    rr=[];cc=[];vv=[];nodal_rows=np.full(len(vertices)+len(edges),-1,dtype=int)
    nodal_rows[:len(vertices)]=system.trace_inverse[system.ub.nodal_dofs[0,vertices]]
    for row in range(system.nt):
        start,end=raw.indptr[row:row+2];cols=raw.indices[start:end];weights=raw.data[start:end]
        for col,w in zip(cols,weights):
            value=w*(2*w-1)
            if value!=0:rr.append(row);cc.append(int(col));vv.append(float(value))
        for i,j in combinations(range(len(cols)),2):
            edge=tuple(sorted((int(vertices[cols[i]]),int(vertices[cols[j]]))))
            column=edge_columns[edge];value=4*weights[i]*weights[j]
            rr.append(row);cc.append(column);vv.append(float(value))
            if len(cols)==2 and weights[i]==weights[j]==.5:
                if nodal_rows[column]>=0 and nodal_rows[column]!=row:raise ValueError('duplicate P2 edge node')
                nodal_rows[column]=row
    P=csr_matrix((vv,(rr,cc)),shape=(system.nt,len(nodal_rows)));P.sort_indices()
    if np.any(nodal_rows<0) or len(np.unique(nodal_rows))!=len(nodal_rows):raise ValueError('incomplete nested P2 nodal carriers')
    delta=P[nodal_rows]-eye(len(nodal_rows),format='csr')
    if delta.nnz and np.max(np.abs(delta.data))!=0:raise ValueError('P2 coarse nodal injection is not identity')
    positions=np.column_stack((system.mesh.p[:,vertices],np.array([(system.mesh.p[:,a]+system.mesh.p[:,b])*.5 for a,b in edges]).T))
    return P,nodal_rows,positions,dict(linear_meta,coarse_degree=2,macro_edge_count=len(edges),coarse_nodal_carriers=len(nodal_rows),edge_nodal_identity_verified=True,
        interpolation_scope='exact nested macro P2 vertex/edge functions at original P4 retained trace nodes; original affine geometry and all mixed equations unchanged')


def macro_quadratic_interpolation(system,nv):
    if nv%3 or nv!=len(system.retained_free)-system.nmacro:raise ValueError('invalid retained velocity count')
    nf=nv//3;free=system.retained_free[:nf]
    for component in range(3):np.testing.assert_array_equal(system.retained_free[component*nf:(component+1)*nf],free+component*system.nt)
    raw,nodal_rows,positions,meta=scalar_macro_quadratic_interpolation(system)
    keep=np.flatnonzero(np.isin(nodal_rows,free))
    if not len(keep):raise ValueError('no free P2 coarse node')
    fixed=np.setdiff1d(np.arange(system.nt),free);boundary=raw[fixed][:,keep]
    if boundary.nnz and np.any(boundary.data!=0):raise ValueError('P2 coarse essential projection does not vanish on fixed fine trace')
    scalar=raw[free][:,keep];rows=np.searchsorted(free,nodal_rows[keep]);delta=scalar[rows]-eye(len(keep),format='csr')
    if delta.nnz and np.any(delta.data!=0):raise ValueError('projected P2 nodal injection lost rank')
    Z=block_diag((scalar,scalar,scalar),format='csr');Z.sort_indices()
    meta.update(coarse_velocity_dofs=Z.shape[1],boundary_excluded_nodal_count=len(nodal_rows)-len(keep),essential_boundary_vanishing_verified=True,
        essential_projection_injective=True,interpolation_nnz=Z.nnz,interpolation_array_bytes=Z.indptr.nbytes+Z.indices.nbytes+Z.data.nbytes,
        interpolation_sha256=storage_sha(Z.indptr,Z.indices,Z.data))
    return Z,meta
