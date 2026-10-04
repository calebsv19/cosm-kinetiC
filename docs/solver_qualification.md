# S3 fluid solver qualification

S3A establishes reproducible numerical experiments and diagnoses the existing
solver. S3B now includes opt-in SI viscosity and a matched local projection; S3C comparison runs are
available but predictive drag is **not qualified**. The solver still uses a
collocated projection, voxelized walls, sparse region solves, and approximate
inlet/outlet treatment. Completion of a run is not a physics acceptance test.

## Use the same agent session

`make physics_sim_session_worker` builds the local worker. Existing MCP/CLI
`scene_create`, `scene_validate`, `run_start`, controls, samples, and results
remain the interface. `capabilities` advertises fluid presets and shape templates.
Normal Setup and the legacy Wind example remain available and unchanged.

New templates: `wind_stl_sphere`, `wind_stl_cube`, `wind_stl_cone`, and `wind_empty`.
`object_size_m` is the diameter/length of sphere/cone or cube edge. Cone length
and base diameter are equal, with its tip upstream (-X); cube faces are axis
aligned. Center is at 45% of domain X and 50% Y/Z. Each mesh template produces
an ASCII STL and an identical indexed runtime mesh from one deterministic
triangulation. This is not a new arbitrary-STL importer. The existing
`core_mesh_asset` loader and closed-fill voxelization consume the runtime mesh.
STL and runtime vertices/triangles, edge closure, orientation, and volume are tested.
A discovered 3D obstacle bug is fixed: mesh instances no longer also stamp their
2D compatibility box. Shape-specific off-center slice tests catch that regression.
Worker initialization reports mesh triangle and voxel counts and rejects missing,
unresolved, empty, or budget-limited qualification mesh geometry.
Mesh digests accompany run provenance; modified dependencies fail validation.

Example `scene_create` arguments:

```json
{"scene_id":"sphere-test","template":"wind_stl_sphere","dimensions":[2,1,1],"object_size_m":0.25,"inflow_speed":0.1}
```

Example `run_start` arguments (substitute the returned revision):

```json
{"request_id":"sphere-air","scene_id":"sphere-test","scene_revision":"REVISION","grid":[64,32,32],"steps":1000,"dt":0.01,"fluid":{"density_kg_m3":1.1612,"dynamic_viscosity_pa_s":0.0000185},"qualification_mode":true,"solver_iterations":48}
```

The optional `fluid`, `qualification_mode`, `solver_iterations`, and `dt` also
work on `scene_validate`. Starts remain paused. The existing desktop workspace
can attach using `--agent-workspace /absolute/session-root`; inspection exposes
SI viscosity and Poisson residual while MCP exposes the full structured report.

## Physical parameters and numerical meaning

- Dynamic viscosity mu is in Pa s; density rho is in kg/m3. The solver uses
  kinematic viscosity nu = mu/rho in m2/s. Dye density is a tracer, not rho.
- The new velocity diffusion operator uses alpha = nu dt/h². Explicit diffusion
  subdivides until alpha <= 1/6, with at most 1024 diffusion substeps. Excessive
  cost is rejected rather than silently reducing viscosity. This does not
  adapt the advection/projection timestep.
- Legacy runs without `fluid` retain their original smoothing operator and
  default 20 pressure iterations. A physical fluid can be used with legacy
  Wind forcing for an explicit comparison, but does not make it predictive CFD.
- `qualification_mode: true` disables the synthetic corridor relaxation,
  obstacle wake injection, and extra Wind carrier transport. It leaves the
  existing inlet/outlet writes and sparse scheduling intact. The inlet/outlet
  faces are excluded from solid walls in qualification mode.
  The domain begins from the existing initializer, not a converged inflow field.
- Pressure iterations remain bounded to 8–48 for legacy runs and 8–512 for qualification runs. Legacy Jacobi is fixed-count;
  qualification uses a conjugate-gradient budget with early convergence. Diagnostics expose
  pre/post-projection maximum divergence, discrete Poisson L-infinity residual,
  displacement before clamping, clamp counts, and skipped regions. These are
  maxima over solved clusters before subsequent boundary writes, not final
  domain-integrated conservation or adaptive convergence guarantees.
- Legacy Poisson residual is `max(abs(Laplacian(phi)-div(u*)))` in 1/s.
  Qualification residual is `max(abs(D F u - D F D^T lambda))` in 1/s. The projection
  potential is not a calibrated physical surface pressure. Drag force and Cd
  remain null, with a reason, rather than relabeling existing wake proxies.

## Repeatable experiments

