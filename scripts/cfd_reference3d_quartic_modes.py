"""All-macro bubble pressure ranks plus global constant restriction for P4/DG-P3.

Detects assembled extra pressure null modes; not a full-space condition bound or
mesh-independent inf-sup certificate. No physical matrix or pressure field changes.
"""
import numpy as np
from scipy.linalg import block_diag,eigh
from scipy.sparse import coo_matrix,diags,hstack


def local_modes(mesh,ub,pb,A,blocks,mass,macro_count,mu):
    assert mesh.nelements==4*macro_count
    minima=[];maxima=[];null=[];errors=[];counts={}
    for parent in range(macro_count):
        cells=parent+macro_count*np.arange(4)
        faces,multiplicity=np.unique(mesh.t2f[:,cells],return_counts=True)
        boundary=ub.get_dofs(facets=faces[multiplicity==1]).all()
        bubble=np.setdiff1d(np.unique(ub.dofs.element_dofs[:,cells]),boundary)
        assert len(bubble)==35
        rows=pb.dofs.element_dofs[:,cells].T.ravel()
        localA=A[bubble][:,bubble].toarray()
        coupling=hstack([b[rows][:,bubble] for b in blocks],format='csr').toarray()
        schur=sum(coupling[:,a*35:(a+1)*35]@np.linalg.solve(localA,coupling[:,a*35:(a+1)*35].T) for a in range(3))
        pressure_mass=block_diag(*[mass.reference*mass.determinant[c]/mu for c in cells])
        eigen=eigh(schur,pressure_mass,eigvals_only=True)
        rank=int(np.count_nonzero(eigen>max(eigen[-1],1.)*1e-10))
        counts[rank]=counts.get(rank,0)+1
        minima.append(float(eigen[1]));maxima.append(float(eigen[-1]));null.append(float(eigen[0]))
        errors.append(float(np.linalg.norm(coupling.T@np.ones(80))/max(np.linalg.norm(coupling),1e-30)))
    return dict(scope='all macro bubble Schur ranks; combines with injective global constants for assembled nullity, not a uniform inf-sup certificate',
        macro_count=macro_count,pressure_dofs_per_macro=80,bubble_velocity_dofs_per_macro=105,
        rank_counts=counts,all_mean_zero_modes_detected=counts=={79:macro_count},
        minimum_positive_generalized_eigenvalue=min(minima),maximum_positive_generalized_eigenvalue=max(maxima),
        maximum_absolute_constant_eigenvalue=max(abs(v) for v in null),maximum_constant_gradient_relative_error=max(errors))


def global_modes(B,A,mass,macro_count,mu):
    parents=np.tile(np.arange(macro_count),4)
    assert len(mass.determinant)==4*macro_count
    npres=20*len(parents)
    P=coo_matrix((np.ones(npres),(np.arange(npres),np.repeat(parents,20))),shape=(npres,macro_count)).tocsr()
    C=B.T@P;inverse_diagonal=np.tile(1/A.diagonal(),3)
    schur=(C.T@diags(inverse_diagonal)@C).tocsr()
    volume=np.bincount(parents,weights=mass.determinant/6,minlength=macro_count)
    scale=diags(np.sqrt(mu/volume));normalized=scale@schur@scale
    eigen=eigh(normalized.toarray(),subset_by_index=(0,min(7,macro_count-1)),eigvals_only=True)
    constant_gradient=B.T@np.ones(npres)
    return dict(scope='global macro-constant diagonal-velocity Schur proxy restriction, not full-space condition bound',
        dimension=macro_count,smallest_eigenvalues=eigen.tolist(),near_null_modes_below_1e_12=int(np.count_nonzero(eigen<1e-12)),
        constant_pressure_gradient_norm=float(np.linalg.norm(constant_gradient)),
        constant_mode_proxy_rayleigh=float(constant_gradient@(inverse_diagonal*constant_gradient)/(volume.sum()/mu)))
