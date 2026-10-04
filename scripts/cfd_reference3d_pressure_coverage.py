"""Sampled approximate pressure Schur coverage; no full spectrum certificate."""
import numpy as np
from cfd_reference3d_pressure_coarse import polynomial_basis
MAX_COLUMNS=64

def reserve(nv,np_):
    if not isinstance(nv,int) or not isinstance(np_,int) or nv<1 or np_<10:raise ValueError('invalid diagnostic size')
    return 8*(8*np_*MAX_COLUMNS+12*nv+8*MAX_COLUMNS**2)

def basis(centers,volumes,mu,length,lo,hi):
    Z=polynomial_basis(centers,volumes,mu,length);root=np.sqrt(volumes/mu)
    raw=[Z[:,j]*root for j in range(10)];names=['polynomial-'+str(j) for j in range(10)]
    x,y,z=(centers/np.array([length,2.,2.])).T
    for k in range(3,9):raw.append(root*np.cos(np.pi*k*x));names.append('axial-cos-'+str(k))
    for axis,t in (('y',y),('z',z)):
        for k in range(1,7):
            for m in (1,2):raw.append(root*np.cos(np.pi*k*x)*np.cos(np.pi*m*t));names.append(f'cos-x{k}-{axis}{m}')
    middle=(lo+hi)/2
    patches=[]
    for a in range(3):
        for side in (-1,1):
            c=middle.copy();c[a]=lo[a]-.1 if side<0 else hi[a]+.1;patches.append(c)
    for a in range(3):
        others=[b for b in range(3) if b!=a]
        for side in ((-1,-1),(-1,1),(1,-1),(1,1)):
            c=middle.copy()
            for b,s in zip(others,side):c[b]=lo[b]-.1 if s<0 else hi[b]+.1
            patches.append(c)
    for j,c in enumerate(patches):raw.append(root*np.exp(-np.sum((centers-c)**2,axis=1)/(2*.18**2)));names.append('body-gaussian-'+str(j))
    cols=[];kept=[];skipped=[]
    for j,(a,name) in enumerate(zip(raw,names)):
        b=a.copy();norm=np.linalg.norm(b)
        for _ in range(2):
            if cols:
                q=np.column_stack(cols);b-=q@(q.T@b)
        if np.linalg.norm(b)<=max(norm,1e-30)*1e-10:
            if j<10:raise ValueError('required polynomial direction lost')
            skipped.append(name);continue
        cols.append(b/np.linalg.norm(b));kept.append(name)
    Q=np.column_stack(cols)
    if len(cols)>MAX_COLUMNS or np.linalg.norm(Q.T@Q-np.eye(len(cols)))>1e-10:raise ValueError('basis budget/orthogonality rejected')
    return Q,dict(proposed_columns=len(raw),columns=len(cols),kept=kept,dependent_columns_rejected=skipped,first_ten_preserved=True)

def analyze(centers,volumes,mu,length,lo,hi,coupling,pressure,velocity,coarse,mesh,sample):
    Q,meta=basis(centers,volumes,mu,length,lo,hi);root=np.sqrt(volumes/mu)
    Y=np.empty_like(Q)
    for j in range(Q.shape[1]):
        p=Q[:,j]/root;Y[:,j]=(coupling.T@velocity(coupling@p)-pressure@p)/root
        if (j+1)%10==0:sample('diagnostic_schur_columns_'+str(j+1))
    result=projected(Q,Y,root,coarse,centers,lo,hi,length)
    cond=np.linalg.cond(mesh.mapping().A.transpose(2,0,1));cell_centers=mesh.p[:,mesh.t].mean(axis=1).T
    outside=np.maximum(np.maximum(lo-cell_centers,cell_centers-hi),0);near=np.linalg.norm(outside,axis=1)<.1
    result.update(basis=meta,geometry=dict(Jacobian_condition_max=float(cond.max()),Jacobian_condition_median=float(np.median(cond)),near_body_cell_count=int(near.sum()),near_body_condition_max=float(cond[near].max()),near_body_condition_median=float(np.median(cond[near]))),diagnostic_reservation_bytes=reserve(coupling.shape[0],len(volumes)))
    return result

def projected(Q,Y,root,coarse,centers,lo,hi,length):
    if Q.shape!=Y.shape or Q.ndim!=2 or len(root)!=len(Q) or np.any(root<=0) or not all(np.all(np.isfinite(a)) for a in (Q,Y,root)):raise ValueError('invalid sampled action')
    K=Q.T@Y;skew=float(np.linalg.norm(K-K.T)/max(np.linalg.norm(K),1e-30));e,U=np.linalg.eigh((K+K.T)/2)
    if skew>=1e-5 or e[0]<=0:raise ValueError('sampled Schur symmetry/positivity rejected')
    H=Q.T@(root[:,None]*coarse.apply(root[:,None]*Q));hs=float(np.linalg.norm(H-H.T)/max(np.linalg.norm(H),1e-30));h,V=np.linalg.eigh((H+H.T)/2)
    if hs>=1e-5 or h[0]<=0:raise ValueError('sampled PC positivity/symmetry rejected')
    HR=(V*np.sqrt(h))@V.T;pe=np.linalg.eigvalsh(HR@((K+K.T)/2)@HR)
    outside=np.maximum(np.maximum(lo-centers,centers-hi),0);near=np.linalg.norm(outside,axis=1)<.2;ends=(centers[:,0]<length*.2)|(centers[:,0]>length*.8);walls=np.min(np.column_stack((centers[:,1:],2-centers[:,1:])),axis=1)<.2
    modes=[]
    for j in range(min(10,len(e))):
        q=Q@U[:,j];y=Y@U[:,j];norm=float(q@q)
        modes.append(dict(projected_eigenvalue=float(e[j]),outside_sample_relative_residual=float(np.linalg.norm(y-e[j]*q)/max(np.linalg.norm(y),1e-30)),ten_polynomial_mass_energy_fraction=float(np.linalg.norm(Q[:,:10].T@q)**2/norm),near_body_mass_energy_fraction=float(q[near]@q[near]/norm),end_mass_energy_fraction=float(q[ends]@q[ends]/norm),wall_mass_energy_fraction=float(q[walls]@q[walls]/norm)))
    return dict(approximate_mass_schur_projected_eigenvalues=e.tolist(),sampled_condition=float(e[-1]/e[0]),projected_relative_skew=skew,balanced_ten_projected_eigenvalues=pe.tolist(),balanced_ten_sampled_condition=float(pe[-1]/pe[0]),balanced_ten_projected_relative_skew=hs,lowest_sampled_modes=modes,physical_accuracy_certified=False,complete_spectrum_certified=False,scope='fixed sampled subspace of approximate Float velocity Schur; actual outside-span residuals reported; not full FE nullspace or inf-sup proof')