```sh
make test-solver-qualification test-agent-qualification
python3 -B scripts/qualify_fluid.py --output build/my-s3-air \
  --shapes sphere,cube,cone --fluid air_300k --speed 2 \
  --grids 32,48 --dts .02,.01 --duration 3 --iterations 24,48
python3 -B scripts/qualify_fluid.py --output build/my-s3-viscous \
  --shapes sphere --fluid viscous_test --speed .0001 --duration .2
```

`--density` and `--viscosity` override the rounded fluid presets. These are
constant-property incompressible single-phase experiments: selecting water does
not turn this into a free-surface liquid simulation. Air and water presets include
reference conditions; `viscous_test` is a synthetic Newtonian fluid, not a claim
about glycerol or another temperature-sensitive material.

The sweep preserves a separate root per case and equal simulated duration across
timesteps. Each report contains effective resolution, nu, Re, dynamic pressure,
reference area, blockage, object cells across, flow-through time, tick diagnostics,
geometry hashes and worker SHA-256. YZ plane samples and fixed-range PNGs show
upstream and two downstream velocity sections. Statistics describe sampled
fluid cells, not an integrated momentum balance or force. Requested sample planes
are quantized to actual voxel coordinates recorded in each sample.

Use `--compare previous/summary.json` with a new output directory to compute
matched-case changes. Physical parameters and geometry must match; changing the
solver binary is allowed and its digest is retained. Reports never overwrite an
existing experiment. Every sweep is limited to 64 cases and has bounded command
waits. Reports survive cancellation; the tool terminates only its own runs.
Short runs are explicitly flagged as insufficient to establish steady flow.

## References and qualification gates

