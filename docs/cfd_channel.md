# Incompressible channel verification slice

Status: local Main Edit source implementation, 2026-09-27. Model:
`incompressible_channel_fv_v1`; scene template: `cfd_channel`.

This is a numerically evolved, fully developed laminar parallel-wall model,
not a general 2D/3D CFD solver. It provides a verified pressure-driven/moving-wall
baseline behind the existing local agent interface. It does not replace Wind,
validate a wind-tunnel outlet, compute obstacle drag, or add turbulence/AMR.

## Physical and numerical contract

Coordinates are meters. The flow is u(y,t) in X, invariant in X and Z; transverse
velocities are zero. Y walls are at 0 and H with prescribed streamwise velocities.
X/Z velocity is periodic. Width W is a spanwise integration extent, not a pair of
additional no-slip walls. Length L defines the imposed pressure difference.

The prescribed parameter G is **-dp/dx** in Pa/m. The gauge is p(L)=0, hence
p(x)=G(L-x) and delta_p=G L. Pressure is an input in this reduction, not an unknown
obtained from a projection or outlet solve. Continuity is identically satisfied
by the reduced field; a reported zero imbalance is not evidence of a general
pressure solver converging.

The solved equation is:

    rho du/dt = G + d(tau_xy)/dy
    tau_xy = mu du/dy

Finite volumes share one viscous stress per Y face. Each internal stress enters
its two neighboring momentum equations with opposite signs. The wall distance is
half a cell. Backward Euler produces a tridiagonal system solved directly in
double precision. The solver starts from rest, never inserts the known analytical
profile, and has no wake forcing, damping heuristic, or velocity clamp.

There is one streamwise face-velocity profile shared by both ends of the reduced
control volumes. Q = W sum_j(u_j dy); outward volume fluxes are [-Q,Q,0,0,0,0].
The same face stresses used by the numerical update produce the reported wall
tractions and momentum balance. This is a reduced conservative momentum update;
it contains no convective momentum transport because streamwise variation is
excluded by the model assumptions.

At each step the diagnostics check:

    d(momentum)/dt = G L H W + (tau_top - tau_bottom) L W
    d(kinetic_energy)/dt = delta_p Q + wall_power
                          - viscous_dissipation - backward_Euler_dissipation

The last term is explicitly reported time-discretization loss, not physical
viscosity. Wall-on-fluid X traction is -tau_bottom at the lower wall and +tau_top
at the upper wall. Fluid-on-wall force has the opposite sign. The pressure drop
recovered from momentum uses the numerical acceleration and wall stresses; it is
an accounting check, not an independent pressure solution.

## Admission and scope limits

- Grid is exactly [1,N,1], 4 <= N <= 256. No hidden cubic-grid expansion.
- Dimensions follow existing scene bounds: each length .1 to 100 m.
- Explicit SI density and dynamic viscosity are required.
- Characteristic Re = rho H (max(abs(U_bottom),abs(U_top)) + abs(G)H^2/(8 mu))/mu
  must be <= 100. This is a deliberately restricted verification range, not a
  general transition criterion. The model cannot represent instability or
  turbulence, regardless of Re.
- dt remains .00001 to .1 seconds and the run limit remains 100000 ticks.
- mu dt/(rho dy^2) must not exceed 1e8 to bound conditioning. Implicit stability
  does not imply timestep accuracy.
- Obstacles and Wind-specific qualification/iteration/budget overrides are
  rejected. `inflow_speed` has no physical role in this pressure-driven template.
- No checkpoint restart, moving geometry, or untrusted/public job submission is
  added. The existing trusted-local process owner and command receipts are reused.

## Agent workflow

Use the existing MCP tools (or the same Python service), with these arguments:

```json
{"scene_id":"channel","template":"cfd_channel","dimensions":[2,1,0.5],
 "channel":{"pressure_gradient_pa_m":0.1,"wall_bottom_m_s":0,"wall_top_m_s":0}}
```

Pass the returned immutable `scene_revision` to `scene_validate` and `run_start`:

```json
{"request_id":"channel-run","scene_id":"channel","scene_revision":"<returned revision>",
 "grid":[1,64,1],"fluid":{"density_kg_m3":1,"dynamic_viscosity_pa_s":0.1},
 "dt":0.05,"steps":800,"start_paused":true}
```

