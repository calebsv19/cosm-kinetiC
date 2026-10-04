"""Local macro pressure-rank diagnostics for the unchanged Alfeld Stokes pair.

Together with an injective global macro-constant restriction, rank 39 on every
40-pressure macro rules out an additional exact B-transpose null direction on
this assembled mesh. This is not a mesh-independent inf-sup bound or force proof.
"""
import numpy as np
from scipy.linalg import block_diag,eigh
from scipy.sparse import hstack


def local_macro_pressure_modes(mesh,ub,pb,A,blocks,mass,macro_count,mu):
    assert mesh.nelements==4*macro_count
    minima=[];maxima=[];null_values=[];constant_errors=[];rank_counts={}
    for parent in range(macro_count):
        cells=parent+macro_count*np.arange(4)
        facets,counts=np.unique(mesh.t2f[:,cells],return_counts=True)
        boundary=ub.get_dofs(facets=facets[counts==1]).all()
        bubble=np.setdiff1d(np.unique(ub.dofs.element_dofs[:,cells]),boundary)
        assert len(bubble)==15
        rows=pb.dofs.element_dofs[:,cells].T.ravel()
        localA=A[bubble][:,bubble].toarray()
        localB=hstack([b[rows][:,bubble] for b in blocks],format='csr').toarray()
        schur=sum(localB[:,a*15:(a+1)*15]@np.linalg.solve(localA,localB[:,a*15:(a+1)*15].T) for a in range(3))
        pressure_mass=block_diag(*[mass.reference*mass.determinant[c]/mu for c in cells])
        eigen=eigh(schur,pressure_mass,eigvals_only=True)
        rank=int(np.count_nonzero(eigen>max(eigen[-1],1.)*1e-10))
        rank_counts[rank]=rank_counts.get(rank,0)+1
        minima.append(float(eigen[1]));maxima.append(float(eigen[-1]));null_values.append(float(eigen[0]))
        constant_errors.append(float(np.linalg.norm(localB.T@np.ones(40))/max(np.linalg.norm(localB),1e-30)))
    return {'scope':'all local macro bubble Schur ranks; combines with global constants for assembled pressure nullity, not a uniform inf-sup certificate',
        'macro_count':macro_count,'pressure_dofs_per_macro':40,'bubble_velocity_dofs_per_macro':45,
        'rank_counts':rank_counts,'all_mean_zero_modes_detected':rank_counts=={39:macro_count},
        'minimum_positive_generalized_eigenvalue':min(minima),
        'maximum_positive_generalized_eigenvalue':max(maxima),
        'maximum_absolute_constant_eigenvalue':max(abs(v) for v in null_values),
        'maximum_constant_gradient_relative_error':max(constant_errors)}


