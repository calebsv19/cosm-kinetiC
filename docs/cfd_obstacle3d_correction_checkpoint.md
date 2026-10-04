# C3D-8 reference and body-edge traction/energy correction

Historical native correction checkpoint. The next reference/pressure investigation
is now implemented; see [current robustness results and continuation](cfd_obstacle3d_reference_refinement_checkpoint.md).

2026-09-30, PhysicsSim Main Edit. **Correction implemented and verified; C3D-8
physical certification remains false.** The [original gates](cfd_obstacle3d_goal.md)
and [correction experiment contract](cfd_obstacle3d_correction_goal.md) are unchanged.
This supersedes current measurements in the [historical checkpoint](cfd_obstacle3d_checkpoint.md).
No commit, package, installation or canonical adoption occurred.

## What changed and why

The previous tangential traction extrapolation integrated body patches without a
continuous zero trace at the actual body edges. Its cell-gradient energy estimator
also combined extrapolations and omitted within-cell gradient variation. These
were observation defects: the masked coupled Stokes equation is unchanged.

The new app-owned reconstruction builds a continuous piecewise trilinear velocity
on the half-cell lattice, imposing zero at actual body and outer-wall planes and
edges. Composite bilinear face integration uses the first half-cell velocity for
tangential no-slip traction. Flat-wall normal derivative is constrained to zero
by the exact no-slip/incompressibility identity; it is not inferred from a drag
reference. Pressure interval reconstruction is unchanged. Physical dissipation is
the exact integral of 2 mu S:S for the declared interpolant, including polynomial
gradient variation. No matrix work, force balance, empirical normalization or
reference value enters either measurement. Old quadratic viscous force and old
cell-gradient dissipation remain explicitly named diagnostics; discrete row
reaction and energy remain separate.

The interpolant is **not pointwise divergence-free**. Its L2 divergence is now
exposed separately from MAC cell-integrated continuity: finest short-grid
0.002091 s^-1 m^1.5 versus approximately 1e-12 s^-1 MAC divergence. Three cached
planes bound reconstruction memory without allocating a dense interpolated volume
on every solve. Existing numerical allocation, session/MCP and MG ownership are
reused; no shared API/adoption changes are needed.

All original seven velocity/pressure binary field artifacts have exactly the same
SHA-256 as before this correction. The masked mixed solver and 62 predecessor CFD
source files outside three existing additive session/observation bridges are
byte-preserved. This proves that the measured improvement is diagnostic correction,
not changed transport or a newly accurate pressure solution.

## Measured native results

| Case | Before | Corrected | Original gate |
|---|---:|---:|---:|
| L4, n48 physical momentum imbalance | 4.750% | 0.988% | <=2% |
| L4, n48 physical energy imbalance | 2.879% | 0.433% | <=3% |
| L4, n48 viscous body force | 0.0160520 N | 0.0145770 N | independent reference required |
| L4, n48 strain dissipation | 0.000170310 W | 0.000176120 W | independent reference required |
| L8, center4, n40 physical momentum imbalance | new admitted control | 0.946% | <=2% |
| L8, center4, n40 physical energy imbalance | new admitted control | 0.395% | <=3% |

The original four short grids and L4/6/8 distance and inlet controls all ran again.
Numerical residual, MAC divergence, flux, discrete energy and symmetry gates pass
in all eight cases. All lower XYZ velocities, Pa pressure, solid masks and upper X
face data match native/agent readback exactly. Original downstream/upstream
fixed-spacing distance gates still pass. Short-grid component/energy errors now
decrease across all four declared grids **against the retained unresolved L4
reference**; this is screening evidence, not an independent accuracy certificate.

The new n40 long-duct case has 160 x 40 x 40 = 256000 cells, 248000 fluid cells,
731600 stored faces. It obeys the existing 262144-cell/512-MiB numerical cap. Agent
wall time was 63.51 s, native 63.73 s; numerical peak 237.50 MiB, observed agent
process peak 455.23 MiB (includes JSON/export). The original n48 short duct took
49.17 s through the agent, numerical peak 199.24 MiB and process peak 396.27 MiB.
These are bounded run measurements with other local work, not isolated benchmarks.
Largest-grid cancellation latency was not newly certified by this matrix.

## Independent measurement and reference evidence

A known analytic curl with exact no-slip planes/edges, analytic face averages,
known cubic-pressure surface force and independently factored polynomial strain
energy tests sample interpretation and reconstruction independently of the solver.
An independent Python implementation integrates the trilinear gradient using
2-point Gauss quadrature on every half-cell; it agrees with the C strain integration
within 1e-10 relative. Body traction quadrature also agrees independently. The
analytic energy errors on n8/16/32/48 decrease 66.83%, 32.90%, 12.23%, 6.28%.
This intentionally high-degree field remains coarse-grid underresolved; its
viscous-force error is not monotonic on every grid, though finest is 4.36%.
The new method is not universally more accurate on coarse data, and this test
must not be advertised as a universal force or second-order certificate.

The independent continuous P2/P1 Stokes reference now assembles scalar velocity
blocks instead of storing the generic vector basis. Same-mesh pressure, separate
forces, weak reaction and energy match the retained implementation to 1.11e-10
relative. Peak RSS falls from 1.594 GiB to 583.25 MiB. Geometry/fluid/boundaries/Q
remain independently prescribed, not calibrated to native drag.

