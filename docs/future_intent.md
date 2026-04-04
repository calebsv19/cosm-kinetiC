# Physics Sim Future Intent

Last updated: 2026-04-04

## Scaffold Alignment Intent
1. Preserve current subsystem decomposition strengths (`app`, `physics`, `render`, `tools`, etc.).
2. Introduce the locked scaffold lifecycle wrapper symbols with behavior parity.
3. Normalize verification into explicit deterministic migration gates.
4. Lock temp/runtime path hygiene so generated/runtime state does not drift into tracked defaults.

## Planned Next Structural Intent
- `PS-S1` (completed):
  - added public scaffold docs floor:
    - `docs/current_truth.md`
    - `docs/future_intent.md`
    - `docs/README.md`
  - updated root README pointer to docs index.

- `PS-S2` (completed):
  - add explicit scaffold verification aliases in `makefile`:
    - `run-headless-smoke`
    - `visual-harness`
  - split test lanes:
    - `test-stable` (deterministic migration gate)
    - `test-legacy` (known stale/failing lanes, including `test-shared-theme-font-adapter` until fixed)

- `PS-S3` (completed):
  - introduce canonical wrapper entry API:
    - `include/physics_sim/physics_sim_app_main.h`
    - `src/app/physics_sim_app_main.c`
  - lock lifecycle stage symbol names:
    - `physics_sim_app_bootstrap`
    - `physics_sim_app_config_load`
    - `physics_sim_app_state_seed`
    - `physics_sim_app_subsystems_init`
    - `physics_sim_runtime_start`
    - `physics_sim_app_run_loop`
    - `physics_sim_app_shutdown`
  - behavior-preserving delegation path retained:
    - `main()` now calls `physics_sim_app_main(...)`
    - prior startup/runtime body remains in `physics_sim_app_main_legacy(...)`

- `PS-S4` (completed):
  - locked temp/runtime ignore lanes:
    - `tmp/`
    - `data/runtime/`
    - `data/snapshots/`
  - locked defaults-vs-runtime persistence split:
    - runtime load prefers `data/runtime/app_state.json`, fallback `config/app.json`
    - runtime save writes to `data/runtime/app_state.json`
    - preset load prefers `data/runtime/custom_preset.txt`, fallback `config/custom_preset.txt`
    - preset save writes to `data/runtime/custom_preset.txt`
  - include strategy lock remains in effect for app-level public entry APIs under `include/physics_sim/`.

- `PS-S5` (completed):
  - closeout sync across private/public/global scaffold trackers completed.
  - final verification gate set completed:
    - `make -C physics_sim clean && make -C physics_sim`
    - `make -C physics_sim run-headless-smoke`
    - `make -C physics_sim visual-harness`
    - `make -C physics_sim test-stable`
  - commit title lock remains in effect for final closeout commit after user confirmation:
    - `Project Scaffold Standardization`

## Post-Scaffold Direction
- completed post-scaffold lanes:
  - font-size standardization (`PS-F0` through `PS-F5`) is complete
  - wrap-up commit title used:
    - `Post-Scaffold Font Size Standardization`
  - trio 2D/3D parity propagation with `line_drawing` and `ray_tracing`:
    - `../docs/private_program_docs/physics_sim/2026-03-30_physics_sim_2d_3d_parity_with_line_drawing_plan.md`
  - current status:
    - `PS-U0` complete (baseline freeze + gap map + tracker sync)
    - `PS-U1` complete (space mode runtime contract + persistence + menu selector)
    - `PS-U2` complete (mode adapter seam for world/view/solver routing)
    - `PS-U3` complete (additive scene/object dimensional contract + compatibility tests)
    - `PS-U4` complete (backend separation + mode routing + route contract tests)
    - `PS-U5` complete (mode visibility + controlled 3D lane UX hints)
    - `PS-U6` complete (verification + docs + memory closeout)
  - parity lane status:
    - complete (`PS-U0` through `PS-U6`)
  - latest execution log:
    - `../docs/private_program_docs/physics_sim/2026-03-31_ps_u6_verification_docs_memory_closeout.md`

- desktop packaging lane:
  - `PS-PK0` baseline mapping complete
  - `PS-PK1` implementation complete
  - `PS-PK2` closeout/docs sync in progress
  - current packaging targets in `makefile`:
    - `package-desktop`
    - `package-desktop-smoke`
    - `package-desktop-self-test`
    - `package-desktop-copy-desktop`
    - `package-desktop-sync`
    - `package-desktop-open`
    - `package-desktop-remove`
    - `package-desktop-refresh`
  - public packaging reference:
    - `docs/desktop_packaging.md`

