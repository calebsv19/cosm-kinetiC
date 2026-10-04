# C3D-7A/B/C: verified wall, transport, startup and controlled transient outlet

2026-09-30. The complete predeclared [A/B/C contract](cfd_wall3d_goal.md) passes
in PhysicsSim Main Edit source. The final worker is `797c464e8708e0579b08bfa00f5a6dba3caec945f37e59bc9f7f16ba4a1ee995`.
No commit, package, Desktop install, canonical adoption or C3D-8 work was performed.
The earlier [checkpoint](cfd_wall3d_checkpoint.md) remains historical progress/
defect evidence; this document and `build/c3d-wall/completion-audit.json` supersede
its outstanding-work status for this bounded contract.

## Requirement and implementation evidence

| Delivery | Verified implementation | Acceptance evidence |
|---|---|---|
| A known-answer wall transient | Three velocities and physical Pa pressure vary through XYZ/time; independent continuous curl-potential source, no-slip Y/Z with nonzero pressure normal gradients; mass/viscosity/pressure solved jointly | Three grids, five timesteps, separate true momentum/divergence, pressure/wall/physical-energy errors and field readback |
| B conservative transport | Shared dual-face fluxes, BE then BDF2 with extrapolated transport; actual component CFL <=.25; separate transport kinetic self-work | Same spatial/time gates, accepted-step harmonic fit, amplitude/orthogonal/pressure/shear/energy diagnostics and independent harmonic readback |
| C physical startup | Start at rest with natural Pin/Pout tractions, all X-normal faces unknown including half slabs; no body forcing | Independent rectangular Fourier profile/Q/wall/E/D/E' reference at .5/2/12 s, tail/energy checks, spatial/time refinement and steady recovery |
| C controlled nonuniform open transient | Three-component unsteady Stokes with continuous natural end tractions and fixed 4 m forcing wavelength | Three grids, five timesteps, physical boundary work and common-upstream 4/6/8 m comparisons |
| Shared agent contract | Four immutable modes in existing source scene/session service; Pa/XYZ/probes, scoped assessments/comparisons, safe ordered controls and full-field artifacts | 47 retained exact-worker cases, independent every-field readback, actual MCP stdio/reconnect and sustained inspection tests |
| Cost and preservation | Cached mixed/MG/Krylov/reference storage, private candidates, exact numerical admission and cleanup; cleanup-tested static JSON dependency | Native/sanitizer/budget/control/ownership tests, measured costs, original 2D/periodic/open numerical digest and agent regression checks |

The shared mixed engine uses integrated B and -B^T with component-specific dual
volumes, implicit vector-Laplacian viscosity and physical cell pressure. Cached
velocity inverses/MG precondition a matrix-free pressure Schur solve. Full true
momentum <=1e-11 and max physical divergence <1e-8 s^-1 are acceptance conditions.
The periodic commuting factorization is not assumed at walls/open boundaries.
Open pressure datum comes from natural traction; wall pressure is not assigned an
artificial zero normal derivative. Continuous forcing and reference functions are
independent of native matrix action on the exact field.

## Accuracy and refinement

All percentages below are measured continuous-reference errors. Wall MMS pressure
uses a fixed nonzero peak RMS scale corresponding to .01 Pa, avoiding undefined
relative errors at temporal pressure zero crossings. Wall errors compare eight
signed tangential components by nonzero reference RMS; startup compares each of
four integrated loads separately.

| Finest case | Velocity | Pressure | Largest wall error | Physical dissipation | Physical budget imbalance |
|---|---:|---:|---:|---:|---:|
| A 32 cubed, t=.4 | .1706% | .1333% | 1.9005% | .4911% | .0816% |
| B same grid/time | .1708% | .1406% | 1.9008% | .4910% | .0816% |
| Open 64x32x32, t=.4 | .1760% | .1726% | .7791% | .3538% | .1008% |
| Startup 64x32x32, t=.5 | .7830% | roundoff | .4323% | .9719% | .6675% |
| Startup same grid, t=2 | .3879% | roundoff | .0848% | .2290% | .4544% |
| Startup same grid, t=12 | .3223% | roundoff | .0011% | .0616% | .4350% |

