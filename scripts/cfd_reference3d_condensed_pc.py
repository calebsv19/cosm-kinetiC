"""SPD incomplete coupled-velocity inverse; only the preconditioner changes."""
import numpy as np
from scipy.sparse import diags
from scipy.sparse.linalg import spilu,spsolve_triangular


def symmetric_ilu(matrix,drop_tol=1e-4,fill_factor=8.):
    factor=spilu(matrix.tocsc(),drop_tol=drop_tol,fill_factor=fill_factor,
        permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.,options={'SymmetricMode':True})
    if not np.array_equal(factor.perm_r,factor.perm_c):
        raise ValueError('symmetric incomplete factor requires identical row/column permutations')
    order=np.argsort(factor.perm_r);L=factor.L.tocsc();D=factor.U.diagonal().copy()
    if not np.all(np.isfinite(D)) or np.min(D)<=0:
        raise ValueError('incomplete LDL inverse requires positive finite diagonal; no flooring allowed')
    metadata=dict(kind='symmetric_ilu',drop_tol=drop_tol,fill_factor=fill_factor,
        factor_nnz=int(L.nnz+len(D)),lower_factor_nnz=int(L.nnz),temporary_ilu_nnz=int(factor.L.nnz+factor.U.nnz),
        minimum_diagonal=float(np.min(D)),maximum_diagonal=float(np.max(D)),
        scope='P^T L D L^T P inverse built from incomplete LU lower factor and positive diagonal; exactly SPD preconditioner, unchanged physical operator')
    del factor
    def apply(x):
        y=spsolve_triangular(L,x[order],lower=True,unit_diagonal=True,overwrite_A=True,overwrite_b=True)
        z=spsolve_triangular(L.T,y/D,lower=False,unit_diagonal=True,overwrite_A=True,overwrite_b=True)
        result=np.empty_like(x);result[order]=z
        return result
    return apply,metadata


def coupled_ilu(matrix,drop_tol=1e-4,fill_factor=5.):
    """Fixed equilibrated incomplete inverse for GMRES; original matrix untouched."""
    diagonal=matrix.diagonal()
    if not np.all(np.isfinite(diagonal)) or np.min(diagonal)<=0:
        raise ValueError('velocity equilibration requires positive finite physical diagonal')
    scale=1/np.sqrt(diagonal);D=diags(scale)
    factor=spilu((D@matrix@D).tocsc(),drop_tol=drop_tol,fill_factor=fill_factor,
        permc_spec='MMD_AT_PLUS_A',diag_pivot_thresh=0.)
    metadata=dict(kind='coupled_ilu_gmres',drop_tol=drop_tol,fill_factor=fill_factor,
        factor_storage_nnz=int(factor.nnz),row_pivot_threshold=0.,diagonal_equilibration=True,
        scope='fixed equilibrated incomplete LU of coupled velocity block for GMRES; unchanged physical operator')
    def apply(x):
        result=scale*factor.solve(scale*x)
        if not np.all(np.isfinite(result)):raise ValueError('nonfinite incomplete inverse action')
        return result
    return apply,metadata


def component_ssor(matrix,factors,cycles=1,action=None):
    """Exact block SSOR inverse from SPD principal factors; fixed SPD action."""
    n=matrix.shape[0]//3;assert matrix.shape==(3*n,3*n) and len(factors)==3
    assert cycles in (1,2,4)
    if cycles>1 and action is None:action=matrix.dot
    upper={(a,b):matrix[a*n:(a+1)*n,b*n:(b+1)*n].tocsr() for a in range(3) for b in range(a+1,3)}
    def sweep(x):
        forward=[]
        for a in range(3):
            rhs=x[a*n:(a+1)*n].copy()
            for b in range(a):rhs-=upper[b,a].T@forward[b]
            forward.append(factors[a].solve(rhs))
        result=[None]*3
        for a in reversed(range(3)):
            correction=np.zeros(n)
            for b in range(a+1,3):correction+=upper[a,b]@result[b]
            result[a]=forward[a]-factors[a].solve(correction)
        return np.concatenate(result)
    def apply(x):
        z=sweep(x)
        for _ in range(cycles-1):z+=sweep(x-action(z))
        return z
    return apply


def coupled_amg(matrix):
    """Fixed vector-block V cycle for GMRES with three translation candidates."""
    from pyamg import smoothed_aggregation_solver
    n=matrix.shape[0]//3;assert matrix.shape==(3*n,3*n)
    order=np.arange(3*n).reshape(3,n).T.ravel()
    A=matrix[order][:,order].tobsr(blocksize=(3,3))
    candidates=np.tile(np.eye(3),(n,1))
    hierarchy=smoothed_aggregation_solver(A,B=candidates,symmetry='symmetric',
        smooth=('energy',{'degree':2}),max_coarse=30,
        presmoother=('block_gauss_seidel',{'sweep':'symmetric'}),
        postsmoother=('block_gauss_seidel',{'sweep':'symmetric'}))
    inverse=hierarchy.aspreconditioner(cycle='V')
    metadata=dict(kind='coupled_amg_gmres',levels=len(hierarchy.levels),
        operator_complexity=float(hierarchy.operator_complexity()),
        hierarchy_operator_nnz=sum(int(level.A.nnz) for level in hierarchy.levels),
        candidate_modes=3,blocksize=3,
        scope='fixed vector-block AMG V-cycle with translation candidates and energy interpolation; unchanged physical operator')
    def apply(x):
        result=np.empty_like(x);result[order]=inverse@x[order]
        if not np.all(np.isfinite(result)):raise ValueError('nonfinite multigrid inverse action')
        return result
    return apply,metadata
