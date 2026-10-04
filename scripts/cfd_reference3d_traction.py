"""Physical planar Cauchy traction, retaining normal stress and edge-band loads."""
import numpy as np
from skfem import FacetBasis


def traction(mesh, ub, pb, u, p, mu, lo, hi, order=4):
    facets = mesh.boundaries['body']
    fb = FacetBasis(mesh,ub.elem,facets=facets,intorder=order)
    fp = FacetBasis(mesh,pb.elem,facets=facets,quadrature=fb.quadrature)
    g = np.stack([fb.interpolate(v).grad for v in u])
    pressure = fp.interpolate(p)
    normal = fb.normals
    raw = -mu*np.einsum('ij...,j...->i...',g+g.swapaxes(0,1),normal)
    diagonal = -2*mu*np.einsum('ii...->i...',g)*normal
    tangential = raw-diagonal
    pressure_load = pressure*normal
    coordinates = fb.global_coordinates()
    mid = coordinates.mean(axis=2)
    faces = []
    bands = [0,.025,.05,.1,.2,.5+1e-10]
    for a in range(3):
        other = [b for b in range(3) if b != a]
        edge = np.minimum(np.min(coordinates[other]-lo[other,None,None],axis=0),
                          np.min(hi[other,None,None]-coordinates[other],axis=0))
        for side,plane in enumerate((lo[a],hi[a])):
            mask = np.isclose(mid[a],plane)
            def integrate(field, selection):
                return np.sum(field*fb.dx*selection,axis=(1,2)).tolist()
            face = {'axis':a,'side':side,'area_m2':float(np.sum(fb.dx*mask[:,None])),
                    'pressure_force_n':integrate(pressure_load,mask[:,None]),
                    'raw_viscous_force_n':integrate(raw,mask[:,None]),
                    'normal_viscous_force_n':integrate(diagonal,mask[:,None]),
                    'tangential_viscous_force_n':integrate(tangential,mask[:,None]),'edge_bands':[]}
            for lower,upper in zip(bands,bands[1:]):
                selected = mask[:,None] & (edge>=lower-1e-12) & (edge<upper-1e-12)
                face['edge_bands'].append({'distance_m':[lower,upper],
                    'pressure_force_n':integrate(pressure_load,selected),
                    'raw_viscous_force_n':integrate(raw,selected),
                    'normal_viscous_force_n':integrate(diagonal,selected)})
            faces.append(face)
    return {'quadrature_order':order,'faces':faces,
            'pressure_force_n':np.sum(pressure_load*fb.dx,axis=(1,2)).tolist(),
            'raw_viscous_force_n':np.sum(raw*fb.dx,axis=(1,2)).tolist(),
            'normal_viscous_force_n':np.sum(diagonal*fb.dx,axis=(1,2)).tolist(),
            'tangential_viscous_force_n':np.sum(tangential*fb.dx,axis=(1,2)).tolist()}
