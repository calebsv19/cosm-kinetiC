"""Independent preconditioners and diagnostics; physical Stokes operator unchanged."""
import hashlib
import numpy as np
from scipy.sparse import hstack, diags, coo_matrix
from scipy.sparse.linalg import splu
from scipy.linalg import eigh


def array_sha(*arrays):
    h=hashlib.sha256()
    for a in arrays:
        x=np.ascontiguousarray(a);h.update(str((x.shape,x.dtype.str)).encode());h.update(x.tobytes())
    return h.hexdigest()


def matrix_sha(matrix):
    m=matrix.tocsr(copy=True);m.sort_indices();m.eliminate_zeros()
    return array_sha(m.indptr,m.indices,m.data)


class PressureMass:
    def __init__(self,pb,mu):
        shape=np.array([entry[0][0] for entry in pb.basis])
        self.reference=np.einsum('iq,jq,q->ij',shape,shape,pb.W)
        self.inverse=np.linalg.inv(self.reference)
        self.determinant=pb.dx.sum(axis=1)/pb.W.sum()
        self.mu=mu

    def solve(self,x):
        return ((self.inverse@x.reshape(-1,10).T)/self.determinant[None]).T.ravel()

    def apply(self,x):
        return ((self.reference@x.reshape(-1,10).T)*self.determinant[None]).T.ravel()

    def precondition(self,x):return self.mu*self.solve(x)

    def norm(self,x):return float(np.sqrt(max(float(x@self.apply(x)),0.)))

    def dual_norm(self,x):return float(np.sqrt(max(float(x@self.solve(x)),0.)))


def velocity_preconditioner(A,kind):
    if kind=='amg':
        from pyamg import smoothed_aggregation_solver
        pc=smoothed_aggregation_solver(A,max_coarse=100,symmetry='symmetric',
            presmoother=('gauss_seidel',{'sweep':'symmetric'}),
            postsmoother=('gauss_seidel',{'sweep':'symmetric'})).aspreconditioner()
        return lambda x:pc@x, {'kind':kind}
    if kind=='factor':
        factor=splu(A.tocsc(),permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.,
            options={'SymmetricMode':True})
        return factor.solve, {'kind':kind,'factor_nnz':int(factor.L.nnz+factor.U.nnz),
            'factor_fill':float((factor.L.nnz+factor.U.nnz)/A.nnz)}
    raise ValueError(kind)


def macro_pressure_modes(B,A,pb,macro_count,mu):
    """Global macro-constant pressure restriction of diagonal-velocity Schur proxy.

    This covers a global subspace, not the full DG pressure-space spectrum.
    Open natural ends remove the constant-pressure gauge null mode.
    """
    parents=np.tile(np.arange(macro_count),4)
    P=coo_matrix((np.ones(pb.N),(np.arange(pb.N),np.repeat(parents,10))),
                shape=(pb.N,macro_count)).tocsr()
    C=B.T@P;inv_diag=np.tile(1/A.diagonal(),3)
    S=(C.T@diags(inv_diag)@C).tocsr()
    volume=np.bincount(parents,weights=pb.dx.sum(axis=1),minlength=macro_count)
    weight=np.sqrt(mu/volume)
    normalized=diags(weight)@S@diags(weight)
    eigen=eigh(normalized.toarray(),subset_by_index=(0,min(7,macro_count-1)),eigvals_only=True)
    constant=np.ones(pb.N);gradient=B.T@constant
    return {'scope':'global macro-constant subspace of diagonal-velocity Schur proxy; not full-space inf-sup proof',
        'dimension':macro_count,'smallest_eigenvalues':eigen.tolist(),
        'near_null_modes_below_1e_12':int(np.count_nonzero(eigen<1e-12)),
        'constant_pressure_gradient_norm':float(np.linalg.norm(gradient)),
        'constant_mode_proxy_rayleigh':float(gradient@(inv_diag*gradient)/(volume.sum()/mu))}


def residual_diagnostics(K,x,rhs,nv,mass):
    r=K@x-rhs;scale=np.linalg.norm(rhs)
    return {'true_residual':float(np.linalg.norm(r)/scale),
        'momentum_relative_to_rhs':float(np.linalg.norm(r[:nv])/scale),
        'continuity_relative_to_rhs':float(np.linalg.norm(r[nv:])/scale),
        'continuum_divergence_l2_unit_response':mass.dual_norm(r[nv:]),
        'pressure_mass_norm':mass.norm(x[nv:])}
