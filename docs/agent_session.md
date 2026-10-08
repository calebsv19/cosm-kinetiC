# Local agent simulation sessions (S1 and S2)

The source checkout and Main Edit package provide a trusted-local session service.
An agent and the native workspace can share one background solver without owning
its lifetime. This is not a remote submission API. Source sessions support approximate Wind and the reduced/2D incompressible
channel verification models, plus the bounded 3D source models below. Water and atmosphere session authoring are future work.
Installed package capabilities depend on its bundled source checkpoint.

## Bounded 3D source models

`cfd_duct_3d`, `cfd_manufactured_3d` and `cfd_open_duct_3d` expose the distinct
`incompressible_cartesian3d_v1` model through the same trusted-local source service.
The duct accepts `channel.volume_flow_m3_s`; it solves the pressure jump at fixed
flow with periodic X and no-slip Y/Z. It requires `steps=1` and keeps time zero.
The manufactured transient is a continuously forced, fully periodic XYZ
verification flow. `cfd_open_duct_3d` is a stationary coupled MAC Stokes solve
with analytic developed inlet, no-slip Y/Z and natural vector-Laplacian traction
outlet; mode `steady_open_duct` also requires one step and keeps time zero. It
accepts the same volume-flow parameter; use 128 MiB for the qualified [64,32,32]
case. No template supports arbitrary obstacles or general transient/wake outlets.
All require explicit SI density/viscosity. Grid is `[Nx,Ny,Nz]`, each 4..256 with
at most 262144 cells. `numerical_memory_limit_mib` and `solver_cell_budget` admit
bounded runs; the numerical cap excludes JSON and total RSS.

For example, through the existing JSON/MCP tools:

```json
{"scene_id":"duct3d","template":"cfd_duct_3d","dimensions":[4,2,2],"channel":{"volume_flow_m3_s":0.008}}
```

Use its returned immutable revision with `run_start`, grid `[64,32,32]`, steps 1,
explicit fluid `{"density_kg_m3":1,"dynamic_viscosity_pa_s":0.1}` and a 64 MiB cap.
`run_assess` separates numerical convergence from duct reference acceptance.
`run_compare` provides matched spatial/time sensitivity; stationary duct temporal
comparisons are rejected. `run_sample` supports XY/XZ/YZ, world probes and
`pressure_pa`; pressure_proxy is unavailable. Sampling never advances the run.
Staggered velocity and cell-pressure artifacts declare their locations and units.
`shear_stress_pa` samples the XY component, not a scalar 3D shear magnitude.
Open fields additionally export the upper X face in `outlet_x_velocity_m_s`;
consumers must not wrap X. Open assessment includes physical work imbalance and
flux conservation. Pressure samples are containing-cell solved pressure, distinct
from second-order boundary traces in physics diagnostics. Cached solver controls
remain between solves; the qualified open solve takes about 11 seconds locally.
See [C3D implementation/evidence](cfd_3d_completion.md) and
[C3D-6 boundary evidence](cfd_open3d_completion.md) for tests and limitations.

## C3D-7 transient source modes

The Main Edit source adapter now also admits four immutable templates:

| Template | Solve mode | Default dimensions (m) |
|---|---|---|
| `cfd_wall_stokes_3d` | `wall_stokes_transient` | 2 x 2.5 x 3 |
| `cfd_wall_transport_3d` | `wall_transport_transient` | 2 x 2.5 x 3 |
| `cfd_pressure_startup_3d` | `pressure_startup` | 4 x 2 x 2 |
| `cfd_open_wall_transient_3d` | `open_wall_stokes_transient` | 4 x 2 x 2 |

They reuse `incompressible_cartesian3d_v1`, explicit SI fluid parameters and the
existing scene revision, run identity, budget and result-artifact contract. Wall
modes are independently forced known-answer tests; startup applies natural end
pressure tractions from rest, with `channel.volume_flow_m3_s` selecting the
continuous limiting steady reference. Open wall mode keeps a fixed 4 m physical
X wavelength; its declared domains are Lx=4,6,8,... m. These are controlled
laminar verification cases; arbitrary geometry, wake exit and backflow remain
unqualified. All four advance physical time with coupled BE/BDF2 steps.

