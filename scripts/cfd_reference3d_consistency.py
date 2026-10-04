"""Independent surface/volume stress diagnostics; none is an accuracy certificate."""
import numpy as np
from skfem import Basis, FacetBasis, ElementTetP2, ElementTetP1, Functional, asm


def diagnose(mesh, ub, pb, u, p, mu, lo, hi):
    """Fluid outward normals; body load is minus traction on the fluid."""
    facets = mesh.boundaries['body']
    fb = FacetBasis(mesh, ub.elem, facets=facets, intorder=4)
    g = np.stack([fb.interpolate(v).grad for v in u])
    div = np.einsum('ii...->...', g)
    normal = fb.normals
    normal_load = -2 * mu * np.einsum('ii...->i...', g) * normal
    divergence_load = -2 * mu * div * normal
    identity = np.sum((normal_load-divergence_load)*fb.dx, axis=(1,2))
    x = fb.global_coordinates().mean(axis=2)
    faces = []
    for a in range(3):
        for side, plane in enumerate((lo[a], hi[a])):
            mask = np.isclose(x[a], plane)[:,None]
            faces.append({'axis': a, 'side': side,
                'normal_load_n': np.sum(normal_load*fb.dx*mask, axis=(1,2)).tolist(),
                'divergence_load_n': np.sum(divergence_load*fb.dx*mask, axis=(1,2)).tolist(),
                'boundary_divergence_l2_s_inv_m': float(np.sum(div**2*fb.dx*mask))**.5})
    # A P2 lift equals e_x on the body and zero on the external boundary.
    # All gradients are integrated independently of assembled matrix rows.
    volume = Basis(mesh, ub.elem, intorder=4)
    vp = Basis(mesh, pb.elem, quadrature=volume.quadrature)
    gradient = np.stack([volume.interpolate(v).grad for v in u])
    pressure = vp.interpolate(p)
    divergence = np.einsum('ii...->...', gradient)
    lifts = []
    for shell in (.25, .4):
        distance = np.linalg.norm(np.maximum(np.maximum(lo[:,None]-ub.doflocs,
                                                        ub.doflocs-hi[:,None]), 0), axis=0)
        t = np.minimum(distance/shell, 1)
        eta = 1 - 3*t*t + 2*t*t*t
        assert np.max(np.abs(eta[ub.get_dofs('body').all()]-1)) < 1e-12
        outer = ub.get_dofs(['walls', 'inlet', 'outlet']).all()
        assert np.max(np.abs(eta[outer])) < 1e-12
        grad_eta = volume.interpolate(eta).grad
        vector_density = mu*np.einsum('j...,j...->...', gradient[0], grad_eta)-pressure*grad_eta[0]
        symmetric_density = mu*np.einsum('j...,j...->...', gradient[0]+gradient[:,0],grad_eta)-pressure*grad_eta[0]
        vector_load = -float(np.sum(vector_density*volume.dx))
        symmetric_load = -float(np.sum(symmetric_density*volume.dx))
        divergence_correction = -mu*float(np.sum(divergence*grad_eta[0]*volume.dx))
        lifts.append({'shell_m':shell,'vector_laplacian_load_n':vector_load,
                      'symmetric_stress_load_n':symmetric_load,
                      'divergence_correction_n':divergence_correction,
                      'integration_by_parts_identity_error_n':symmetric_load-vector_load-divergence_correction})
    return {'normal_load_n':np.sum(normal_load*fb.dx,axis=(1,2)).tolist(),
            'normal_divergence_identity_error_n':identity.tolist(),'faces':faces,'volume_lifts':lifts,
            'scope':'independent diagnostics on approximate fields; volume lifts do not replace raw surface traction or certify reference accuracy'}
