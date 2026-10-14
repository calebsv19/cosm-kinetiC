# `src/tools/`

Command-line and diagnostic tools live here.

- `cli/physics_sim_headless.c` - direct retained-scene, Water, Wind, volume,
  and render-frame headless runner.
- `cli/physics_sim_job_runner.c` - detached submit/status/cancel runner and
  job-status surface.
- `cli/physics_trace_tool.c`, `cli/vf2d_pack_tool.c`, and
  `cli/vf2d_dataset_tool.c` - trace, pack, and dataset export tooling.
- `cli/runtime_scene_emitter_diag_tool.c` - runtime-scene, runtime mesh,
  emitter, footprint, and Wind projected-area diagnostic summaries.
- `cli/shape_*` plus `ShapeLib/` - shape import, flattening, validation, and
  ShapeAsset conversion tools.
- `physics_sim_flow_time_probe.c` - contained 64-cubed evolving plume
  qualification with flow steps of 5, 2.5 or 1.25 ms and fixed 1.25 ms heat
  and smoke steps. Build with `open-flow-time-probe` in a fresh `BUILD_DIR`;
  the compile profile is separate from ordinary worker admission. Input is
  the frozen 32-window native source fixture and output is a new binary field
  path. `scripts/fire_fluid_flow_time_report.py` reads the terminal three-case
  replay. This tool establishes temporal sensitivity, not spatial convergence
  or material/surface qualification.

- The same probe has a separate `open-flow-spatial-probe` profile for paired
  independent 64/128-cubed flow at 5 ms, with fixed 1.25 ms heat/smoke timing.
  `physics_sim_flow_source.c` embeds the original fine receiver's compact lower
  source support into the full 128-cubed domain. Fine input is **not** parent/8:
  that changes the source shape near the original Fire nodal boundaries.
  At 64 the existing full source input is copied exactly. Both native input
  layouts happen to contain 524,288 doubles per window: two full 64-cubed fields
  versus two 128x128x16 support fields. The qualified replay binds their distinct
  hashes and checks the reconstructed full fine source against the prior fixture.
  `scripts/fire_fluid_flow_spatial_report.py` uses the bundled NumPy interpreter
  for finite-volume scalar restriction, normal-face area velocity restriction,
  native wind probes and source-adjacent localization. The 10% physical heat
  region stays unchanged; fine-grid temporal and surface qualification are
  separate boundaries.

- `physics_sim_buoyancy_projection_probe.c` - one smooth manufactured thermal
  projection impulse with independent continuum and discrete eigenfunction
  references. Build `buoyancy-projection-probe`; grids 8/16/32/64/128 are
  qualification-only. `test-buoyancy-projection-sanitize` covers 8/16. Pressure
  and MAC face-area means are compared at the original mixed pressure
  boundaries. This checks forcing/projection consistency, not nonlinear plume,
  finite-time wall viscosity, surface heating or physical calibration.

- `physics_sim_matched_original_probe.c` - corrected matched-velocity heat
  attribution with original source mapping at both grids and fixed 1.25 ms
  scalar timing. `open-matched-original-probe` takes the full 64 source,
  original fine compact support and a fresh output directory. The 64 driver
  evolves accepted flow, a 64 heat control must match every step exactly, and
  the 128 heat carrier follows flux-preserving prolonged driver velocity.
  This does not independently solve fine momentum. It supersedes the earlier
  parent/8-source diagnostic for original-source attribution. Analyze its
  terminal replay with `scripts/fire_fluid_matched_original_report.py` and
  the separately completed evolving-flow pair. Do not infer additive shares
  from the difference norms.

- `physics_sim_prescribed_flow.c` and `physics_sim_prescribed_flow_probe.c` -
  online 64-cubed accepted driver and independently evolving 128-cubed flow
  under the driver's **old-time** thermal cell averages. Build
  `open-prescribed-flow-probe` in a fresh `BUILD_DIR`; arguments are the original
  full 64 source, original fine compact source and fresh output directory.
  The helper temporarily supplies extensive thermal averages only to momentum,
  restores accepted heat before scalar advancement, and preserves all accepted
  commit/refusal paths. Native/sanitizer controls cover exact ordinary evolution,
  conservative temperature mapping, no scalar injection and refusal restoration.
  The diagnostic shares 5 ms flow and 1.25 ms scalar timing, emits final fields
  and the last old-time forcing samples, and stores no large histories. Analyze
  the completed replay with `scripts/fire_fluid_prescribed_flow_report.py`.
  Fine heat/smoke retain the original source mapping and their own transport;
  coarse forcing is piecewise constant in temperature. This is externally forced
  attribution, not a newly qualified fully coupled solver or worker capability.

- `physics_sim_thermal_transfer.c` and `physics_sim_reconstructed_flow_probe.c` —
  qualification-only conservative bounded MC reconstruction of the coarse
  old-time thermal averages. Build `open-reconstructed-flow-probe`; use the
  same three arguments as the piecewise-constant probe. Eight child offsets
  preserve each parent mean; one multidimensional limiter and final floating-
  point bounds enforcement prevent overshoot. Domain-edge slopes are zero.
  This changes the prescribed momentum input, not either grid's heat source.
  `test-thermal-transfer-native` and `test-thermal-transfer-sanitize` check
  constants, affine interiors, sharp/graded support, smooth refinement and
  admission/refusal closure. `scripts/fire_fluid_reconstructed_flow_report.py`
  compares terminal transfer and separate fine timing with frozen controls.

