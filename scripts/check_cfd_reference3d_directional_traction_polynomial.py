"""Independent analytic face-load calibration on the actual directional cube meshes.

Prescribed quartic divergence-free velocity and cubic pressure only: no PDE solve,
force correction, gauge fit, or physical cube-flow qualification.
"""
import gc
import hashlib
import json
from pathlib import Path
import resource
import time
import numpy as np
from skfem import Basis, ElementDG, MeshTet
from cfd_reference3d_p4 import ElementTetP4, require_sorted
from cfd_reference3d_p3 import ElementTetP3
from cfd_reference3d_traction import traction

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'build/c3d-directional-traction-polynomial'
MU, A, B, C = .1, .03, .07, .11
LINEAR = np.array([.2, -.3, .4])
CUBIC = np.array([.13, -.17, .19])


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pressure(r, gauge):
    return gauge + np.einsum('i,i...->...', LINEAR, r) + np.einsum('i,i...->...', CUBIC, r**3) + .23 * r[0] * r[1] * r[2]


def velocity(r):
    x, y, z = r
    return np.array([2*A*x**3*y+B*x**3+C*x**2,
                     -3*A*x**2*y**2-3*B*x**2*y-2*C*x*y, np.zeros_like(z)])


def exact_faces(gauge):
    """Independent tensor-product Gauss integration of analytic Cauchy stress."""
    nodes, weights = np.polynomial.legendre.leggauss(4)
    yy, zz = np.meshgrid(nodes/2, nodes/2, indexing='ij')
    ww = weights[:, None] * weights[None, :] / 4
    result = []
    for axis in range(3):
        other = [j for j in range(3) if j != axis]
        for side in (0, 1):
            r = np.zeros((3, 4, 4))
            r[axis] = -.5 if side == 0 else .5
            r[other[0]], r[other[1]] = yy, zz
            x, y, z = r
            gradient = np.zeros((3, 3, 4, 4))
            gradient[0, 0] = 6*A*x*x*y+3*B*x*x+2*C*x
            gradient[0, 1] = 2*A*x**3
            gradient[1, 0] = -6*A*x*y*y-6*B*x*y-2*C*y
            gradient[1, 1] = -6*A*x*x*y-3*B*x*x-2*C*x
            normal = np.zeros(3); normal[axis] = -1 if side == 0 else 1
            pressure_load = -pressure(r, gauge)[None] * normal[:, None, None]
            viscous = MU*np.einsum('ij...,j->i...', gradient+gradient.swapaxes(0, 1), normal)
            diagonal = 2*MU*np.einsum('ii...->i...', gradient)*normal[:, None, None]
            result.append(dict(axis=axis, side=side, area_m2=1.,
                               pressure_force_n=np.sum(pressure_load*ww, axis=(1, 2)),
                               raw_viscous_force_n=np.sum(viscous*ww, axis=(1, 2)),
                               normal_viscous_force_n=np.sum(diagonal*ww, axis=(1, 2))))
    np.testing.assert_allclose(np.sum([q['pressure_force_n'] for q in result], axis=0),
                               -LINEAR-CUBIC/4, rtol=0, atol=1e-14)
    np.testing.assert_allclose(np.sum([q['raw_viscous_force_n'] for q in result], axis=0),
                               [2*MU*C, -MU*A, 0.], rtol=0, atol=1e-14)
    return result


def main():
    DEST.mkdir(exist_ok=False)
    inputs = ['scripts/check_cfd_reference3d_directional_traction_polynomial.py',
              'scripts/cfd_reference3d_traction.py', 'scripts/cfd_reference3d_p3.py',
              'scripts/cfd_reference3d_p4.py',
              'build/c3d-directional-quality/geometry.npz']
    hashes = {q: sha(ROOT/q) for q in inputs}
    frozen = DEST/'source'
    for q in inputs[:4]:
        p = frozen/q; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes((ROOT/q).read_bytes())
    contract = dict(source_sha256=hashes, absolute_force_error_max_n=1e-9,
                    wall_cap_s=300, rss_cap_bytes=2048*1024**2, quadrature_orders=[4, 8],
                    gauges=[0., 1.7], physical_cube_force_qualification=False)
    (DEST/'contract.json').write_text(json.dumps(contract, indent=2)+'\n')
    started = time.monotonic(); rows=[]; maximum=0.
    for label, archive, prefix in (
            ('directional-L4-edge047', inputs[4], 'L4_matched_edge047_'), ('directional-L8-edge047', inputs[4], 'L8_matched_edge047_')):
        with np.load(ROOT/archive, allow_pickle=False) as saved:
            m = MeshTet(saved[prefix+'vertices_m'], saved[prefix+'tetrahedra'])
            lo, hi = saved[prefix+'lo'], saved[prefix+'hi']
        require_sorted(m)
        def body(x):
            return np.all((x >= lo[:, None]-1e-10) & (x <= hi[:, None]+1e-10), axis=0) & np.any(np.isclose(x, lo[:, None]) | np.isclose(x, hi[:, None]), axis=0)
        m = m.with_boundaries({'body': body})
        ub = Basis(m, ElementTetP4(), intorder=1, elements=np.array([0]))
        pb = Basis(m, ElementDG(ElementTetP3()), quadrature=ub.quadrature, elements=np.array([0]))
        center = (lo+hi)/2
        u = velocity(ub.doflocs-center[:, None])
        for gauge in contract['gauges']:
            p = pressure(pb.doflocs-center[:, None], gauge)
            expected = exact_faces(gauge)
            for order in contract['quadrature_orders']:
                observed = traction(m, ub, pb, u, p, MU, lo, hi, order)
                error = 0.
                for face, exact in zip(observed['faces'], expected):
                    assert (face['axis'], face['side']) == (exact['axis'], exact['side'])
                    assert abs(face['area_m2']-1.) < 1e-10
                    for key in ('pressure_force_n', 'raw_viscous_force_n', 'normal_viscous_force_n'):
                        error = max(error, float(np.max(np.abs(np.array(face[key])-exact[key]))))
                        np.testing.assert_allclose(face[key], exact[key], rtol=0, atol=1e-9)
                maximum = max(maximum, error)
                rows.append(dict(mesh=label, tetrahedra=m.nelements, gauge_pa=gauge,
                                 quadrature_order=order, maximum_face_load_error_n=error,
                                 pressure_force_n=observed['pressure_force_n'],
                                 viscous_force_n=observed['raw_viscous_force_n']))
                print(json.dumps(rows[-1]), flush=True)
                assert time.monotonic()-started < 300 and resource.getrusage(resource.RUSAGE_SELF).ru_maxrss < 2048*1024**2
        del m, ub, pb, u, p, observed
        gc.collect()
    for q, h in hashes.items():
        assert sha(ROOT/q) == h
    result = dict(status='passed_actual_mesh_polynomial_traction_calibration', controls=rows,
                  maximum_face_load_error_n=maximum, source_sha256=hashes,
                  wall_s=time.monotonic()-started,
                  peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
                  native_operator_changed=False, physical_cube_force_qualification=False,
                  scope='known quartic divergence-free velocity and cubic pressure on actual cube meshes, six individual face loads, two gauges and orders; prescribed fields, no PDE solve')
    (DEST/'checkpoint-audit.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(dict(status=result['status'], maximum_error_n=maximum)), flush=True)


if __name__ == '__main__':
    main()