A/B asymptotic spatial orders are about 1.93 velocity, 2.00 pressure, 2.05 wall
shear and 2.01 dissipation; final time self-difference pairs exceed 2.05 for
velocity and pressure. B harmonic amplitude error is .7712%, phase error .0965
 degrees; pressure amplitude error .1430%, phase error .000274 degrees. Transport
self-work is negligible on these divergence-free impermeable-wall cases. This is
not an exact physical energy conservation claim: physical strain, forcing, boundary,
transport work, kinetic rate and residual are exposed independently.

Open-transient final spatial orders are 1.977 velocity, 1.982 pressure, 2.004 wall
and 1.862 dissipation. Coarse dissipation order .257 is not declared asymptotic.
Both final temporal pairs pass the 1.8 screen. Across 4/6/8 m, maximum common
upstream velocity change is .00601%, pressure change .002325% of .01 Pa, D/length
change .00357% and E/length change .000368%; all below the predeclared 1% gate.

Startup time orders approach 2.00. Its pressure is exactly linear; roundoff-only
temporal pressure differences are not assigned a convergence order. Outlet
extensions preserve Q, each wall load/length, E/length and D/length to roundoff.
At 12 s, continuous Q remains .2539% below its limit; measured Q differs from the
limiting .008 m3/s by .1206%. The early impulsive interval is excluded until the
reference is resolved (.5 s for this default case). Startup is temporal development
of an X-uniform profile, not axial entrance or wake development.

## Agent controls, observations and runtime cost

Templates: `cfd_wall_stokes_3d`, `cfd_wall_transport_3d`,
`cfd_pressure_startup_3d`, `cfd_open_wall_transient_3d`. All reuse
`incompressible_cartesian3d_v1` and the same immutable scene revision/owned local
session contract. Agent inspection has separate numerical, continuous-reference
and harmonic status; failed coarse physical assessments remain retained even when
numerical convergence passes. No per-run pass certifies arbitrary CFD.

Inspection during a mixed solve reads last accepted fields/time/physical diagnostics;
private candidates never leak into pressure, energy or CFL observations. Sampling
never advances state or allocates numerical storage. Cooperative cancellation is
checked in inner/outer Krylov iterations; only the next ordered revision-bound
cancel can interrupt. Its receipt follows safe cancellation/export, and an
interrupted paused step has its own cancelled receipt. Pause/continue remain
step-boundary controls. Accepted fields are preserved on cancellation and failed
admission/solve; terminal sessions cannot restart.

The final [64,32,32] bounded test measured live sampling 107.2 ms
and cancellation receipt 290.5 ms including full export;
all retained cancelled fields exactly match an independent one-step reference.
This is not a worst-case maximum-grid latency guarantee.

Serial optimized local measurements include observation and in-solve publication.
Numerical memory excludes JSON and process RSS; actual process peak includes them.
Some early fixtures overlapped focused test processes, so these are bounded cost
measurements rather than hardware-independent benchmarks.

| Case | Whole agent run | Last step wall | Numerical peak | Process peak RSS |
|---|---:|---:|---:|---:|
| A walls 32 cubed, 160 steps | 94.25 s | 0.571 s | 54.80 MiB | 100.64 MiB |
| B transport 32 cubed, 160 steps | 95.44 s | 0.575 s | 54.80 MiB | 100.05 MiB |
| Startup 64x32x32, 600 steps | 194.01 s | 0.341 s | 100.84 MiB | 185.84 MiB |
| Open L=4, 80 steps | 135.71 s | 1.594 s | 113.14 MiB | 199.67 MiB |
| Open L=6, 80 steps | 217.60 s | 2.565 s | 169.41 MiB | 276.77 MiB |
| Open L=8, 80 steps | 312.60 s | 3.745 s | 225.70 MiB | 353.97 MiB |

