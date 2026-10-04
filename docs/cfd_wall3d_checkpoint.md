> Historical checkpoint. The complete A/B/C contract subsequently passed the
> corrected-worker matrix, independent readback and full audit on 2026-09-30.
> See [final completion evidence](cfd_wall3d_completion.md). Progress statements
> below describe their recorded stage and remain retained as defect evidence.

# C3D-7 numerical checkpoint: goal remains active

2026-09-29. Main Edit source only; no commit, package, Desktop refresh or canonical
adoption. The full A/B/C goal is **not complete**. Numerical matrices now pass;
The session/agent adapter now passes focused integration tests; retained agent
matrices and the full completion audit remain required. The contract is [cfd_wall3d_goal.md](cfd_wall3d_goal.md).

## Implemented numerical deliveries

A/B use a shared integrated MAC mixed engine, cached component momentum matrices
and Z-aware sparse MG, matrix-free pressure Schur iteration and true residual/
full divergence rejection. BE startup then BDF2 inertia, vector-Laplacian viscosity
and cell pressure are solved together; no pressure-splitting assumption is inherited
from the accepted fully periodic solver. The independent curl-potential flow has
all three velocities and nonzero wall-normal pressure gradients. Source averages
are continuous analytical functions, not native matrix action on a reference field.
B adds conservative shared dual-face transport with extrapolated velocity and
actual component-flux CFL rejection; its self-transport kinetic work is checked.

C adds two distinct tests: physical pressure-driven rectangular channel startup
from rest with no body forcing, and a nonuniform three-component unsteady Stokes
reference with continuously derived natural tractions at both X ends. End-normal
face unknowns use half momentum slabs. The latter retains its 4 m physical X
wavelength on 4/6/8 m domains, rather than changing forcing when the outlet moves.
Pressure and physical symmetric stress work are reconstructed from solved fields;
vector-Laplacian traction is not mislabeled zero symmetric Cauchy traction.

## Final numerical evidence

Percentages are continuous-reference errors; pressure in manufactured tests uses
a fixed nonzero peak RMS scale so pressure zero crossings stay well-defined.

| Delivery and finest case | Velocity | Pressure | Largest wall-shear error | Physical dissipation | Physical budget imbalance |
|---|---:|---:|---:|---:|---:|
| A Stokes walls, 32 cubed, t=.4 | .1706% | .1333% | 1.9005% | .4911% | .0816% |
| B transported walls, same grid/time | .1708% | .1406% | 1.9008% | .4910% | .0816% |
| C nonuniform open, 64x32x32, t=.4 | .1760% | .1726% | .7791% | .3538% | .1008% |
| C startup, 64x32x32, t=.5 | .7830% | roundoff | .4323% | .9719% | .6675% |
| C startup, same grid, t=2 | .3879% | roundoff | .0848% | .2290% | .4544% |
| C startup, same grid, t=12 | .3223% | roundoff | .0011% | .0616% | .4350% |

Three-grid A/B asymptotic orders: velocity about 1.93, pressure about 2.00,
wall shear about 2.05 and dissipation about 2.01. Time self-difference orders
for velocity and pressure exceed 2.05 in the final pairs. B's independent
one-period harmonic screen has velocity amplitude error .7712% and phase error
.0965 degrees; pressure amplitude error .1430% and phase error .000274 degrees.

The open nonuniform test has final spatial orders 1.977 velocity, 1.982 pressure,
2.004 wall shear and 1.862 dissipation. Coarse dissipation order is only .257;
that grid is not declared asymptotic. Both final time-refinement pairs pass 1.8
for velocity and pressure. The largest common-upstream velocity change across
4/6/8 m is .00601%, pressure change .002325% of .01 Pa, dissipation-per-length
change .00357% and kinetic-per-length change .000368%; all below 1%.

Startup timestep orders approach 2.00 at .5 and 2 s. Pressure is exactly linear
for this flow; its temporal differences are roundoff and no temporal pressure
order is invented. Extending the startup duct leaves flow, individual wall load
per length, kinetic and dissipation per length invariant to roundoff. At t=12
the continuous flow is still .2539% below its limiting steady value; measured
flow differs from the limiting .008 m3/s by .1206%. This is temporal development
of a spatially X-uniform profile, not axial entrance/wake development.

## Observed defect and correction

