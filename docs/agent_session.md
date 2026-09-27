# Local agent simulation sessions (S1)

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

The ordinary desktop menu also has **Agent workspace [F8]**. Use New Wind to
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

Eight discoverable tools provide the complete first workflow:

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