Inspection exposes Pa, eight component wall-shear RMS observations for wall modes
or four integrated wall loads for startup, physical strain dissipation, boundary/
body/transport power, kinetic energy/rate and separate independent reference errors.
`run_assess` distinguishes numerical readiness, reference accuracy and harmonic
accuracy. Startup reference acceptance is unavailable before the resolved
reference interval, .5 s for the default fluid/domain. Wall harmonic assessment
needs the first full 1 s period; the fit uses every accepted step, independently
of the bounded publication history. A single passing run does not certify
refinement, outlet distance or arbitrary CFD.

The new modes support cooperative cancellation inside inner/outer mixed Krylov
iterations. The next ordered, revision-bound cancel is observed at checkpoints;
its receipt is finalized at the safe boundary. An interrupted paused `step`
receives `status=cancelled`, followed by the next cancel's own applied receipt.
Pause/continue remain step-boundary controls. There is no queue bypass: a command
before cancel must first be processed. Terminal runs cannot restart.

During a solve, bounded diagnostic requests and snapshots are serviced on an
approximately 100 ms checkpoint polling cadence. `solve_in_progress` refers to
private candidate work; tick, time, pressure and preview always refer to the last
accepted state. This cadence is not a maximum-grid latency guarantee. Runtime cost
reports cumulative `checkpoint_service_wall_ms` and `checkpoint_service_count`,
ordinary previous publication and final export separately. Step wall/CPU cost
includes any in-solve service work. Cancellation preserves accepted fields and
exports them before terminal cancellation publication/acknowledgement; allow the
owner to finish its final publication before
expecting a complete result-artifact inventory.

Five agent tests cover the four source modes, actual MCP stdio reconnect,
sustained inspection memory and safe controls. Native session/harmonic and
sanitizer tests pass. The final 47-case matrix, independent field/refinement/
outlet readback and full C3D-7 audit pass. The macOS source build binds a tested
static 0.18 JSON archive after isolating the installed release 0.19 destructor
leak. `test-json-container-ownership` runs before the optimized worker build;
snapshots expose `json_library_version`. See [complete evidence/limits](cfd_wall3d_completion.md).
No Desktop package update is implied by these source capabilities.

## C3D-8 stationary obstacle development mode

Main Edit source now also accepts `cfd_obstacle_3d` / `steady_obstacle_duct`.
It fixes a stationary aligned 1 m cube centered at Y=Z=1 in a 2 x 2 m duct;
`channel.center_x_m` defaults to 2 and `volume_flow_m3_s` to .008. Both X ends
have natural vector-Laplacian pressure traction, with Pin solved from Q and
Pout=0. This is creeping Stokes, without inertial transport. Fluid parameters
are explicit SI; scene admission rejects mean body Reynolds number >.1,
nonaligned body faces or less than two fluid cells between body and outer walls.
Use dimensions `[4,2,2]`, grid `[16,8,8]`, one step and a 32 MiB numerical cap
for the small exploratory run. Physical time remains zero; temporal comparisons
are rejected. The existing 262144-cell limit is unchanged.

Snapshots expose all six signed closed-face pressure and physical viscous loads,
XYZ totals, outer-wall forces and momentum residual; exact discrete row reactions
are separate. Physical strain dissipation/boundary power and discrete diffusion
have separate balances. XYZ planes/world probes, `solid`, Pa and centerline
recovery are non-mutating. Field artifacts declare fluid/solid cells, null solid
pressure, lower XYZ velocities and the separate upper X face. Upstream probes
use `run_sample`; pressure deficit is evaluated against the matched empty-duct
background in the multi-run audit.

Actual MCP discovery/create/start/sample/step/assess, comparison, memory rejection,
full-field readback and in-solve sample/cancel are tested. `run_start` uses
`request_id` through MCP; retain the returned run identity/revision for controls.
Candidate solves remain private; cooperative cancellation preserves accepted
fields. Single-run `run_assess` keeps `reference_accuracy.status=not_established`
and `physical_accuracy_certified=false`, even when numerical residuals pass.
The body-edge correction exposes continuous half-cell strain quadrature,
interpolant L2 divergence, legacy quadratic viscous force and legacy cell-gradient
dissipation alongside the corrected physical measurements. Numerical and finest
physical momentum/energy budget gates pass. Independent references now pass small
last-grid changes but fail genuine normal-direction refinement; matched native
pressure-force estimates remain provisional. Offline exact reference-pressure
projection separates trace reconstruction from solved-pressure force-functional
error without changing agent observations or certification authority. A tiny
MAC divergence does not certify pointwise divergence of the interpolated field.
This is an exploratory source mode, not certified obstacle drag, a general
STL/moving-body solver, or a turbulent wake model. See
[reference robustness evidence and continuation](cfd_obstacle3d_reference_refinement_checkpoint.md).