The first nonuniform-open matrix failed its dissipation-order screen (1.785)
even though velocity, pressure and wall shear converged near second order.
An independent exact-face probe reproduced the slow trend without a solve.
The physical strain observation had treated averaged cell tangential velocities
as point values in reflected wall ghosts. Cubic one-sided cell stencils near
real boundaries and fourth-order centred cross derivatives elsewhere remove
that reconstruction bias. Quadratic face-average profiles reproduce exact centre
gradients; independent composite physical Gauss integration agrees with closed-form
reference E/D within 1e-8 relative. Exact-face fine dissipation reconstruction error
falls from 1.5528% to .5744%. Accepted velocity and pressure evolution is unchanged
by this observation correction. The gate was not relaxed or hidden by a fourth grid.

## Cost, controls and preservation

Serial local optimized fixture measurements (some suites run concurrently):

| Case | Last-step process CPU | Numerical peak | Whole run wall time |
|---|---:|---:|---:|
| A/B 32 cubed, 160 steps | .57 s | 54.80 MiB | 95/97 s |
| Startup 64x32x32, 600 steps to 12 s | .33 s | 100.84 MiB | 190 s |
| Open nonuniform L=4, 80 steps | 1.64 s | 113.14 MiB | 138 s |
| Open nonuniform L=6, 80 steps | 2.59 s | 169.41 MiB | 218 s |
| Open nonuniform L=8, 80 steps | 3.64 s | 225.70 MiB | 314 s |

These are fixture costs, not agent publication costs or hardware-independent
latency guarantees. Candidate solves support cooperative cancellation at outer
and inner Krylov checkpoints. Hashes of every accepted velocity/pressure value
and time remain unchanged on cancellation and memory rejection. The worker
control adapter is now connected and verified by the focused tests below. A BE-to-BDF2 hierarchy rebuild is permitted;
subsequent steps allocate no numerical blocks. Exact peak caps pass and one byte
below rejects/cleans up for all four new modes. ASan/UBSan pass; macOS LeakSanitizer
is unavailable, so deterministic numerical live-byte cleanup is also checked.

The existing periodic 3D spatial/time qualification, session contract, 2D memory
and sparse-MG tests pass. Accepted Cartesian geometry, duct, periodic-transient,
sparse-MG and C3D-6 numerical sources remain unchanged. Native source links; no
GUI visual acceptance or Desktop installation is claimed.

`build/c3d-wall/numerical-checkpoint.json` binds source, binaries and evidence and
independently reads nine packed field files. It recomputes every cell's physical
face divergence, continuous XYZ/time pressure and velocity errors, startup
pressure and section flux, and independent transient startup volume flow. It
explicitly records `goal_complete=false` and `agent_integration_complete=false`.

Reproduce the numerical checkpoint:

```sh
make qualify-cfd-wall3d
python3 scripts/qualify_cfd_wall3d_phase.py
make qualify-cfd-startup3d qualify-cfd-open-transient3d
make test-cfd-wall3d-contract test-cfd-wall3d-sanitize
make test-cfd-startup3d-contract test-cfd-startup3d-sanitize
make test-cfd-transient3d-budget test-cfd-transient3d-budget-sanitize
make test-cfd-wall3d-reconstruction
make qualify-cfd-3d test-cfd-memory test-cfd-sparse-mg test-cfd-3d-session
make physics_sim
python3 scripts/audit_cfd_transient3d_numerics.py
```

The audit consumes the named retained logs from this checkpoint; a fresh checkout
must retain command outputs under the same names, or update the audit input paths
explicitly. Generated outputs stay under build/ and are not release artifacts.

## Required continuation to complete A/B/C

1. Extend the existing `Cfd3dSession` owned adapter with immutable wall-Stokes,
   wall-transport, pressure-startup and nonuniform-open modes. Reuse scene/session
   authority and ownership; retain every old mode. Add the numerical sources to
   the session worker build and the new observation as an adjacent app module.
2. Expose pressure Pa, all three staggered faces including the upper open X face,
   transient physical energy terms, independent reference errors, component wall
   shear, amplitude/phase history and numerical memory/cost. Sampling must share
   the declared boundary/gradient conventions and never advance time.
3. Connect worker cancellation to mixed checkpoints, using only the last accepted
   fields for any publication. Keep numerical failure, cancellation and physical
   reference failure distinguishable. Exercise immutable scene configuration,
   no-advance inspection, invalid/CFL/budget rejection and safe control receipts.
4. Retain agent runs across refinement and open extension matrices. Independently
   read their digest-bound exports, inspect assessments and comparisons, measure
   publication/cancellation costs and run the complete 2D/periodic/open regressions.
   Audit the full original A/B/C contract before marking the goal complete.