- trio shared-scene bridge lane:
  - `TP-S4` complete in `physics_sim`:
    - runtime preflight/apply bridge for `scene_runtime_v1`
    - namespace-safe writeback guardrails (`extensions.physics_sim.*`)
    - stable contract tests integrated into `test-stable`
  - `TP-S5` complete:
    - fixture-driven round-trip interop checks landed across `ray_tracing` + `physics_sim`
    - nondestructive namespace-preservation behavior validated for both app overlays

- connection-pass lane:
  - completed:
    - `PS-CP0` baseline routing/ownership map captured
    - `PS-CP1` wrapper context + guarded stage-transition hardening
    - `PS-CP2` explicit wrapper dispatch seam extraction
    - execution log:
      - `../docs/private_program_docs/physics_sim/2026-04-01_physics_sim_connection_pass_cp0_cp2_execution.md`
  - next:
    - optional `PS-CP3+`: deeper extraction of runtime/update/render ownership from legacy concentration points

- cross-program wrapper initiative:
  - `W0` complete (canonical wrapper contract frozen in scaffold docs)
  - `W1` complete for `physics_sim` (wrapper alignment in `src/app/physics_sim_app_main.c`)
  - `W2` complete for `physics_sim` (structured wrapper diagnostics normalization with stage-order violations, wrapper error taxonomy, and final wrapper exit summary logging)
  - `W3` complete:
    - `S0` baseline freeze + verification rerun complete
    - `S1` typed runtime-loop adapter seam complete (`physics_sim_app_runtime_loop_adapter(...)`)
    - `S2` typed run-loop handoff seam cutover complete (`physics_sim_app_run_loop_handoff_ctx(...)`)
    - `S3` seam diagnostics and ownership hardening complete (ownership state + seam-local wrapper error diagnostics)
    - `S4` closeout/docs/memory sync complete
  - next:
    - optional `W4+` only if deeper legacy-lane extraction is needed
  - execution note:
    - `../docs/private_program_docs/physics_sim/2026-04-02_physics_sim_w1_w2_wrapper_hardening.md`
    - `../docs/private_program_docs/physics_sim/2026-04-02_physics_sim_w3_s0_s1_execution.md`
    - `../docs/private_program_docs/physics_sim/2026-04-02_physics_sim_w3_s2_execution.md`
    - `../docs/private_program_docs/physics_sim/2026-04-02_physics_sim_w3_s3_execution.md`
    - `../docs/private_program_docs/physics_sim/2026-04-02_physics_sim_w3_s4_closeout.md`

- RS1 render split lane:
  - `RS1-S0` complete (render ownership baseline map captured in scene controller)
  - `RS1-S1` complete (typed update/derive/submit frame contracts + explicit phase seams landed)
  - `RS1-S2` complete (diagnostics closeout + tracker synchronization)
  - execution note:
    - `../docs/private_program_docs/physics_sim/2026-04-03_physics_sim_rs1_s0_s1_execution.md`
    - `../docs/private_program_docs/physics_sim/2026-04-03_physics_sim_rs1_s2_closeout.md`
  - next:
    - optional deeper extraction only if RS1 contracts need promotion into a broader shared kit lane

- IR1 input-routing lane:
  - `IR1-S0` complete (top-level input ownership baseline map captured in scene controller)
  - `IR1-S1` complete (typed input frame contracts landed)
  - `IR1-S2` complete (explicit input phase seams landed with behavior parity)
  - `IR1-S3` complete (diagnostics + tracker synchronization closeout)
  - execution note:
    - `../docs/private_program_docs/physics_sim/2026-04-03_physics_sim_ir1_s0_s3_execution.md`
  - next:
    - optional deeper pane-target routing split only if upcoming editor work requires finer route-policy ownership

## Non-Goals During Scaffold Migration
- No feature-expansion work unrelated to scaffold alignment.
- No shared subtree redesign inside scaffold migration commits.
- No one-pass broad naming churn; changes stay phase-bounded and parity-verified.

## Release Readiness Next Intent
- active release lane:
  - `PS-RL0` through `PS-RL5` execution plan:
    - `../docs/private_program_docs/physics_sim/2026-04-04_physics_sim_release_readiness_rl0_rl5_execution_plan.md`
- completed now:
  - `PS-RL0` release contract freeze
  - `PS-RL1` bundle audit + Vulkan runtime hardening
  - `PS-RL2` signing/notary integration
  - `PS-RL3` artifact + desktop release flow
  - `PS-RL4` validation evidence + docs synchronization
  - `PS-RL5` one-shot `release-distribute` closeout (notarize/staple/verify)
- next:
  - no immediate release-lane work; proceed to next program release rollout
