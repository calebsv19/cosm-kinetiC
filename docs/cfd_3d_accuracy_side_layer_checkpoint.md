# Targeted side-layer geometry assessment

2026-10-04, existing Main Edit. This follows the sealed
[accuracy-first force and stress measurements](cfd_3d_accuracy_first_checkpoint.md).
The first fluid layers on Y/Z side faces are the next spatial target because the
signed stress defect concentrates near those cube edges. This checkpoint prepares
concrete geometry controls, without solving a new field or changing the equations.

Two predefined controls halve the first side-normal spacing from 0.0625 to
0.03125 m on both side axes and both physical domains. Redistribution moves the
existing first outer planes. Bisection retains them and adds halfway planes.
Both preserve actual cube triangles, physical boundary planes, reflected cells,
Y/Z exchange, positive volume and exact fluid volume/boundary areas. Neither
claims to preserve each original tetrahedron or to be uniform refinement.

| Control | Tetrahedra L4 / L8 | Max Jacobian condition L4 / L8 |
|---|---:|---:|
| Accepted finer parent | 43008 / 49920 | 107.872 / 181.230 |
| First-layer redistribution | 43008 / 49920 | 215.373 / 362.240 |
| First-layer bisection | 62976 / 72384 | 215.373 / 362.240 |

The worst intrinsic shape measures rise from 26.913 / 53.353 to
53.345 / 106.448. These are measured geometric diagnostics, not force-error bounds.
Shrinking the side layers without a corresponding graded transition improves
normal resolution but worsens anisotropy. No control is selected for a numerical
trial yet; no force benefit is asserted from geometry alone. The geometry survey
explicitly permits up to 75000 cells, while the existing numerical contract stays
50000. The bisection controls therefore also require a separately declared numerical
resource contract if a balanced variant is selected. There is no failed numerical
factor admission, since no factorization was attempted.

The next mesh design should balance these first layers with neighboring tangential
and streamwise transition cells, then record local and global quality before
fresh complete-factor memory admission. Once an appropriate geometry is selected,
solve L4 with unchanged physical residual, pressure, flux, divergence, energy and
raw-force gates. Advance to its matched L8 only after actual L4 numerical and
physical readback. Retain component changes and the raw/reaction discrepancy even
when one improves and another does not. Do not substitute weak loads or tune the
preconditioner merely to make a physical measurement run faster.

Two independent controls pass on all four geometries. Frozen source/test proof,
exact paired geometry archive, survey metrics and the once-only audit are under
`build/c3d-accuracy-side-layer/`. The archive contains meshes, not velocities or
pressures. Native binary/audit hashes remain preserved. The broad stationary,
general-object and transient CFD qualification sequence remains open.