The initial reliability pass adds `normal_viscous_trace_policy` and
`shares_unrestricted_stress_with_energy_interpolant` to obstacle force snapshots.
The imposed normal wall identity differs from the energy interpolant's raw stress.
Capabilities disclose template geometry restrictions and last accepted inspection.
Native admission and post-flow-scaling candidate validation are strengthened without
changed ordinary fields. See [initial improvements](cfd_3d_initial_improvements_checkpoint.md).

## Explicit stationary-box source workflow

The optional Main Edit template `cfd_box_3d` selects `steady_box_duct` and requires
`channel.body_min_m` / `channel.body_max_m`, each three physical metre bounds.
It preserves the 2×2 m duct, exact grid alignment, at least two body/fluid cells
to each outer plane, bulk body Re≤0.1 and one stationary solve step. Physics is
creeping incompressible Stokes with natural end pressure tractions and no-slip
body/walls; no inertia/advection or transient-wake claim. `center_x_m` remains a
legacy-cube parameter and cannot accompany explicit box bounds.

The actual source CLI/MCP authoring, admission, controls, observations, export and
spatial comparison include this template. Snapshots use actual six face areas,
projected area/characteristic length, body-centred recovery and XYZ momentum
residual. `run_assess` separates numerical convergence, physical budgets and
unestablished absolute force accuracy. The session cell limit remains 262144.
Terminal stationary runs use retained fields/results for pressure/velocity
inspection; new live sampling after termination is unavailable.

The separately named source worker and one end-to-end [48,24,24]→[96,48,48] scene
are reproducible via `scripts/run_cfd_box_scene.py`;
`scripts/inspect_cfd_box_scene.py` verifies retained hashes and produces local SI
field previews/probes and the comparison report. These developer commands use
the existing NumPy-capable environment. Full exported-field native equation
readback, actual MCP box tests and focused cube/Cartesian regressions pass.
Forces remain provisional; the example force changes still exceed 1%. See the
[runnable stationary-box checkpoint](cfd_3d_box_checkpoint.md) for exact build/run
commands, observed limits and the incomplete transient-obstacle boundary. This
is Main Edit source capability; no desktop package or protected-worker update.

## Build and open

```sh
make physics_sim_session_worker clang-build
python3 scripts/physics_sim_session.py capabilities
build/clang/physics_sim --agent-workspace
```

The ordinary desktop menu also has **Live inspection [F8]**. Use Wind example to
create, validate, and start a paused template; use Continue, Pause, Step, Cancel,
and Fit. Pan and zoom operate while a separate process advances the solver.
The workspace attaches to the most recent run in its session root.

## MCP connection

Configure a trusted local MCP client with command `python3` and arguments:

```json
["/absolute/checkout/scripts/physics_sim_session.py", "--root", "/absolute/session-root", "--mcp"]
```

Build the worker first. Optionally pass `--worker /absolute/physics_sim_session_worker`.
The packaged launcher also accepts `--agent-mcp` and selects its bundled worker
and scripts. Python 3 is required on the host. No client configuration is changed
automatically. Stdio carries newline-delimited JSON-RPC; stdout is protocol-only.

To share an explicit root with the desktop, launch
`physics_sim --agent-workspace /absolute/session-root`, or set
`PHYSICS_SIM_SESSION_ROOT` for both clients. Without an override, source sessions
use `data/runtime/agent_sessions`; packaged runtime namespaces have separate roots.

Eleven discoverable tools provide session control and live inspection:

1. `capabilities`: discover models, templates, limits, and semantics.
2. `scene_create`: create an immutable `wind_box`, `wind_sphere`, `cfd_channel`, or
   `cfd_channel_2d`, or exploratory `cfd_obstacle_2d` template with
   dimensions and inflow speed. Retain the returned scene revision.
3. `scene_validate`: initialize the real solver, report effective grid and storage
   estimate, without advancing time.
4. `run_start`: bind a request ID, scene revision, grid, timestep and tick limit.
   Starts paused by default. The request ID becomes the run ID.
5. `run_control`: pause, step, continue, or cancel with a unique command ID and
   the expected scene revision. Repeat the same ID to collect a pending receipt.
6. `run_inspect`: read status, health, snapshot age and optional bounded log tail.
   `preview: true` adds an XY velocity-magnitude PNG and sampling metadata.