No obstacle, moving-body, turbulence, arbitrary nonlinear outflow/backflow or 3D
local-refinement certificate follows from these tests. Stop before C3D-8. The
numerical infrastructure is app-owned and reuses existing geometry, memory and
sparse MG; no shared library API/version/adoption changes were needed.

## Agent integration checkpoint (2026-09-29)

The four immutable source modes are connected through the existing Cfd3dSession,
local worker, Python service and discoverable protocol. Pressure/XYZ inspection,
explicit upper open X face, physical energy, component wall observations,
independent reference assessment, first-period accepted-step harmonic fitting and
numerical memory/cost are exposed. Separate adjacent observation/harmonic modules
keep numerical policy app-owned; existing shared scene/runtime meaning, memory,
Cartesian geometry and sparse MG are reused. No shared API/version change.

`test-cfd-transient3d-session` and its sanitizer target pass: every exported face/
pressure value, physical divergence, energy/wall observations, no-advance samples,
BE/BDF2 cache reuse, full-field cancellation preservation and an independent
Fourier-moment check of the native harmonic fit. The three agent tests pass:
all four templates/validation/inspection/results, immutable/CFL/budget rejection,
and live sampling during an open mixed solve followed by ordered cancellation of
a paused step. Retried cancel receipts match exactly, cancellation is under two
seconds for that bounded fixture, and the entire retained cancelled field equals
an independent one-step reference run. No largest-grid latency guarantee.

The extended worker passes existing periodic 3D, C3D-6 agent and refined 2D agent
regressions; 2D memory/MG/transient/energy/units/mixed and old 3D session checks pass.
All numerical-source digests from the numerical checkpoint remain unchanged.
Logs: `transient-session.log`, `transient-session-sanitize.log`,
`agent-session-tests.log`, `bridge-preserved-regression.log`,
`bridge-periodic-agent.log`, `bridge-open-agent.log`, `bridge-refined-agent.log`.

`verify_cfd_transient3d_agent.py` is currently retaining the A/B spatial/time and
phase, C startup spatial/time and both outlet matrices under a worker-digest
namespace. It preserves failed coarse physical assessments and resumes completed
records without replacing their original cost data. The first three A spatial
exports independently reproduce continuous XYZ/time pressure/velocity errors and
match the native fields exactly. `audit_cfd_transient3d_agent.py` will check the
complete artifact inventory, every field, independent kinetic/wall observations,
refinement/outlet gates, phase readback and scoped agent comparisons after the
matrix ends. **That complete audit has not passed yet. The goal remains active.**

Initial matrix costs include a serial source worker; some early cases overlap
regression processes. A fine wall case takes about 95 s for 160 accepted steps,
with about 4 s cumulative in-solve snapshot service. These are bounded local
observations, not guaranteed performance. Retained matrix evidence records each
run's actual costs. No commit, package, canonical adoption or obstacle work.

### Accepted-diagnostic isolation correction

A checkpoint callback regression exposed transport work/CFL metrics being changed
while preparing a candidate, although accepted fields/time stayed intact. The
adapter now freezes those three physical diagnostics only after successful
acceptance. `accepted-diagnostics-red.log` reproduces the defect;
`accepted-diagnostics-green.log`, native/sanitizer and agent logs verify the fix.
The callback compares the complete accepted energy budget and CFL both during
and after cancellation. Numerical kernels/accepted evolution are unchanged.
Cancelled fields are now exported before terminal publication and command
acknowledgement; the agent test checks immediate field availability and equality.

The pre-correction agent batch was stopped deliberately, including its exact
fixture worker. Its digest namespace is retained as superseded evidence; it is
not the final matrix. A fresh batch uses a frozen executable under its new digest
namespace, so later builds cannot silently change a running matrix's identity.
The current fixed-worker control measurement is about 100 ms live sampling and
230 ms cancel receipt (including about 110 ms full export) for [64,32,32]. This
is one bounded fixture measurement, not a latency guarantee.

Continuation commands after the live matrix is terminal:

```sh
make audit-cfd-transient3d-agent-evidence
```

Then perform the full original A/B/C requirement/source/control/cost/regression
completion audit and update the current-truth documentation. The numerical
checkpoint alone and focused adapter tests still do not complete this goal.

## Dependency and external-agent closure checkpoint (2026-09-30)

The first corrected 47-case agent matrix and its independent all-field audit
passed under worker `b6507faff4adaa30998ed4a74372b98d50cca8e347679ce7ba5cc8023719f25c`.
These establish the numerical/session readback evidence; they are retained as
pre-dependency-correction evidence. A source cost audit then found unbounded
inspection memory growth, so they do not close the full runtime/cost goal.