Matrices, MG, Krylov and reference workspaces are reused. One BE-to-BDF2 mass/
hierarchy transition is permitted; subsequent steps allocate no numerical blocks.
Every new mode passes exact peak admission and safe rejection one byte below.
ASan/UBSan and deterministic numerical cleanup pass; LeakSanitizer is unavailable
on macOS. Separate macOS allocator/live-worker probes cover JSON ownership.

## Defects found and verified corrections

1. Physical strain reconstruction treated face/cell averages as point values near
   walls. An exact-face probe isolated it without a solve. Cubic one-sided wall
   derivatives and fourth-order centred cross derivatives remove that bias;
   quadratic and independent Gauss physical-energy tests pass. Evolution unchanged.
2. Native transport metrics changed during candidate preparation. The adapter now
   caches them only after acceptance; complete accepted-budget/CFL callback tests
   prove coherence during and after cancellation. Evolution unchanged.
3. Owner-exit supervision could overwrite a durable terminal publication after a
   stale running read. The service now rereads under the released owner lock.
   Deterministic completed/cancelled/failed race tests and actual worker-death tests
   pass; the affected original run and guarded single-case retest remain retained.
4. Installed release json-c 0.19 leaked container storage. A minimal array put loop
   and macOS leaks reproduced it independently of the solver. The
   [upstream destructor](https://github.com/json-c/json-c/blob/json-c-0.19/json_object.c#L412)
   places cleanup inside an assertion. Main Edit's macOS source build binds the
   already installed tested 0.18 static archive, with a required optimized-build
   ownership test. No global dependency install or opt-symlink modification.
   Snapshots declare the runtime JSON version. Over 150 warmed-up samples RSS
   growth is 16.0 KiB; the original probe grew about 128 MiB.

The final full 47-case matrix and independent readback were rerun under the corrected
worker identity. Earlier numerical/agent matrices, failed probes and superseded
worker namespaces remain historical evidence; they are not silently replaced.
Static JSON binding is a tested macOS source mitigation. Linux dependency/runtime
behavior and future package dependency inclusion require their own qualification.

## Evidence and reproduction

`build/c3d-wall/completion-audit.json` covers all twelve original requirement groups,
current source/executable/dependency digests, regression evidence, cost and limits.
`agent-evidence/qualification.json` retains all 47 cases under the final worker
hash; `agent-evidence/readback-audit.json` independently reconstructs every exported
field, kinetic/wall observations, every-cell divergence, continuous error/pressure/
flow, spatial/time differences, harmonic results and outlet comparisons. Native
field equality is checked to 1e-10; artifact bytes are digest-verified.

```sh
make qualify-cfd-wall3d qualify-cfd-startup3d qualify-cfd-open-transient3d
python3 scripts/qualify_cfd_wall3d_phase.py
make test-cfd-wall3d-contract test-cfd-startup3d-contract test-cfd-transient3d-budget
make test-cfd-transient3d-session test-cfd-transient3d-session-sanitize
make test-cfd-transient3d-agent-session
make verify-cfd-transient3d-agent-evidence
make audit-cfd-transient3d-agent-evidence
make audit-cfd-transient3d-completion
```

The final completion command consumes retained checkpoint/regression evidence;
for a fresh checkout also execute the preserved 2D/periodic/C3D-6 commands in
their completion documents and the native source build. Repeated evidence runs
reuse only completed records under the same frozen executable. Generated build
artifacts are local proof, not public releases.

## Stop boundary and next useful gate

Stop before C3D-8. The next bounded physical deliverable is one stationary low-Re
body with a reference appropriate to its actual walls/domain, separate closed
pressure/viscous forces, independent momentum and physical energy checks, three
meshes and boundary-distance sensitivity. Choose geometry/reference before
implementation. Force convergence must pass before STL variety or moving bodies.
Only then compare uniform and targeted local 3D resolution at matched physical
force/wake accuracy and measured cells/memory/time.

These tests do not qualify general wind CFD, arbitrary nonlinear outlet/backflow,
obstacle drag, moving objects, atmosphere, free-surface water, turbulence, GPU or
local adaptive 3D meshes. The established 2D, periodic XYZ and C3D-6 numerical
sources/baselines remain preserved. No shared library API/version/adoption changed.
