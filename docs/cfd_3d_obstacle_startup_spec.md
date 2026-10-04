# Next bounded slice: pressure-driven stationary-obstacle startup

Finite delivery assessment, 2026-10-04. **Transient obstacle implementation is
incomplete.** No startup mode is exposed for boxes in this checkpoint. Existing
steady box scenes remain usable with provisional forces. This specification is
the deliverable-2 fallback permitted by the finite delivery contract; it is not
proof of a new transient capability.

## One case and its physical equations

Use the original aligned 1 m cube, bounds [1.5,0.5,0.5]–[2.5,1.5,1.5] m, in the
4×2×2 m duct. Density is 1 kg/m³; dynamic viscosity is 0.1 Pa s. Initial velocity
is exactly zero. Apply Pin=0.02 Pa for t>0 and Pout=0. No interior body forcing
and no prescribed analytic interior field are permitted.

Solve unsteady incompressible Stokes:

    rho du/dt - mu Laplacian(u) + grad(p) = 0
    div(u) = 0

All body faces and Y/Z walls have u=0. Both X planes retain the existing natural
vector-Laplacian traction condition

    mu du/dn - p n = -P(t) n.

Pressure is in Pa and is solved jointly with momentum and continuity. No separate
periodic pressure projection, artificial wake or downstream velocity prescription
is introduced. This model includes local time acceleration. It omits convective
acceleration u·grad(u); it is distinct from nonlinear Navier–Stokes and validated
wake behavior. Admit the case only if its matched stationary limiting bulk body
Re remains ≤0.1. Do not clip velocity or pressure to satisfy this restriction.

The pressure jump drives a time-dependent flow rate. Do not repeatedly use the
steady fixed-Q response rescaling, which would remove the startup evolution.

## Inspected minimum coupled changes

`src/app/cfd_obstacle3d_mixed.c` already contains an unused `mass` member. Its
`build` currently starts every diagonal at zero; the public obstacle factory
accepts viscosity and body topology but no mass. The unmasked
`cfd_mixed3d_create/set_mass` and `cfd_startup3d_step` demonstrate the existing
coupled mass/history approach. Reuse the compact obstacle maps, B/-Bᵀ, viscosity,
SI residual checks, memory owner and cooperative checkpoints.

For M=diag(actual velocity dual volumes), K=current integrated viscous matrix
and B=current integrated divergence, construct

    (rho alpha M/dt + K) u_new - B^T p_new = rho M history/dt + f_end
    B u_new = 0.

Use alpha=1 and history=u_old for the first BE step. For later BDF2 steps, use
alpha=3/2 and history=2u_old-u_older/2. End normal-X velocity faces retain half
cell dual volumes; eliminated body/wall-normal values remain zero. On an inlet
normal-X face, f_end=Pin*area_x; outlet end load is zero. Pressure constant modes
must remain in the full natural-boundary system: do not impose a zero-mean gauge
that changes the prescribed end-traction pressure level.

The concrete source changes are:

1. Add `cfd_obstacle_mixed3d_create_mass(grid,mu,mass,lo,hi)` and
   `cfd_obstacle_mixed3d_set_mass` to `include/app/cfd_obstacle3d.h` and implement
   them in `src/app/cfd_obstacle3d_mixed.c`. Keep the original factory as the
   exact zero-mass path. Assemble `mass*volume` on each diagonal and rebuild the
   momentum hierarchy when mass changes. Validate mass before mutation. A failed
   hierarchy rebuild is fatal for that private transient solver; it must leave
   published fields untouched and remain safe to destroy. Never resume a partially
   rebuilt operator. Check full B/-Bᵀ adjointness and positive momentum action.
2. Add `include/app/cfd_obstacle3d_startup.h` and
   `src/app/cfd_obstacle3d_startup.c`. Own current and older accepted velocities,
   pressure, private candidates, rhs, dt/time/tick and the requested traction.
   Start from rest, never from the steady solution. Compute histories and loads
   in SI, run the existing full coupled solve, evaluate every candidate control
   privately, and publish fields, histories, observations and time together only
   after acceptance. Propagate checkpoint cancellation and allocator failures.
3. Reuse existing body/wall physical stress reconstruction, but add explicit
   transient budgets. The existing steady momentum residual omits d(momentum)/dt;
   the steady energy residual omits the kinetic-energy rate. Do not present these
   as valid transient checks. The existing mixed `work` includes a mass diagonal
   after extension; subtract `mass*u^T M u` to recover K's discrete dissipation.
   Keep physical 2mu ε:ε dissipation separate from this row-work quantity.
4. Add one dedicated startup fixture and frozen developer runner. Do not expose
   a scene mode until the backend gates below pass. Only then extend
   `include/app/cfd_3d_session.h`, `src/app/cfd_3d_session.c`, a dedicated startup
   observation module, `scripts/agent_session/cartesian3d.py`, service/protocol,
   and `make/sources-tools.mk` / `make/rules-tools.mk`. Keep the current stationary
   cube/box paths and their time-zero semantics intact.