A paused 8 cubed run with 150 32x32 diagnostic requests grew from 7.06 MiB to
134.77 MiB without advancing time. A native standalone allocator probe reproduced
this in the reduced sample builder and in a minimal JSON array create/put loop.
macOS `leaks` found 43,311,568 leaked bytes after 50 diagnostic samples. The
installed json-c 0.19 container destructor is the cause; the
[upstream 0.19 source](https://github.com/json-c/json-c/blob/json-c-0.19/json_object.c#L412)
places its final container cleanup call inside an assertion, which is omitted
when assertions are disabled. No numerical array or field evolution was involved.
The installed 0.18 archive passes the identical ownership probe.

Main Edit's macOS flags now select the installed 0.18 static archive when
pkg-config resolves 0.19. `PHYSICS_SIM_JSON_COMPAT_PREFIX` can supply its installed
prefix; missing archive fails with an actionable build error. This does not
install/downgrade Homebrew, modify the global opt symlink, vendor a new library,
or create a package. Static binding prevents runtime drift through that symlink.
The optimized worker runs `test-json-container-ownership` before qualification;
its snapshot declares `json_library_version`. Linux cleanup behavior is not
certified by this macOS allocator test.

The 2000-cycle nested/shared-container regression grows live allocation by only
2144 bytes with 0.18 (1 MiB gate). The sustained agent inspection test grows process
RSS by only 16 KiB after warmup over 150 requests (16 MiB gate), with tick/time,
physical energy and numerical allocations unchanged. Five new-mode agent tests
now include actual MCP stdio discovery of all four templates, Pa PNG/probes in
XY/XZ/YZ, transport disconnect/reconnect, field digest readback, cancellation,
immutable/CFL/budget rejection and sustained memory behavior. Native session and
ASan/UBSan tests and old periodic/open/refined/general-agent tests pass again.
Evidence: `json-ownership-green.log`, `json-bound-transient-agent.log`,
`json-bound-native-session.log`, `json-bound-periodic-agent.log`,
`json-bound-open-agent.log`, `json-bound-refined-agent.log`,
`json-bound-session-agent.log`, `inspection-memory-probe.jsonl`,
`inspection-leaks.log`.

The new source worker is
`797c464e8708e0579b08bfa00f5a6dba3caec945f37e59bc9f7f16ba4a1ee995`.
A fresh full 47-case agent matrix is running under this exact frozen identity.
It must finish and pass the independent full readback plus original A/B/C
requirement audit before completion. Old passes and source-only dependency
correction are not substituted for that new runtime qualification. No numerical
kernel changed. No C3D-8 work, commit, package or canonical adoption.

### Terminal supervision correction

The service could read a running snapshot, observe the owner lock released after
completion, then overwrite the already durable completed snapshot with a synthetic
worker-death failure. `_status` now rereads while holding the released owner lock
and preserves actual completed/cancelled/failed publications. Deterministic
red/green owner-boundary tests reproduce the race and prove all terminal outcomes
win; genuine owner death still reconciles once. One affected completed startup
case was repeated with an explicit guarded alias in the old matrix; the original
failure/events/fields remain retained. `verification-retries.json` records exact
predecessor digests and the replacement's identical request fingerprint/fields.

The current invocation for full closure remains:

```sh
make verify-cfd-transient3d-agent-evidence
make audit-cfd-transient3d-agent-evidence
make audit-cfd-transient3d-completion
```

The verifier reuses completed records only under the same frozen executable hash.
Do not start a duplicate while a matrix process is live. After both commands pass,
perform the complete source/control/cost/regression audit and close the docs.

The completion auditor is prepared in `scripts/audit_cfd_transient3d_completion.py`.
It refuses a worker identity mismatch, an unfinished matrix/readback, source or
artifact drift, missing control/cost/cleanup evidence or changed prior numerical
baselines. Its twelve requirement groups cover the full predeclared A/B/C and
common completion contract. It has not been executed successfully yet. The old
2D sparse-MG header is byte-for-byte preserved after removing only the additive
C3D-1 XYZ constructor declaration; its current implementation digest is unchanged.

The new fine A/B agent cases already report approximately 100 MiB process peak
RSS at 54.8 MiB numerical peak, versus over 1 GiB with the leaking dependency.
Their 160-step run times remain about 94/95 s. Final measured costs await the
complete corrected matrix. The default source worker and native source link also
use the static archive; no app was launched or installed for this build proof.
