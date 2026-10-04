# Finite CFD delivery checkpoint

Started 2026-10-04 15:27 America/Los_Angeles; reviewable checkpoint target 18:30.
Delivered earlier after reaching the stable boundary. Physical validity takes
priority. Authoritative writer: physics_sim_main_edit, codex/physics-sim-main-edit.
No commit, package, install, release or remote work. The broad CFD aspiration is
future work; no automatic continuation should bypass the requested human checkpoint.

1. **Stationary-box workflow — complete within bounded steady scope.** Explicit
   physical bounds flow through actual scene authoring/compile/revision and
   source CLI/MCP/native sessions. One reproducible [48,24,24]→[96,48,48] scene
   passes complete U/P export/native SI equation readback, physical budgets,
   inspection and spatial comparison. Separate force components are reported
   with unestablished absolute accuracy. Backend/session ordinary/sanitized,
   box MCP and existing cube/Cartesian regressions pass. Frozen accepted fields,
   source/worker identity and initial failed attempts are preserved.
2. **Transient obstacle — partial, specification delivered; implementation
   incomplete.** Inspected masked operator lacks mass/history startup and valid
   transient budgets. Exact pressure-driven cube-from-rest equations/boundaries,
   APIs/files, independent semi-discrete time oracle, acceptance/failure controls
   and one estimated 4–6 hour next backend slice are in
   [startup specification](cfd_3d_obstacle_startup_spec.md). No transient-obstacle
   runtime, nonlinear transport or validated wake capability is claimed.
3. **Reference closeout — complete for this batch.** Retained paired cube raw
   mismatches 1.1951/1.1996%, side-normal trial 1.1343% but failed paired-run
   admission. Original raw 1% gate remains open. This blocks certified cube-force
   claims while provisional stationary capability proceeds. No further cube
   mesh/preconditioner experiments are run in this finite window.

Deliverable-1 evidence: nineteen completed developer fields, exact pressure-area
readback, symmetry/material controls, short-box finest 1% grid screen, retained
long/narrow-gap grid failures and passing matched-spacing L4/L6 force sensitivity.
Two original 600 s field stops remain failures, with separately declared 1800 s
diagnostic repeats under unchanged equations/residual/memory/cell caps.

Primary handoff: [usable source checkpoint](cfd_3d_box_checkpoint.md).
Accepted source scene: build/c3d-box/scenes/checkpoint-workflow/receipt.json.
Retained inspection: build/c3d-box/inspection/checkpoint/index.html.
Preservation/validation seal: build/c3d-box/checkpoint-audit.json.

Next decision: a dedicated masked mass/history pressure-startup implementation
using the one stated case and oracle; broader CFD work and further reference
research are outside this completed finite batch.
