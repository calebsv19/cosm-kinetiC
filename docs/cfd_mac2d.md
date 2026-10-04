# S3: staggered 2D incompressible channel

The `cfd_channel_2d` agent template selects `incompressible_mac2d_v1`.
It evolves both velocity components and solves a spatial pressure correction,
behind the existing immutable scene, background session, control receipt, live
inspection, and digest-bound result interfaces. The reduced `cfd_channel`
verification model remains available independently.

## Numerical scope

This is a constant-density, laminar, rectangular channel: periodic X, impermeable
no-slip Y walls with prescribed tangential velocities, and invariant Z. Width
scales physical flux, momentum and energy. Grid is `[Nx, Ny, 1]`, each active axis
4–64 cells. Admission requires the conservative characteristic Reynolds estimate
(including wall speed, pressure-driven peak and diagnostic perturbation) <= 100.
There are no obstacles, open outlets, free surfaces, turbulence closures or AMR.

Pressure is cell-centered; X velocity is stored on unique periodic X faces and
Y velocity on Y faces including the two impermeable wall rows. Adjacent cells
share the same face flux. Momentum uses conservative donor-cell/upwind fluxes on
staggered dual volumes, centered viscous differences, and explicit time stepping.
Adaptive substeps account for advection, viscosity and forcing, with a 1024
substep limit per outer tick. Outer timestep is positive and at most .1 s.
This is a first-order time and advective transport baseline. It does not use the
bounded MacCormack transport in the separate graphics-derived Wind solver;
low numerical diffusion is not yet established for this new conservative path.

The projection solves `-L(phi) = -div(u*)`, then applies `u = u* - grad(phi)`.
The matched divergence and gradient use the same staggered face storage.
Matrix-free conjugate gradients removes the constant nullspace and explicitly
recomputes the true residual before accepting the correction. The correction has
periodic X and homogeneous Neumann Y conditions. Physical split pressure is
`rho * phi / substep_dt` in Pa with zero spatial mean.