7. `run_events`: read up to 100 events after a cursor, with an optional bounded wait.
8. `run_result`: obtain terminal status and SHA-256 artifact provenance.
9. `run_sample`: request a bounded slice and world-space probes; see S2 below.
10. `run_compare`: compare matched completed MAC runs in space or time; see
    [CFD agent lab](cfd_agent_lab.md). Sensitivity is not physical acceptance.
11. `run_assess`: assess open CFD numerical readiness with separate steady, conservation,
    energy, force-consistency and matched-refinement gates; see [acceptance](cfd_run_acceptance.md).

The same operation names and JSON arguments work through the CLI, for example:

```sh
python3 scripts/physics_sim_session.py scene_create '{"scene_id":"example","template":"wind_sphere"}'
# Substitute the revision returned above:
python3 scripts/physics_sim_session.py scene_validate '{"scene_id":"example","scene_revision":"REVISION"}'
python3 scripts/physics_sim_session.py run_start '{"request_id":"example-run","scene_id":"example","scene_revision":"REVISION","steps":100}'
python3 scripts/physics_sim_session.py run_inspect '{"run_id":"example-run","preview":true,"log_tail_lines":20}'
```

## Ownership and control contract

States are starting, running, paused, completed, cancelled, and failed. Only the
solver process mutates fields. Commands apply at safe tick boundaries. Step
requires paused and advances one fixed tick, including configured internal
substeps, before pausing again or completing at the requested limit. Pause and
cancel may wait for an expensive tick; their pending receipts are not an
acknowledgement. Terminal runs cannot be continued. Continuing a live paused
process is not checkpoint restart.

There is one active run per root. Starts and commands are serialized under a
local file lock. Identical ID retries reuse their outcome; conflicting arguments
fail. Scene and executable digests bind each immutable run request. Editing a
source scene cannot change an active run. Closing the desktop or disconnecting
MCP leaves the solver running; reconnect to the same root to inspect/control it.
A dead worker becomes failed and does not block admission of a new run.

Snapshots atomically bind run ID, scene revision, tick, time and publication
sequence. The sparse XY preview samples at most 64 by 64 cells and does not
materialize the full volume. It is a diagnostic slice, not a rendered 3D volume.
Completed runs export final VF3D fields and a pack through the existing export
path. Cancelled/failed runs preserve diagnostics and provenance. `run_result`
returns an artifact manifest; these files are not restart checkpoints.

## Model and acceptance boundaries

Wind remains approximate: pressure is a projection/wake quantity and drag is a
proxy. This work establishes control and observation, not validated predictive
CFD, moving 3D boundaries, atmospheric microphysics, or a free-surface liquid
solver. Preview ranges and actual resolution are reported explicitly.

`make test-agent-session` exercises real worker lifecycle, concurrent clients,
ID conflicts, stale revisions, bounded preview, MCP disconnect/reconnect,
completion exports, budget failure, process death and artifact provenance.
`make test-stable` supplies existing solver/export/regression coverage. Native
workspace proof additionally measures navigation and frame gaps while the solver
runs expensive ticks; package self-tests are a separate acceptance plane.

## S2: live field inspection

`run_sample` is a ninth MCP tool for independent diagnostic requests. For example:

```json
{"run_id":"example-run","request_id":"inspect-001","plane":"XZ","position":0.5,"resolution":48,"field":"vorticity","vectors":true,"points":[[0.2,0.5,0.5]],"wait_ms":1000}
```

Slices support XY, XZ, and YZ. `position` is a normalized location along the plane
normal (0 to 1, inclusive); the response reports the actual cell index and world
coordinate. `resolution` caps each slice dimension at 4–64 samples. Fields are
speed, dye, solid mask, vx/vy/vz, pressure_proxy, divergence, and vorticity magnitude.
Every response includes field names, units, sample statistics, world bounds,
run/revision identity, sample tick/time/age, and up to 16 XYZ-meter probes. Probes
outside the domain report `inside: false` rather than silently clamping.

MCP returns a compact image and metadata, omitting the raw slice array. CLI
responses retain the numeric array. Optional `color_range: [min,max]` keeps the
same range for comparisons; otherwise the current sample min/max sets the range.
`vectors: true` adds in-plane velocity arrows normalized to the slice speed maximum.
White marks solids and magenta marks nonfinite field samples. Statistics include
solids and describe the sampled slice, not the entire domain. Local divergence
and curl use neighboring cells (one-sided at domain edges); obstacle interfaces
are not wall-corrected. These are diagnostic estimates, not solver residuals.
Pressure values remain solver proxies with no claim to pressure in pascals.