def macro_pressure_preconditioner(mesh,ub,pb,A,blocks,mass,free,macro_count,mu):
    """SPD additive local-mean-zero and global-constant Schur inverse actions.

    Only the preconditioner changes. Local velocity bubbles use their exact
    scalar stiffness; global constants use the diagonal-velocity Schur proxy.
    The rank-one term completes each local solve and is projected out, so it
    adds no pressure penalty to the Stokes operator or to its physical field.
    """
    from scipy.linalg import cho_factor,cho_solve
    from scipy.sparse import coo_matrix,diags
    from scipy.sparse.linalg import splu
    inverse=np.empty((macro_count,40,40));indices=np.empty((macro_count,40),dtype=np.int32)
    constant=np.ones(40)
    for parent in range(macro_count):
        cells=parent+macro_count*np.arange(4)
        facets,counts=np.unique(mesh.t2f[:,cells],return_counts=True)
        boundary=ub.get_dofs(facets=facets[counts==1]).all()
        bubble=np.setdiff1d(np.unique(ub.dofs.element_dofs[:,cells]),boundary)
        assert len(bubble)==15
        rows=pb.dofs.element_dofs[:,cells].T.ravel();indices[parent]=rows
        localA=A[bubble][:,bubble].toarray()
        localB=hstack([b[rows][:,bubble] for b in blocks],format='csr').toarray()
        schur=sum(localB[:,a*15:(a+1)*15]@np.linalg.solve(localA,localB[:,a*15:(a+1)*15].T) for a in range(3))
        pressure_mass=block_diag(*[mass.reference*mass.determinant[c]/mu for c in cells])
        weight=pressure_mass@constant;volume=float(constant@weight)
        projection=np.eye(40)-np.outer(weight,constant)/volume
        complete=schur+np.outer(weight,weight)/volume
        local_inverse=cho_solve(cho_factor(complete,lower=True),projection)
        inverse[parent]=projection.T@local_inverse
    assert np.array_equal(np.sort(indices.ravel()),np.arange(pb.N))
    parents=np.tile(np.arange(macro_count),4)
    P=coo_matrix((np.ones(pb.N),(np.arange(pb.N),np.repeat(parents,10))),shape=(pb.N,macro_count)).tocsr()
    B=hstack([b[:,free] for b in blocks],format='csr');C=B.T@P
    diagonal=np.tile(1/A[free][:,free].diagonal(),3)
    coarse=(C.T@diags(diagonal)@C).tocsc()
    factor=splu(coarse,permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.,options={'SymmetricMode':True})
    def apply(x):
        local=x[indices]
        result=np.einsum('mij,mj->mi',inverse,local)+factor.solve(local.sum(axis=1))[:,None]
        out=np.empty_like(x);out[indices.ravel()]=result.ravel()
        return out
    metadata={'kind':'macro_schur','macro_count':macro_count,'local_inverse_bytes':int(inverse.nbytes),
        'coarse_nnz':int(coarse.nnz),'coarse_factor_nnz':int(factor.L.nnz+factor.U.nnz),
        'scope':'additive local mean-zero Schur inverses plus global macro-constant diagonal-velocity Schur inverse; unchanged physical operator'}
    return apply,metadata


def patch_pressure_preconditioner(mesh,ub,pb,A,blocks,mass,free,macro_count,mu):
    """SPD two-level block Jacobi of the global diagonal-velocity Schur proxy.

    Each pressure block retains coupling to every free velocity DOF touching the
    macro, including interface DOFs. This avoids the severe bubble-only Schur
    underestimate observed in the first additive candidate.
    """
    from scipy.linalg import cho_factor,cho_solve
    from scipy.sparse import coo_matrix,diags
    from scipy.sparse.linalg import splu
    B=hstack([b[:,free] for b in blocks],format='csr')
    diagonal=np.tile(1/A[free][:,free].diagonal(),3)
    inverse=np.empty((macro_count,40,40));indices=np.empty((macro_count,40),dtype=np.int32)
    for parent in range(macro_count):
        cells=parent+macro_count*np.arange(4)
        rows=pb.dofs.element_dofs[:,cells].T.ravel();indices[parent]=rows
        local=B[rows];schur=(local.multiply(diagonal)@local.T).toarray()
        inverse[parent]=cho_solve(cho_factor(schur,lower=True),np.eye(40))
    assert np.array_equal(np.sort(indices.ravel()),np.arange(pb.N))
    parents=np.tile(np.arange(macro_count),4)
    P=coo_matrix((np.ones(pb.N),(np.arange(pb.N),np.repeat(parents,10))),shape=(pb.N,macro_count)).tocsr()
    C=B.T@P;coarse=(C.T@diags(diagonal)@C).tocsc()
    factor=splu(coarse,permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.,options={'SymmetricMode':True})
    def apply(x):
        local=x[indices]
        result=np.einsum('mij,mj->mi',inverse,local)+factor.solve(local.sum(axis=1))[:,None]
        out=np.empty_like(x);out[indices.ravel()]=result.ravel()
        return out
    return apply,{'kind':'patch_schur','macro_count':macro_count,'local_inverse_bytes':int(inverse.nbytes),
        'coarse_nnz':int(coarse.nnz),'coarse_factor_nnz':int(factor.L.nnz+factor.U.nnz),
        'scope':'full-interface macro pressure blocks and global constants of diagonal-velocity Schur proxy; unchanged physical operator'}
