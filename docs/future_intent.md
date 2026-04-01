# Physics Sim Future Intent

Last updated: 2026-04-01

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

## Non-Goals During Scaffold Migration
- No feature-expansion work unrelated to scaffold alignment.
- No shared subtree redesign inside scaffold migration commits.
- No one-pass broad naming churn; changes stay phase-bounded and parity-verified.