The obstacle pressure preconditioner is currently viscosity-scaled. Its behavior
with a dominant mass term is unproven. First test the one small stated case under
strict complete equations. If it stalls, retain the full residual blocks and
failure packet; do not begin a broad preconditioner or mesh sweep in this slice.
The existing non-obstacle mass-aware pressure support is a reuse candidate for
that specific follow-up, not a reason to weaken equation checks.

## Independent validation and acceptance

Use a separately assembled Python/SciPy MAC matrix on the actual [16,8,8] cube
topology as the time oracle. Enumerate fluid cells and active faces from physical
bounds independently; construct K, B and half-end dual volumes without importing
C matrix entries or fitting to C outputs. Preserve natural end-traction pressure
modes. Let Z span null(B), Mr=rho ZᵀMZ and Kr=ZᵀKZ. A symmetric generalized
eigensolve Kr W=Mr W Λ provides the exact semi-discrete pressure-step response:

    u(t) = Z W [(W^T Z^T f_end) * (1-exp(-Lambda*t))/Lambda].

Recover pressure directly from Bᵀp=rho M du/dt+K u-f_end, with no fitted pressure
offset. Check the oracle's full momentum/continuity equations, dimensions,
positive eigenvalues and stationary limit. This is an independent time-integration
oracle for the stated discretization; it does not establish absolute continuum
cube traction accuracy. Reuse the existing independent rectangular Fourier
startup controls in the body-free solver as a separate continuous known answer.

Required backend acceptance:

- Compare every velocity face and fluid pressure cell at t=0.5 and 1 s for
  dt=0.05,0.025,0.0125 s (choose exact step counts). The finest relative
  volume-weighted L2 velocity and pressure errors against the semi-discrete oracle
  must each be ≤1%; successive time-error ratios must be ≥3. Pressure error is
  evaluated directly in the prescribed-traction gauge. A ratio test below a
  declared roundoff floor is reported unavailable rather than fabricated.
- Every accepted step retains full SI momentum residual ≤1e-11, MAC divergence
  <1e-8/s and cross-section flux variation ≤1e-9 relative to the matched limiting
  flow. The imposed flow is not constant in time. Evaluate the residual with all
  mass/history/end-load terms after reconstruction of the complete candidate.
- Verify exact discrete global momentum including the BE/BDF2 momentum rate.
  Normalize by the 4 m² inlet pressure load, target ≤1e-9. Distinguish row reactions
  from reconstructed physical traction.
- Verify discrete work including time-history effects to ≤1e-9. For BE,
  f·u_new = (E_new-E_old)/dt + rho||u_new-u_old||²_M/(2dt) + u_newᵀK u_new.
  For BDF2 use G_n=rho(||u[n]||²_M+||2u[n]-u[n-1]||²_M)/4 and the additional
  rho||u[n]-2u[n-1]+u[n-2]||²_M/(4dt) term. Do not replace this identity with a steady
  energy balance or mistake temporal dissipation for physical viscosity.
- Track physical kinetic energy, its rate, 2mu ε:ε dissipation, full Cauchy-stress
  end work and transient physical momentum separately. Natural vector-Laplacian
  traction differs from full Cauchy traction by mu(grad u)ᵀn; retain that end-work
  contribution. Original 3% physical-energy /2% physical-momentum screens must not
  be relaxed. Report the unresolved impulsive interval separately, using the
  already declared t≥0.5 s resolved startup interval for these physical screens.
- At a late time chosen from the independently measured slowest decay mode
  (exp(-lambda_min*T)≤1e-4), compare full fields and separate forces with the
  matched existing steady pressure response. Require ≤1% difference; still label
  absolute body force provisional. Compare [16,8,8] and [32,16,16] spatial behavior
  without calling a time-oracle pass a spatial certificate. The steady fallback
  and existing cube/box fields must remain bitwise identical on the zero-mass path.
- Test cancellation before the first acceptance and after an accepted step,
  mass/hierarchy allocation rejection, nonfinite inputs and solver rejection.
  Current/older fields, pressure, force diagnostics and time/tick must remain at
  the last accepted state. Destroyed owner live bytes must be zero. Ordinary and
  ASAN/UBSAN checks must pass.

Declare 128 MiB owned memory, 512 MiB sampled RSS and 600 s per small startup case
before execution. Source session cell budgets remain unchanged. If the proposed
higher spatial case does not fit, retain its admission failure and do not raise
caps retroactively. No reference force gate is bypassed by this startup test.

## Delivery decision and next batch

The current finite checkpoint delivers the stable stationary workflow and this
implementation specification. **No transient-obstacle runtime or validation pass
is claimed.** The blocker is missing masked mass/history support and correctly
qualified transient budgets/oracle, not the separate 1.13–1.20% reference force gap.
Implementing just a mode flag, density parameter or repeated steady solve would
not satisfy the physical request.

Next specific batch: one estimated 4–6 hour backend/validation pass implementing
only the mass factory/history startup and the [16,8,8] semi-discrete oracle above.
Stop at one accepted/rejected startup packet with the stated dt sequence,
conservation and failure publication controls. Source scene exposure follows only
if that packet passes. This is a scope/time estimate, not a performance promise;
report an earlier architectural or numerical failure with its exact evidence.
No nonlinear advection, arbitrary bodies, turbulence, moving solids or further
cube reference mesh experiments belong to that batch.
