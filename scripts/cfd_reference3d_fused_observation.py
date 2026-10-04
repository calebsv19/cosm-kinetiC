"""Identical bounded volume integrals with one shared exact basis/gradient pass."""
import numpy as np
from skfem import FacetBasis
from cfd_reference3d_chunked import cell_basis


def fused_metrics_and_consistency(mesh,ub,pb,u,p,mu,lo,hi,chunk_size):
    fb=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['body'],intorder=4)
    g=np.stack([fb.interpolate(v).grad for v in u]);div=np.einsum('ii...->...',g)
    normal=fb.normals;normal_load=-2*mu*np.einsum('ii...->i...',g)*normal
    div_load=-2*mu*div*normal;x=fb.global_coordinates().mean(axis=2);faces=[]
    for axis in range(3):
        for side,plane in enumerate((lo[axis],hi[axis])):
            mask=np.isclose(x[axis],plane)[:,None]
            faces.append({'axis':axis,'side':side,
                'normal_load_n':np.sum(normal_load*fb.dx*mask,axis=(1,2)).tolist(),
                'divergence_load_n':np.sum(div_load*fb.dx*mask,axis=(1,2)).tolist(),
                'boundary_divergence_l2_s_inv_m':float(np.sum(div**2*fb.dx*mask))**.5})
    etas=[];lifts=[]
    distance=np.linalg.norm(np.maximum(np.maximum(lo[:,None]-ub.doflocs,ub.doflocs-hi[:,None]),0),axis=0)
    for shell in (.25,.4):
        t=np.minimum(distance/shell,1);eta=1-3*t*t+2*t*t*t
        assert np.max(np.abs(eta[ub.get_dofs('body').all()]-1))<1e-12
        assert np.max(np.abs(eta[ub.get_dofs(['walls','inlet','outlet']).all()]))<1e-12
        etas.append(eta);lifts.append({'shell_m':shell,'vector_laplacian_load_n':0.,
            'symmetric_stress_load_n':0.,'divergence_correction_n':0.})
    D=0.;div_squared=0.;div_max=0.
    for begin in range(0,mesh.nelements,chunk_size):
        indices=np.arange(begin,min(begin+chunk_size,mesh.nelements))
        vb,vp=cell_basis(ub,indices),cell_basis(pb,indices)
        gradient=np.stack([vb.interpolate(v).grad for v in u]);pressure=vp.interpolate(p)
        divergence=np.einsum('ii...->...',gradient)
        e=.5*(gradient+gradient.swapaxes(0,1))
        D+=float(np.sum(2*mu*np.einsum('ij...,ij...->...',e,e)*vb.dx))
        div_squared+=float(np.sum(divergence**2*vb.dx));div_max=max(div_max,float(np.max(np.abs(divergence))))
        for eta,lift in zip(etas,lifts):
            grad_eta=vb.interpolate(eta).grad
            vector=mu*np.einsum('j...,j...->...',gradient[0],grad_eta)-pressure*grad_eta[0]
            symmetric=mu*np.einsum('j...,j...->...',gradient[0]+gradient[:,0],grad_eta)-pressure*grad_eta[0]
            lift['vector_laplacian_load_n']-=float(np.sum(vector*vb.dx))
            lift['symmetric_stress_load_n']-=float(np.sum(symmetric*vb.dx))
            lift['divergence_correction_n']-=mu*float(np.sum(divergence*grad_eta[0]*vb.dx))
    for lift in lifts:
        lift['integration_by_parts_identity_error_n']=lift['symmetric_stress_load_n']-lift['vector_laplacian_load_n']-lift['divergence_correction_n']
    return (D,div_squared**.5,div_max), {'normal_load_n':np.sum(normal_load*fb.dx,axis=(1,2)).tolist(),
        'normal_divergence_identity_error_n':np.sum((normal_load-div_load)*fb.dx,axis=(1,2)).tolist(),
        'faces':faces,'volume_lifts':lifts,
        'scope':'chunked independent diagnostics; raw surface traction remains the physical gate'}


def observe_fused(mesh,ub,pb,u,p,mu,lo,hi,chunk_size):
    metrics,result=fused_metrics_and_consistency(mesh,ub,pb,u,p,mu,lo,hi,chunk_size)
    body=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['body'],intorder=8)
    g=np.stack([body.interpolate(v).grad for v in u]);div=np.einsum('ii...->...',g)
    centers=body.global_coordinates().mean(axis=2)
    for row in result['faces']:
        plane=(lo if row['side']==0 else hi)[row['axis']]
        mask=np.isclose(centers[row['axis']],plane)[:,None]
        row['boundary_divergence_l2_s_inv_m']=float(np.sum(div**2*body.dx*mask))**.5
    result['scope']='quartic field diagnostics; boundary divergence norm checked with high-order facet quadrature; no raw-force replacement'
    return metrics,result
