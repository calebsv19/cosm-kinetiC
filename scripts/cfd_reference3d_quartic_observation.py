"""Reuse volume stress diagnostics with degree-six-exact boundary divergence norms."""
import numpy as np
from skfem import FacetBasis
from cfd_reference3d_chunked import diagnose_chunked


def quartic_consistency(mesh,ub,pb,u,p,mu,lo,hi,chunk_size):
    result=diagnose_chunked(mesh,ub,pb,u,p,mu,lo,hi,chunk_size)
    body=FacetBasis(mesh,ub.elem,facets=mesh.boundaries['body'],intorder=8)
    g=np.stack([body.interpolate(v).grad for v in u]);div=np.einsum('ii...->...',g)
    centers=body.global_coordinates().mean(axis=2)
    for row in result['faces']:
        plane=(lo if row['side']==0 else hi)[row['axis']]
        mask=np.isclose(centers[row['axis']],plane)[:,None]
        row['boundary_divergence_l2_s_inv_m']=float(np.sum(div**2*body.dx*mask))**.5
    result['scope']='quartic field diagnostics; boundary divergence norm checked with high-order facet quadrature; no raw-force replacement'
    return result
