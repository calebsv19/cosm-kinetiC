# Local agent simulation sessions (S1 and S2)

The source checkout and Main Edit package provide a trusted-local session service.
An agent and the native workspace can share one background solver without owning
its lifetime. This is not a remote submission API. The current session model is
approximate Wind; Water and atmosphere session authoring are future work.

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

Nine discoverable tools provide session control and live inspection:

1. `capabilities`: discover models, templates, limits, and semantics.
2. `scene_create`: create an immutable `wind_box` or `wind_sphere` template with
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