A pending response is retrieved by repeating the same request ID and parameters.
Sampling never advances simulation time. Each request is fulfilled by the owner
at a safe boundary; two requests at most are processed per boundary. The service
allows eight pending requests and retains 32 diagnostic request/result pairs.
Abandoned pending requests expire after 30 seconds when new requests are admitted.
An evicted ID can be reused for a fresh observation; diagnostic requests are not
permanent command receipts. New live samples are unavailable after the worker
terminates; retained samples and the final exported artifacts remain available.

`run_inspect` with `history: true` returns up to 128 distinct published-tick health
samples, including divergence, maximum speed, velocity clamp count, and tick
runtime. Publication history may skip ticks in fast runs. This is a bounded recent
history, not a full trajectory archive or restart checkpoint.

The desktop exposes field cycling, plane selection, slice position +/- controls,
vector overlay, auto/locked color range, and a centered view. Right-click a slice
cell for its sampled value and velocity; pan/zoom and run controls remain active.
Requested and displayed planes and sample ticks are shown separately while a
safe-boundary observation is pending. The side panel includes a recent divergence
plot and quantitative color legend.

The workspace uses shared `core_font` body/header/caption sizes and paths, with
an SDL presentation adapter that rasterizes glyphs at the drawable pixel density,
caches 96 text textures, and reloads fonts on display-density changes. Logical UI
size stays fixed on Retina displays. Existing `core_viewport2d` owns navigation;
fluid sampling, request budgets, and field presentation remain app-specific. No
shared library APIs or minimum versions changed.

Additional acceptance: `make test-session-observation` checks a manufactured
linear velocity field with known divergence and curl on all three planes,
including domain-edge stencils. `make test-agent-session` checks slices, probes,
retention, concurrency, MCP images, and non-mutating observation alongside S1.

Use Ctrl/Cmd + or - to adjust workspace text size, and Ctrl/Cmd 0 to reset.
The default is 125% of shared font-role sizes. Source and packaged runs resolve
the same vendored font assets. Runs started by an older S1 worker must be replaced
with a new run to use S2 live sampling; controls remain available for the old run.

## Setup and inspection navigation

Normal Desktop launch opens the original setup shell. Existing preset selection,
mode configuration, 2D/3D selection and the original simulation launch paths remain
there. Live inspection [F8] opens the independent background Wind session workspace.
Setup & modes [Esc] returns to the original shell, preserving its in-memory setup
when entered from that shell. The explicit --agent-workspace shortcut also has a
working Setup return; it is an optional direct entry, not the application default.
Returning to setup leaves the background session alive. Wind example creates a
separate Wind template: it does not silently run the currently selected preset.
Existing preset/mode runs have not all been migrated to the agent session runtime.

## S3 numerical qualification

See [solver_qualification.md](solver_qualification.md) for deterministic STL sphere,
cube, and cone templates, editable SI fluid properties, opt-in runs without synthetic
Wind forcing, numerical residual diagnostics, and repeatable experiment sweeps.
Qualification Wind uses a bounded full-domain projection with fixed inlet velocity.
`solver_iterations` accepts 8–512 in qualification mode (legacy stays 8–48).
Health distinguishes actual iteration usage, linear-system convergence, and final
conservation. Over-budget full grids fail validation. Physical drag
coefficients remain unavailable until force measurement and validation are complete.


The S3 local qualification CLI now saves wake time histories and explicit steady
window assessments. `scripts/assess_fluid.py` compares matched spatial, temporal,
or outlet-length studies; it is a local analysis tool, not a new remote MCP
endpoint. MCP `scene_create` accepts `object_center_m` so an outlet-extension
study can keep the obstacle fixed. See the acceptance-screen section of
[solver_qualification.md](solver_qualification.md) for thresholds and limitations.

Qualification health also reports `velocity_transport`, `transport_scope`, and
`transport_corrected_components`, `transport_limited_components`, and
`transport_fallback_components`. Counts cover the latest solved step and exclude
solid cells; limited is a subset of corrected. See [transport accuracy](transport_accuracy.md)
for the method, fallback behavior, tests, and physical-acceptance limitations.

## Reduced incompressible channel model