The validation request uses the applicable subset: scene ID/revision, grid, fluid
and dt. `run_control` supports the existing pause/step/continue/cancel contract.
Starting is asynchronous: wait for `paused` before treating initialization as
complete. `run_inspect` exposes the profile, face stresses, pressure semantics,
fluxes, Reynolds estimate, acceleration, and dimensional momentum/energy residuals.
The bounded history records sampled flux and balance diagnostics; it is not a
complete per-step log or a steady-state certificate.

`run_sample` accepts `pressure_pa` and `shear_stress_pa` for this model. Wind
rejects these physical fields and continues to expose only `pressure_proxy`.
Channel rejects `pressure_proxy`. Rich channel samples retain the first nine
field slots, with the proxy slot null, then append physical pressure and shear.
The `fields` and `units` arrays identify values explicitly. Dye is unused (zero).
Live slices/probes reconstruct velocity linearly in Y and evaluate the imposed
affine pressure; the view is not a separately simulated 3D volume.

A completed run writes `output/channel_fields.json`, containing the reduced
profile, face stresses, dimensions, and pressure model. `run_result` includes it
in the existing SHA-256 artifact manifest with scene and worker identity.
Cancellation still yields a terminal snapshot, without claiming a completed field
export. The installed Desktop app and its setup menu are not refreshed here.

## Verification and evidence

Run:

```bash
make test-cfd-channel
make test-agent-channel
python3 -B scripts/qualify_channel.py --output build/my-channel-verification
```

The last command requires a new directory and runs twelve actual agent sessions:
Poiseuille, Couette, combined forcing, and reversed pressure gradient, each at
16/32/64 cells. `build/s3-channel-acceptance/campaign/report.json` records the
final implementation's campaign. Each case retains a digest-bound result manifest.

The independent steady reference is:

    u(y) = U_bottom + (U_top-U_bottom)y/H + G y(H-y)/(2 mu)
    Q = W[(U_bottom+U_top)H/2 + G H^3/(12 mu)]

For L=2 m, H=1 m, W=.5 m, rho=1 kg/m^3, mu=.1 Pa s, G=.1 Pa/m,
stationary walls: delta_p=.2 Pa, exact Q=.0416666667 m^3/s, and each wall exerts
-.05 Pa streamwise traction on the fluid.

| Y cells | Maximum velocity error (m/s) | Flow-rate error (m^3/s) |
|---|---:|---:|
| 16 | .000488281 | .000325521 |
| 32 | .000122070 | .0000813802 |
| 64 | .0000305176 | .0000203451 |

Observed spatial order is 2.0. At 64 cells, velocity error divided by the exact
peak speed is .0244%, and flow error is .0488%. Couette's linear profile matches
to floating-point precision. Wall stress and reconstructed pressure drop match
the analytic values within 1e-10 in the C fixtures.

The startup test uses the independent Fourier-series solution from rest at
T=.5 s, N=256: L2 errors at dt=.1/.05/.025 are .00129133/.000661694/.000334899
m/s, showing the expected first-order temporal trend. This prevents a steady-only
test from hiding inaccurate transient evolution. Tests also cover SI density/
viscosity/pressure scaling, reversed forcing, rest, invalid input, and per-step
momentum and energy residuals below 1e-9 in the tested fixtures. Address and
undefined-behavior sanitizer runs pass.

These are code-verification results for the stated equations and fixtures, not
experimental validation of general wind-tunnel flow. Analytical solutions live in
the test/benchmark harness only, not in the runtime solver.

## Next extension

The [2D staggered extension](cfd_mac2d.md) now carries these face-flux,
pressure-unit and wall-stress conventions into a pressure-velocity solve. It
recovers channel solutions and rejects divergence perturbations. Quantitative
nonuniform transient/refinement tests remain required before adding 3D objects
or open-outlet physics.
This reduced baseline does not itself implement that coupling. Adaptive grids and
turbulence modeling remain later work after those contracts are verified.

Ownership: reuse existing core_scene compilation and the session process/control,
inspection and result infrastructure. The finite-volume discretization and
parallel-wall model are app-owned policy; a shared numerical abstraction is
reuse-deferred. No shared-library API, version, or adoption changes are required.