- `open-fine-flow-time-probe` compiles the existing flow probe in a separate
  128-cubed timing profile. Arguments: `128 <0.005|0.0025|0.00125> 0
  <original-fine-support.bin> <new-fields.bin>`. Its own accepted heat drives
  momentum; reconstructed forcing is absent. Original source windows and
  1.25 ms scalar timing remain fixed. Ordinary worker admission is unchanged.

- `open-vertical-flow-probe` uses the same accepted flow probe with a contained
  `PHYSICS_SIM_OPEN_VERTICAL_REFERENCE` capacity profile. The reference is
  128x128x256 at 5 ms flow and 1.25 ms scalar timing, with independently evolving
  momentum and heat. `physics_sim_vertical_source.c` splits each original
  128-cubed extensive source sample equally into two vertical children; XY
  samples and lower-1/8 physical support stay unchanged. Arguments:
  `128x128x256 .005 0 <original-fine-support.bin> <new-fields.bin>`.
  An optional final `1` requests a one-window pilot and requires a one-window
  source file. Full qualification uses all 32 original windows. The contained
  profile admits at most 4,194,304 cells, a 4 GiB tracked allocation budget and
  12 billion scalar work cells; ordinary worker and prior full-grid admission
  remain unchanged. `test-vertical-source-native` / `test-vertical-source-sanitize`
  verify conservative splitting, old isotropic mapping parity, refusal and
  anisotropic flow accounting. `scripts/fire_fluid_vertical_reference_report.py`
  compares terminal fields against the frozen 128-cubed baseline. This is
  directional refinement only, not full three-axis convergence or a solver repair.

- `open-momentum-diagnostic-probe` enables observers only in the contained
  vertical-reference profile. It keeps the original solver math and reports
  predictor finiteness, momentum CFL, pressure-cache/allocation/solve, CG
  breakdown and projected divergence separately. It saves the last accepted
  state, rejected candidate scratch, rejected-step source and JSON observations
  on refusal, using the ordinary probe output argument as the capture prefix.
  `physics_sim_momentum_capture.c` serializes a trusted-local same-ABI state:
  twelve native uint64 values, twenty-six native doubles, then velocity,
  pressure, heat, smoke and four boundary-flux receipt blocks. Pointers and
  cached matrices are excluded. Parameters/time/counters are preserved; matrices
  are rebuilt. `open-momentum-replay` loads this state and retries one step:
  `<accepted.bin> <source.bin> <new-prefix>`. This is a qualification helper,
  not an ordinary worker checkpoint protocol or cross-platform format.
  Native/sanitizer tests prove exact small-case save/load/continued-step parity
  and refused-state preservation, plus distinct CFL/cache/CG/divergence checks.
  Candidate pressure/RHS are phase-invalid when pressure was not reached;
  solver residuals are the last reported true residual and may precede an early
  terminal CG breakdown. `scripts/fire_fluid_momentum_readback.py` independently
  checks accepted accounting/divergence, the recorded worst-face CFL, valid
  candidate pressure/divergence, and the original rejected-step source mapping.
  The 10% spatial heat gate stays unchanged; a captured refusal is not a spatial pass.

- `open-matched-vertical-probe` is a separate 128x128x256 qualification profile
  at 2.5 ms flow and fixed 1.25 ms scalar timing. Original source windows divide
  into forty flow inputs; ordinary admission and the earlier 5 ms vertical
  profile remain unchanged. Use `128x128x256 .0025 0 <fine-support.bin>
  <new-fields.bin>`. `open-momentum-replay` also accepts an optional final
  `half-flow` for two 2.5 ms steps from a saved 5 ms state. It halves source,
  rebases the logical step index while preserving physical time/fields/receipts,
  and records retiming.json. This local mixed-history test is distinct from a
  full new reference started from zero. `physics_sim_half_flow.c` and its
  native/sanitizer tests cover prevalidation, source closure, clock/cycle
  partition and conservation. `scripts/fire_fluid_matched_vertical_report.py`
  binds the old completed 64/128 2.5 ms baselines and the new full vertical case,
  with finite-volume scalar and face-area velocity restriction and native probes.
  The 10% heat region/gate remains unchanged; no surface or burning-object claim.

- `open-matched-horizontal-probe` uses a separate capacity profile for the
  first horizontal X refinement: 256x128x256 at 2.5 ms flow and 1.25 ms scalar
  timing. `physics_sim_horizontal_source.c` splits original compact source
  into X/Z children; X sums recover the accepted vertical source exactly.
  Arguments: `256x128x256 .0025 0 <fine-support.bin> <new-fields.bin>`.
  Tracked memory is capped at 8 GiB, scalar work at 22 billion and cells at
  8,388,608. Native/sanitizer tests cover conservative source restriction,
  refusal preservation, timing and accounting. The horizontal report restricts
  extensive scalars by sum and tangential face areas by average at fixed Z.
  Y refinement, surface heating and burning faces remain separate gates.

Tools may call app/import/export contracts, but they should keep product
runtime policy in the owning source lane. Generated outputs belong under
ignored `tmp/`, `build/`, `export/`, or caller-selected output roots.
