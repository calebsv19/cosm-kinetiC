"""Body-relative adaptive edge decisions; original physical coordinates restored."""
import numpy as np
from skfem import MeshTet
from cfd_reference3d_mesh import edge_distance

def refined_octant(axes, lengths, lo, hi, passes, body=True, radii=None):
    assert 0 <= passes <= 3
    mesh = MeshTet.init_tensor(*[x[x <= lengths[a]/2+1e-12] for a,x in enumerate(axes)])
    if body:
        c = mesh.p[:,mesh.t].mean(axis=1)
        mesh = mesh.remove_elements(np.nonzero(np.all((c>lo[:,None]) & (c<hi[:,None]),axis=0))[0])
    counts = [int(mesh.nelements)*8]
    for level in range(passes):
        if level == 0:
            mesh = MeshTet(np.round(mesh.p-lo[:,None],12),mesh.t)
        c = mesh.p[:,mesh.t].mean(axis=1)
        selected = np.nonzero(edge_distance(c,np.zeros(3),hi-lo) < (radii[level] if radii is not None else .12/2**level))[0]
        mesh = mesh.refined(selected)
        counts.append(int(mesh.nelements)*8)
        assert counts[-1] <= 50000, ('reference_mesh_cap',counts)
    if passes:
        mesh = MeshTet(mesh.p+lo[:,None],mesh.t)
    points, elements = [], []
    for side in range(8):
        coordinates = mesh.p.copy()
        for a in range(3):
            if side & (1<<a): coordinates[a] = lengths[a]-coordinates[a]
        elements.append(mesh.t+len(points)*mesh.p.shape[1]); points.append(coordinates)
    combined = np.concatenate(points,axis=1)
    unique,inverse = np.unique(np.round(combined.T,12),axis=0,return_inverse=True)
    full = MeshTet(unique.T,inverse[np.concatenate(elements,axis=1)])
    # Every boundary facet must lie on an actual domain or cube plane. A crack
    # on an artificial octant interface would otherwise become a false wall.
    mid = full.p[:,full.facets[:,full.boundary_facets()]].mean(axis=1)
    exterior = np.any(np.isclose(mid,0)|np.isclose(mid,lengths[:,None]),axis=0)
    body_face = np.all((mid>=lo[:,None]-1e-10)&(mid<=hi[:,None]+1e-10),axis=0)
    body_face &= np.any(np.isclose(mid,lo[:,None])|np.isclose(mid,hi[:,None]),axis=0)
    assert np.all(exterior | body_face), 'nonconforming octant interface'
    return full, counts