`pressure_gradient_pa_m = G` prescribes `-dp/dx`; it enters momentum as `G/rho`.
Reported total pressure combines the computed correction with `G*(L-x)`.
Thus the mean pressure drop remains imposed, while the nonuniform correction is
solved. This is not a computed open-outlet pressure or a drag measurement.
The non-incremental split and its wall pressure conditions need further temporal
and boundary verification before general pressure accuracy can be claimed.
See [MIT staggered projection notes](https://ocw.mit.edu/courses/18-336-numerical-methods-for-partial-differential-equations-spring-2009/b406f49765c802cb34f11df825842451_MIT18_336S09_lec23.pdf)
for the method family and [Vreman's pressure boundary analysis](https://www.vremanresearch.nl/Vreman_JCP2014_ProjectionMethod_preprint.pdf)
for why boundary/splitting accuracy needs a separate check.

## Agent workflow and observations

Use the normal MCP/CLI `scene_create`, `scene_validate`, `run_start`,
`run_control`, `run_inspect`, `run_sample`, and `run_result` tools:

```json
{"scene_id":"channel-2d","template":"cfd_channel_2d","dimensions":[2,1,0.5],"channel":{"pressure_gradient_pa_m":0.1,"wall_bottom_m_s":0,"wall_top_m_s":0,"initial_divergence_perturbation_m_s":0.1}}
```

Pass the returned revision to validation/start, with `grid: [16,16,1]`,
`fluid: {"density_kg_m3":1,"dynamic_viscosity_pa_s":0.1}`, `dt: 0.01`, and a
bounded step count. Runs start paused. The optional perturbation is a bounded
initial diagnostic velocity, not ongoing forcing; omit it for startup from rest.
Template parameters are compiled into the immutable scene extension and checked
against the request. Invalid grids, excess substep budgets, obstacle arguments,
and unrelated Wind qualification controls are rejected.

Live fields include velocity, divergence, vorticity, `pressure_pa`, and
`shear_stress_pa`; `pressure_proxy` is unavailable. Samples disclose cell-based
pressure and reconstructed velocity semantics. Wall probes return prescribed
wall velocity. Health includes actual final divergence, maximum substep pre/post
projection divergence, true pressure residual in s^-1, iteration/substep counts,
projection status, physical flux and wall tractions, streamwise momentum balance,
kinetic energy, and projection energy change. The last two do not constitute a
complete discrete energy budget. Predictor acceleration is not a steady-state
certificate; `steady_status` explicitly remains uncertified.

Terminal `output/channel_fields.json` uses `physics_sim_mac2d_fields_v1` and
includes row-major staggered velocity arrays and pressure correction in Pa with
layout metadata. X-mean profiles have explicit names. SHA-256 provenance uses
the existing result manifest. No desktop menu or packaged installation is changed.

## Verification checkpoint

Reproduce from this checkout:

```sh
make physics_sim_session_worker test-cfd-mac2d test-agent-mac2d
python3 -B scripts/qualify_channel.py --model mac2d --output build/s3-mac2d-acceptance/campaign
```

Saved evidence: `build/s3-mac2d-acceptance/README.md`.

- All 12 agent-driven Poiseuille, Couette, combined and reversed-flow cases pass
  at 16/32/64 wall-normal cells. Nonlinear profile errors decrease by four per
  doubling (observed spatial order 2.0); the linear Couette profile is exact to
  floating-point precision. At 64 cells, Poiseuille maximum velocity error is
  .0000305176 m/s, or .0244% of the analytical peak; flow error is .0488%.
  Wall stresses recover the imposed .2 Pa pressure drop through steady force
  balance. These fully developed cases do not test nonlinear transport accuracy.
- Projection recovers an independently specified discrete pressure in physical
  Pa for two density/timestep combinations, with maximum errors below 6e-15 Pa
  and divergence below 1.4e-14 s^-1. Uniform flow is preserved. A separate
  nonuniform discrete divergence-free field is preserved without a CG iteration.
- A continuous analytical pressure gradient sampled on 8/16/32 grids gives
  pressure L2 errors .00322442/.000803934/.000200844 Pa, approximately second
  order. This verifies spatial pressure projection, not the full transient PDE.
- An unforced 2D perturbation exercises advection, diffusion and repeated
  projection: kinetic energy decays, pressure solves converge and global
  streamwise momentum balances. Quantitative vortex decay remains a next gate.
- A saved coupled agent run reduces initial divergence .3046725656 s^-1 to
  4.0045e-12 s^-1 after one tick. The momentum residual is 1.73e-16 N;
  pressure takes 64 iterations across two substeps. Live sampling, idempotent
  control, transverse flow and terminal staggered arrays are exercised.
- Address/undefined-behavior sanitizers, both MAC agent tests, reduced-channel
  agent tests, session/qualification tests, the stable regression suite, and the
  original reduced 12-case campaign pass.

These are numerical code-verification results, not experimental wind-tunnel
validation. The next bounded gate is a quantitative nonuniform transient solution
with spatial/time refinement, followed by lower-diffusion conservative transport
and pressure splitting/boundary accuracy. Only then extend to obstacle walls and
physical inlet/outlet momentum and surface forces. AMR and turbulence remain later.

Ownership: reuse core_scene compilation and existing session supervision,
inspection and provenance. Discretization remains app-owned; a shared numerical
abstraction is reuse-deferred. No shared API/version/adoption changes are required.

The [nonuniform transient refinement gate](cfd_mac2d_transient.md) is now measured:
temporal order is approximately 1.0, but the spatial velocity gate fails.
Pressure convergence alone is insufficient; the next gate is manufactured
transient forcing to separate transport and wall/splitting errors.

The [manufactured transient campaign](cfd_mac2d_manufactured.md) now provides
known-answer errors and separately controls transport, pressure timing and wall
gradients. A verification-only limited flux reduces fine-grid velocity error
about 22 times; production transport remains upwind pending boundedness and
long-run qualification. Obstacles, outlets and forces remain unqualified.

[Boundary traction and momentum qualification](cfd_boundary_forces.md) now
calibrates physical surface forces and exposes MAC `boundary_force_budget`
wall loads and explicit qualification status. Obstacle geometry, open outlets
and body drag remain unsupported/unqualified, separate from channel wall proof.

The [stationary obstacle projection](cfd_mac2d_obstacle.md) now implements
matched solid-face pressure topology and measured pressure reaction. Leakage,
known-pressure recovery and projection momentum tests pass. Full obstacle
time stepping remains explicitly disabled pending viscous/advective wall
momentum; agent obstacle authoring and open outlets remain unsupported.

The [masked obstacle momentum extension](cfd_mac2d_obstacle_momentum.md) now
advances complete stationary-obstacle steps and accounts for pressure/viscous
reactions. It supersedes the pressure-only stepping restriction. Physical drag,
agent obstacle authoring and open outlets remain unqualified/unavailable.

The [surface-pressure correction and CFD assessment](cfd_boundary_pressure_correction.md)
now reconstructs pressure at obstacle walls and recovers the .075 N baffle
reference at all tested grids. Discrete reactions and unresolved boundary-volume
drive remain separate; arbitrary drag and open outlets are still unqualified.
This remains a 2D CFD core; the existing 3D Wind path is separate.