Plane-graded reference meshes 6/8/10/12 retain the entire declared sequence.
The finest unmirrored reference passes separate pressure/viscous/energy refinement
changes but physical surface total versus weak reaction differs 1.302%, failing
the original 1% gate. It also shows spurious transverse loads in this symmetric
geometry. A conforming mesh built by reflecting one octant removes the transverse
bias without post-solve force averaging. Its 8/10/12 controls produce:

| Final n10 -> n12 reference check | Measured | Gate |
|---|---:|---:|
| Pressure-force change | 0.610% | <=1% |
| Raw symmetric viscous-force change | **1.383%** | <=1% |
| Physical dissipation change | 0.00510% | <=1% |
| Physical surface total vs weak reaction | 0.763% | <=1% |

Finest symmetric reference: pressure force 0.02451139 N, viscous force 0.01493733 N,
physical dissipation 0.0002249264 W, Pin 0.02804856 Pa. It uses 47232 tetrahedra,
738.03 MiB RSS (773881856 bytes) and 39.72 s. True free-row residual is 1.00e-10;
a small linear residual does not resolve surface-force discretization uncertainty.

Against this **still unresolved** matched symmetric long-domain reference, native
n40 pressure force is **5.71% low**, outside the 5% screen; viscous error is 3.25%,
total force 4.78%, dissipation 1.97%, Pin 2.12%. Its favorable unmirrored reference
screen is retained separately, not selected to certify the run. The symmetric
reference is the physically better symmetry control but still needs force
convergence. A new matched L4 reference must also pass before the original
short-domain qualification can be closed.

A consistent grad-div gamma=mu control on 8/10/12 does not fix the discrepancy:
physical surface versus stabilized reaction differs about 1.588%. Added and
unstabilized reactions remain separate. Gamma=10 mu reaches the 3000-iteration
limit despite a small true residual; it is a failed control, not a reference.
An empty n12 mesh is rejected before assembly at 57600 tetrahedra; an earlier
empty n10 solve fails the true-residual gate. Both are retained, followed by a
separately named tighter-solve n10 empty calibration passing continuous Fourier
Pin/energy within 0.051%. No memory, mesh, iteration, time or acceptance limit
was relaxed. References retain <=50000 tetrahedra, <=1800 MiB own RSS and <=180 s.

## Verification and reproduction

Current optimized worker SHA-256:
`c2a234ff0238f12b64c739ccbb1f6304e2ff3244613b125eb453fd40d08caf64`.
The audit binds source, worker, artifact, current test-log and reference receipt
hashes. Historical predecessor audit and fields remain under `correction-v1`.

```sh
make test-cfd-obstacle3d-reconstruction
make cfd-obstacle3d-correction-reference
make verify-cfd-obstacle3d-extended40
make audit-cfd-obstacle3d-correction
# Expected exit 1 until the physical gates are genuinely qualified:
python3 scripts/audit_cfd_obstacle3d_correction.py --require-qualified
```

Native contracts and ASan/UBSan pass (gauge/adjoint/cache/cancel/cap/cleanup).
Current obstacle MCP controls, cartesian/open/transient 3D and refined 2D agent
tests pass; native C7 session/sanitizer, numerical allocator/MG and refined 2D
transient/energy/units/mixed regressions pass. The source GUI builds. No Desktop
package/visual acceptance was exercised; macOS LeakSanitizer remains unavailable.

Machine evidence: `build/c3d-obstacle/correction-v1/completion-audit.json`.
Root `build/c3d-obstacle/completion-audit.json` now points to this correction.
Audit is `implemented_unqualified`, with unchanged gates and all failures exposed.
Single-run agent assessment continues to return reference accuracy not established.

## Next bounded continuation

1. Qualify the reflection-symmetric reference's **raw viscous traction**, retaining
   pressure and weak reaction independently. Investigate edge/corner traction
   integration and pressure/velocity approximation separately; use a known-answer
   no-slip surface-stress test and genuine conforming local reference refinement
   under the existing resource cap. Do not replace raw physical traction by weak
   reaction or tune a stabilization coefficient to native drag.
2. Repeat that accepted method on both L8/center4 and the original L4 domain, with
   separate force/energy changes <=1% and surface/reaction agreement <=1%.
3. If the matched reference remains stable, investigate the native pressure-force
   deficit with pressure interval/edge known answers and solver spatial refinement
   controls. Distinguish reconstruction error from the solved pressure field.
   Preserve all eight controls, current flow fields and physical balance diagnostics.
4. Close the original 5% component force, 3% Pin/D, 2% momentum and distance gates
   before C3D-8 qualification. Stop before native adaptive grids, geometry variety,
   moving bodies or inertial/turbulent wakes.

Suggested continuation: `/c3d-next Read docs/cfd_obstacle3d_correction_checkpoint.md
and docs/cfd_obstacle3d_correction_goal.md in Main Edit. Qualify the symmetric
independent reference raw viscous edge traction under existing caps, then run
matched L4/L8 reference controls and identify native pressure-force error. Preserve
all failed controls and physical gates. No commit or package.`