The `cfd_channel` template selects `incompressible_channel_fv_v1`, using the same
scene revision, run controls, inspection, sampling and result contracts. Its grid
is `[1,N,1]`, SI fluid properties are required, and Wind solver overrides are
rejected. `pressure_pa` is imposed affine physical pressure; `shear_stress_pa` is
computed viscous stress. Neither field is advertised as available for Wind.
See [cfd_channel.md](cfd_channel.md) for complete scope, signs, parameters,
analytical verification, and reproducible agent examples.

See [2D staggered CFD](cfd_mac2d.md) for channel authoring, physical pressure,
actual pressure-solve diagnostics, limits and verification evidence.

[Boundary traction and momentum qualification](cfd_boundary_forces.md) now
calibrates physical surface forces and exposes MAC `boundary_force_budget`
wall loads and explicit qualification status. Obstacle geometry, open outlets
and body drag remain unsupported/unqualified, separate from channel wall proof.

Open CFD presets `cfd_open_channel_2d` and `cfd_open_obstacle_2d` now use this same
session contract. See [CFD agent laboratory](cfd_agent_lab.md) for physical units,
initial-condition semantics, force histories, refinement and qualification.

### Refined 2D CFD development model

Main Edit supports `cfd_refined_channel_2d` and `cfd_refined_obstacle_2d` scene
templates. Channel options include `solve_mode` (`transient_navier_stokes` or
`steady_stokes`) and `refinement_regions`, each with physical `bounds_m`
`[xmin,ymin,xmax,ymax]` and dyadic `level` 0..10. Regions are fixed for the run;
this does not provide dynamic remeshing. The stationary rectangle must align
with removable refined leaves. Base grids are `[Nx,Ny,1]`, 4..256; the maximum
fluid leaf budget is 200000. `numerical_memory_limit_mib` defaults to 512 and
accepts 1..4096, accounting for numerical setup and solver allocations, not total
process or artifact memory. Rejected requests/runs do not silently lower quality.

Steady Stokes requires `steps=1` and reports physical time zero. Transient runs
retain fixed timestep and CFL admission. `run_sample` exposes physical pressure
as `pressure_pa`; unavailable fields are not replaced by proxies. Results contain
`physics_sim_refined2d_fields_v1` leaf and shared-face data. `run_compare` and
optional coarse-run arguments to `run_assess` expose matched spatial/temporal
integral sensitivity; stationary temporal comparisons are invalid. Physical
qualification is limited to the explicitly matching steady rectangle reference.
Use `make test-cfd-refined-agent-session` for the real-worker workflow proof.

New `cfd_refined_channel_2d` scenes default to an empty `refinement_regions`
list (uniform base cells). Smooth manufactured-flow cost comparisons favor this
choice. Local regions are opt-in; `cfd_refined_obstacle_2d` retains its body-region
refinement. Neither default is a physical accuracy certificate. Existing scene
revisions retain their authored mesh policy.

Refined `run_assess` includes `reference_accuracy` with an explicit status:
`passed`, `failed`, `not_established`, or `not_applicable`. Separate force and
energy checks accompany applicable cases. Numerical convergence is required;
a completed coarse run can correctly report failed physical accuracy. A case
outside the fixed reference scope reports not applicable rather than failed.

Refined health includes `fluid_cells_per_level` (indices 0..10),
`minimum_cell_spacing_m`, and `maximum_cell_min_spacing_m` (maximum of each
leaf's smaller XY spacing). Counts and spacings are cached at initialization.
`phase_cost` reports process CPU milliseconds for setup once and transport,
mixed solve and physical observation on the last accepted step. Mixed solve
includes hierarchy preparation. Unavailable phases are null. Snapshot
`runtime_cost` separately reports monotonic wall milliseconds for the previous
completed snapshot/preview publication and final full-field export, with zero
before measurement. CPU and wall costs must not be summed as if equivalent.

## Diagnostic preservation

Main Edit preserves retired live samples and request identities in verified
operational history before removing active copies. Pending requests are held
regardless of age. See [session_sample_retention.md](session_sample_retention.md)
for request limits, retries and remaining lifecycle requirements.

Session root, lock, asset and JSON admission is documented in
[session_path_admission.md](session_path_admission.md). These are trusted-local
source controls; public upload sandboxing remains unsupported.

Authoring and validation retain requests, stage files and bounded worker logs in
fresh attempt identities. See [session_worker_attempts.md](session_worker_attempts.md)
for failure, reuse and recovery boundaries.