Drag comparison must specify Reynolds number, reference area, geometry/orientation,
confinement, and physical conditions. `Cd = F/(0.5 rho U² A)` and
`Re = rho U L/mu`; see [NASA drag equation](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/drag-equation/)
and [NASA Reynolds number](https://www.grc.nasa.gov/www/k-12/airplane/reynolds.html).
The initial quantitative drag target is the asymptotic Stokes sphere,
`Cd = 24/Re`, `F = 3 pi mu U D`, restricted to Re <= 0.1, steady unbounded
creeping flow. See [sphere drag research](https://doi.org/10.2514/1.J060153).
Our finite tunnel does not automatically satisfy those assumptions.

Cube/cone and higher-Re sphere cases are repeatable geometry/wake diagnostics,
with numerical Cd targets deliberately unset until a condition-matched reference
dataset and surface-force calculation are admitted. Shape alone does not define
a universal Cd; see [NASA shape effects](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/shape-effects-on-drag/).
Reference values are never inserted into the solver as an obstacle force.
Fluid references: [NIST gas viscosity](https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=926439)
and [NIST fluid properties](https://webbook.nist.gov/chemistry/fluid/).

Numerical tests currently require rest preservation, known cosine-shear viscous
decay and decreasing spatial error, a divergence-reduction case, solid velocity
suppression, and rejection of excessive diffusion cost. These verify bounded
operator behavior, not general solver correctness or drag convergence.

Next S3B boundary: consistent pressure/divergence/gradient operators and boundary
conditions, convergence-based pressure solving, and physical surface stress or
control-volume force accounting. Then S3C must demonstrate grid, timestep,
blockage and duration independence against condition-matched drag/wake data.
Moving objects (S4) and atmospheric/free-surface models (S5) remain later work.

## Architecture

Reuse adopted: existing core_scene_compile, core_mesh_asset geometry loading,
core_sim step ownership, core_units meter conventions, core_font/viewport
presentation, and session lifecycle/provenance. Qualification geometry policy,
reference equations, solver numerics, and experiment orchestration are app-owned.
New shared abstractions are deferred; no shared APIs or minimum versions change.

## S3B matched local projection

Qualification mode now uses the exact transpose of the existing centered
clamped-domain divergence instead of combining centered gradient/divergence
with an unrelated nearest-neighbor Laplacian. The wall constraint F zeros
velocity in solid cells and the normal component in fluid cells touching a
solid in either direction. Solve `(D F D^T) lambda = D F u` using matrix-free
CG, then subtract `F D^T lambda`. No subsequent directional wall clamp runs
in this path. This conservative wall constraint can thicken voxel obstacles;
it is not cut-cell geometry or a full no-slip discretization.

CG stops at local L-infinity residual `max(1e-7, initial_max * 1e-6)` or the
requested iteration budget. The diagnostic recomputes the true residual from
the operator. Independent tests compare it with divergence of the actual
float velocity output, require at least 1000-fold reduction for smooth 12-cubed
fields with and without an obstacle, assert exact wall constraints, and check
that kinetic energy does not increase. Scratch cost is 59 bytes/cell allocated
per projection, in addition to existing solver storage. This is an initial
correctness implementation; reusable scratch/preconditioning remain future work.

`health.projection_operator` identifies the operator in run snapshots.
The potential remains uncalibrated: the boundary transpose differs from a
physical prescribed-pressure open boundary. Cell-centered checkerboard modes,
sparse cluster interfaces, and later inlet/outlet writes remain limitations.
This local algebra repair does not establish global mass conservation or
physical drag. Next gate: grid/timestep and steady-state qualification, physical outlet
boundary treatment, then force/momentum budgets.

Numerical-method background: [Bridson and Mueller-Fischer course notes](https://www.cs.ubc.ca/~rbridson/fluidsimulation/fluids_notes.pdf).
Our current implementation is the explicitly described collocated transpose
operator above, not that reference's staggered-grid implementation.

### Wind port and final-state conservation repair

Previously all six domain faces were marked solid, including authored Wind inlet
and outlet ports. Qualification now opens those two faces (and all faces for the
existing open-wall policy), preserving the legacy path. Tests sample both ports
and require actual fluid cells. The empty tunnel is a required comparison case:
a tiny residual with no through-flow must not pass as a working tunnel.

`health.conservation` reports bounded final-snapshot diagnostics for Wind volumes
up to 262144 cells; `available=false` explicitly represents unavailable data.
Interior face velocity is the average of neighboring cell velocities, solid-face
flux is zero, and exterior flux uses the boundary cell velocity. Six signed
outward fluxes are ordered min-X, max-X, min-Y, max-Y, min-Z, max-Z (m3/s).
Their sum and the integrated diagnostic divergence agree by discrete cancellation.
The maximum absolute final divergence (1/s) is separate from the local projection
residual. With an explicit fluid, multiplying volume flux and integrated kinetic
energy by carrier rho gives kg/s and joules. These are sampled-state diagnostics,
not conserved transport fluxes or a validated force integral. Legacy throughput
is dye-weighted and must not be interpreted as physical carrier mass flux.

Analytic tests verify boundary flux, the divergence theorem with and without
solid cells, and nonfinite rejection. Projection tests also verify preservation
of total cell-centered momentum without walls. Solid-wall force/impulse accounting,
consistent post-projection inlet/outlet treatment, larger-grid diagnostics, and
pressure/viscous surface stress integration remain open S3 work.


## S3B prescribed inlet and global pressure coupling

Qualification Wind runs now solve one full-domain pressure problem. A run whose
full grid exceeds `solver_cell_budget` fails validation with
`qualification_global_solver_budget_exceeded`, rather than silently skipping
regions. Existing legacy sparse scheduling is unchanged.

At the authored inlet face, fluid-cell velocity is prescribed along the inward
normal at `inflow_speed`, with zero tangential components. Solid constraints take
precedence for blocked cells/components. These fixed components are excluded from
the pressure correction; their flux remains in the right-hand side. The outlet
remains free under the matched transpose operator. This is an affine constrained
velocity projection, not yet a calibrated physical-pressure outlet. No new
surface-force or Cd claim follows from this boundary change.

Tests recover uniform 2 m/s empty-channel flow in all six inlet orientations,
require exact inlet values after projection, verify balanced final flux, and
exercise convergence failure under a one-iteration budget. Agent tests verify
actual inlet samples, global-budget rejection, and the larger pressure budget.

Qualification `solver_iterations` accepts 8–512; legacy retains 8–48. This is a
maximum, with residual-driven early exit. Snapshots expose
`health.projection_iterations_used`, `projection_unconverged_regions`, and
`projection_status` (`not_solved`, `not_converged`, or `converged`). Convergence
refers to the local linear-system tolerance, not physical validation or steady
state. `physics.boundary_model`, inlet/outlet face and inlet speed describe the
boundary contract. Operator identity is `prescribed_inlet_transpose_cg_v2`.

The 48-iteration ceiling is insufficient at some resolutions; choose a larger
explicit budget and inspect convergence instead of trusting tick completion.
Pressure tolerance, grid/timestep convergence, physical outlet treatment, and
wall stress/force measurement remain separate acceptance gates.


Wake reports now include `central_wake`: an unweighted fluid-only sample average
in a D-by-D square centered on each YZ section, with its exact physical width and
sample count. Whole-section mean velocity measures transport and can remain
unchanged around an obstacle when conservation holds; it is insufficient to
measure wake deficit. The central aperture supplements existing minimum velocity
and reverse-flow fraction. It is not an integrated force or drag coefficient.


## S3 wake, refinement, and outlet acceptance screens

`qualify_fluid.py` now records coherent near/far wake slices throughout each run,
not just a final frame. The raw slices and tick-bound summaries survive the live
session's bounded sample cache. `wake_series.json` is updated during the run;
`report.json` contains the series, exact criteria, `steady_screen`, and solver
history. A successful process or pressure solve is not a steady-state result.

Default project screening settings (engineering choices, not universal CFD
standards):

- Wait three domain flow-through times **before** two trailing one-second windows.
- Require at least five wake samples per window and adequate time coverage.
- Near/far central and whole-section means must drift by at most 1% of inlet speed
  between windows; their within-window range must be at most 2% of inlet speed.
  Reverse-flow fraction uses the corresponding absolute fraction tolerances.
- Require converged pressure, no clamps/skipped regions, available finite flux
  diagnostics, nonzero through-flow, and relative net flux imbalance <= 1e-4
  throughout the assessment windows.

A pass is named `steady_window_screen_pass`. Oscillation, drift, missing data,
pressure nonconvergence, or clamp activity prevents it. This does not certify
statistical stationarity for an inherently unsteady wake, or demonstrate
physical accuracy. A longer record and statistical analysis may be required.

```sh
python3 -B scripts/qualify_fluid.py --output build/my-steady-case \
  --shapes sphere --speed 2 --diameter .3 --grids 32 --dts .02 \
  --duration 7 --iterations 256 --sample-every .2 --steady-window 1
```

The CLI also accepts `--mean-drift-tolerance`, `--wake-range-tolerance`,
`--flux-tolerance`, and `--warmup-flow-throughs`; their values are preserved in
reports. Sparse or short records return `insufficient_evidence` instead of a
pass. Sampling is rounded to an integer number of solver steps, and the actual
interval is recorded. No automatic early stopping hides a failed assessment.

`scene_create.object_center_m` allows a fixed obstacle position when extending
the outlet. It is validated against the domain bounds and included in immutable
scene identity. The experiment runner keeps the object at (0.9,0.5,0.5) m and
near/far target planes at x=0.9+D and x=0.9+3D; it rejects stations outside the
domain rather than silently clamping them. `--domain-length` changes only the
X extent. For example, grid 32 over 2 m and grid 48 over 3 m both use h=0.0625 m.
The actual nearest-cell sample locations remain in every slice.

```sh
python3 -B scripts/assess_fluid.py --kind outlet \
  --output build/outlet-assessment.json SHORT/report.json LONG/report.json
python3 -B scripts/assess_fluid.py --kind temporal \
  --output build/timestep-assessment.json COARSE/report.json MEDIUM/report.json FINE/report.json
```

`--kind spatial` likewise requires at least three distinct resolutions. Temporal
and spatial screens require matched fluid, geometry, object position, solver
identity/budget, duration, window criteria, and observation setup, with only the
selected independent variable changing. Outlet studies require at least two
lengths at the same cell size/timestep. All cases must pass the steady screen.
Near and far trailing-window central means must differ by no more than 2% of
inlet speed (`--tolerance`), and successive differences must decrease when three
or more levels exist. This is a finite-level sensitivity screen; it does not
compute observed order, Richardson extrapolation, or a Grid Convergence Index.

The distinction between iterative, spatial, and temporal verification follows
[NASA's verification assessment guidance](https://www.grc.nasa.gov/WWW/wind/valid/tutorial/verassess.html).
For formal discretization uncertainty, see the separate
[spatial convergence procedure](https://www.grc.nasa.gov/www/wind/valid/tutorial/spatconv.html).
The project thresholds above are not taken from those sources.

Every assessment continues to report `physical_outlet_status: not_qualified`
and `force_measurement_ready: false`. Numerical sensitivity passing is necessary
but does not validate the current uncalibrated pressure outlet or authorize
physical pressure/viscous drag claims.

## Transport derivation and outlet predictor audit

See [solver_accuracy_audit.md](solver_accuracy_audit.md) for the inviscid transport
amplification derivation, finer-grid/smaller-timestep evidence, and outlet repair.
Qualification receive outlets now copy adjacent interior velocity into their
predictor instead of resetting it to a uniform target each tick. The subsequent
projection remains unchanged; this is not a physical pressure-outlet condition.
The six-orientation regression covers the copy. The analytic transport test
characterizes current interpolation loss, not acceptable CFD accuracy. All
refinement/outlet physical-acceptance gates remain closed on the recorded runs.

## Bounded velocity transport

The qualification lane now uses the [bounded transport method](transport_accuracy.md).
The previous-method amplification assertion is replaced by independent continuum
amplitude/phase budgets, with a transported viscous case and explicit wall and
bounds tests. Snapshot health identifies the method and reports correction,
limiter and fallback component counts. Original audit evidence remains immutable;
new runs have new worker hashes and must not be mixed with it in refinement studies.
