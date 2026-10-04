# C3D-6: qualified stationary open straight 3D duct

2026-09-29. Implemented and verified in the existing PhysicsSim Main Edit.
**The documented C3D-6 boundary gate passes. Stop here.** No C3D-7/8 work, commit,
package, Desktop installation or canonical-source mutation was performed.
The predeclared equations and limits are in [cfd_open3d_gate.md](cfd_open3d_gate.md).
C3D-1..5 and the established 2D baselines remain intact.

## What was implemented

`cfd_open3d` is a separate stationary uniform MAC **Stokes** solver. It solves
all three face-velocity components and pressure at every cell. The interior
integrated continuity operator B shares faces, and momentum uses its negative
transpose. Component-specific dual volumes include the outlet-normal half volume.
Cached Z-aware sparse MG preconditions velocity inverses inside a matrix-free
pressure Schur CG solve. No periodic X edge, assigned interior pressure gradient,
reference pressure field, independently pinned outlet cell or projection shortcut
is used. Final true momentum residual and actual face divergence are checked.

Inlet X=0 is the analytic face-area average of the continuous developed rectangular
profile; Y/Z are stationary no-slip. Outlet X=L has the explicitly declared natural
**vector-Laplacian traction**, mu*d(u)/dx-p*e_x=0. This is not zero full symmetric
Cauchy traction. The weak-form distinction is also illustrated in the official
[FEniCS Stokes demo](https://docs.fenicsproject.org/dolfinx/main/python/demos/demo_stokes.html),
whose pressure sign convention differs from ours. Physical pressure is in Pa.
The test-only nonzero outlet datum shifts the applied natural traction and the
solved pressure together; it does not add an independent pressure constraint.

Pressure traces come from second-order extrapolation of solved cross-section cell
means. The upstream gradient is fitted on common x in [1,3] m. Four separate wall
shear integrals use no-slip half-cell gradients. Physical dissipation integrates
2mu S:S from reconstructed velocity derivatives. Boundary work reconstructs
symmetric stress and solved pressure at the inlet/outlet separately. Matrix
vector-gradient diffusion work is labeled separately and includes prescribed
inlet edges and its half-slab wall contribution. Agreement of matrix work is never
substituted for the physical energy reference.

The continuous Fourier reference is independent of the native mixed matrix:
pressure gradient G=mu Q/(H W mean_response), separate series wall integrals,
and physical power G L Q. The existing independently tested reference helpers
are reused; context is https://doi.org/10.1016/0735-1933(94)90046-9 .

## Fixed-case three-grid gate

L/H/W=4/2/2 m, rho=1 kg/m3, mu=0.1 Pa s, Q=0.008 m3/s.
Errors use actual staggered faces with dual-volume weights, solved pressure traces,
individual wall shear and independent physical energy. Numerical convergence and
physical acceptance remain separate.

| Grid | Face velocity L2 error | Pressure-drop error | Largest individual wall error | Physical dissipation error | Work/dissipation imbalance | Physical gate |
|---|---:|---:|---:|---:|---:|---|
| 16x8x8 | 3.041% | 6.267% | 6.384% | 11.357% | 6.722% | fail |
| 32x16x16 | 0.851% | 1.834% | 1.867% | 3.125% | 1.747% | fail |
| 64x32x32 | 0.220% | 0.513% | 0.522% | 0.804% | 0.442% | pass |

Finest limits declared in advance: 1% velocity/pressure/Q, 2% each wall shear,
physical dissipation and physical work imbalance. Finest pressure drop is
0.00566161642 Pa; each wall load is about 0.00566112359 N. Dissipation is
4.51604656e-5 W and boundary work 4.53602350e-5 W. Momentum true residual is
1.74e-14; max divergence 4.17e-15 s^-1; inlet/outlet relative flux difference
8.02e-15. Inlet Fourier integration differs from requested Q by 3.63e-10 relative,
without any profile rescaling. It meets the 1% requested-flow gate; actual flux
conservation meets the separately declared 1e-10 gate.

Observed orders coarse/mid and mid/fine: velocity 1.838/1.952, pressure
1.773/1.837, physical energy 1.862/1.958. Pressure approaches second order more
slowly; this does not establish asymptotic pressure order for arbitrary boundaries.
No gates were relaxed, and no extra finer grid was used to hide a boundary defect.

The unequal-spacing 4x1.5x3 m, 48x24x32 case also passes: velocity 0.356%,
pressure 0.733%, max wall shear 1.334%, physical energy 1.235% and work imbalance
0.694%. Doubling viscosity doubles pressure, wall force, dissipation and boundary
power at unchanged prescribed flow. Shifting natural outlet datum by 0.02 Pa
shifts inlet/outlet pressure traces by 0.02 Pa while preserving pressure drop,
velocity error and dissipation within 1e-9 (measured differences below 7e-14).

## Outlet-distance gate

Fixed dx=dy=dz=0.0625 m; grids 64x32x32, 96x32x32, 128x32x32 for L=4/6/8 m.
All three individually pass the continuous straight-duct gate.

| Extension | Common upstream gradient change | Largest wall shear per metre change | Dissipation per metre change |
|---|---:|---:|---:|
| 4 to 6 m | 5.69e-12 relative | 0.0493% | 0.000809% |
| 6 to 8 m | 9.19e-14 relative | 0.0246% | 0.000405% |

Every change is below the predeclared 1% limit. This establishes a steady,
fully developed straight-duct outlet screen. It does not establish wake exit,
backflow, unsteady transport, pressure-wave or arbitrary traction accuracy.

## Agent contract, evidence and cost

New immutable template `cfd_open_duct_3d`, mode `steady_open_duct`, uses the existing
`incompressible_cartesian3d_v1` local source session contract. It requires one
stationary solve and keeps physical time zero. Pressure/health/energy/four-wall
measurements, XYZ slices/probes, comparison and assessment are available. The
upper X face is exported separately as `outlet_x_velocity_m_s`; X must never wrap
when consuming open fields. `run_assess` recomputes numerical/reference limits
and adds work imbalance and flux conservation for this mode. A periodic duct
pass cannot silently qualify an open run, and an individual pass cannot certify
the multi-run outlet-distance screen.

Example scene request (JSON/MCP `scene_create`):

```json
{"scene_id":"open3d","template":"cfd_open_duct_3d","dimensions":[4,2,2],"channel":{"volume_flow_m3_s":0.008}}
```

Use the returned scene revision with `run_start`, grid [64,32,32], steps 1,
explicit SI fluid rho=1/mu=0.1 and a 128 MiB numerical cap. Starting paused permits
configuration inspection and a deliberate step. Sampling does not advance state.
Controls are serviced between solves; cancellation inside a stationary solve is
not implemented. Finest step responsiveness is therefore about 11 seconds here,
and the 8 m numerical solve about 29 seconds; no maximum-grid latency claim.

Optimized serial local worker measurements, including physical observation:

| Grid | Agent solve wall time | Numerical peak | Process peak RSS |
|---|---:|---:|---:|
| 16x8x8 | 56.4 ms | 1.01 MiB | 8.02 MiB |
| 32x16x16 | 824.6 ms | 8.58 MiB | 21.70 MiB |
| 64x32x32 | 11.08 s | 70.70 MiB | 130.84 MiB |

Finest setup is about 0.11 s, 37 pressure iterations and 3253 aggregate velocity
iterations; final full-field export takes 121 ms, ordinary snapshot publication
1.84 ms, total fresh agent run 11.36 s. Separate numerical extension measurements:
L=6 takes 20.18 CPU s / 20.70 wall s, 106.09 MiB numerical peak; L=8 takes
29.20 CPU s / 29.64 wall s, 141.49 MiB numerical peak. These are bounded local
measurements, not hardware-independent scaling guarantees. Budget excludes JSON
and process RSS. Cached solve/sampling allocate no numerical blocks after setup.
The nested solve is substantially costlier than the periodic scalar reduction;
future preconditioning/interruptibility can improve cost without weakening this gate.

`build/c3d-open/qualification.json` contains all grid/outlet/rectangle/scaling
measurements and executable identity. `agent-evidence/qualification.json` contains
retained runs and original cost data. `completion-audit.json` checks 18 artifact
digests and independently reconstructs pressure traces, inlet/outlet flux and
every cell's nonperiodic divergence from exported fields. It verifies the original
Cartesian, duct, periodic-transient and sparse-MG source digests are unchanged.
Worker SHA256: `e0a14bb657c4b0c2b042bbbd1119e53eefc9c939debaf183e5837c29033d7b5a`.

Verification includes independent-vector B/B^T adjoint identity and H symmetry,
positive quadratic work, strict true residuals, exact setup cap and one-byte-below
rejection, partial-allocation cleanup, no-advance sampling, unavailable-field
semantics, digest readback, idempotent evidence retry, ASan/UBSan, periodic 3D
spatial/time qualification, 2D memory/MG/transient/energy/units/mixed and 2D/3D
agent regressions. Native source build links. LeakSanitizer is unavailable on this
macOS platform; deterministic numerical live-byte cleanup is checked. No GUI
visual acceptance or Desktop refresh was performed.

```sh
make qualify-cfd-open3d
make test-cfd-open3d-sanitize test-cfd-open3d-agent-session
make verify-cfd-open3d-agent-evidence
python3 scripts/audit_cfd_open3d_evidence.py
```

The audit consumes the retained regression logs. For a fresh checkout, also run
`make qualify-cfd-3d`, the documented 2D regression targets and source native build
before a full completion audit. Generated evidence lives under build/ and is not
a public release artifact.

## Stop boundary

C3D-6 is complete for this fixed steady straight Stokes duct and declared outlet.
The next separately authorized gate is C3D-7: a nonuniform three-component wall
transient with known physical pressure, independent space/time refinement and
separate wall/pressure-coupling diagnostics. Obstacles and closed body forces remain
C3D-8. This slice does not qualify general wind CFD, moving objects, atmosphere,
water, turbulence, 3D local refinement or arbitrary geometry.
