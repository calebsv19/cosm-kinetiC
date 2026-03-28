# Physics Sim Future Intent

Last updated: 2026-03-28

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

## Non-Goals During Scaffold Migration
- No feature-expansion work unrelated to scaffold alignment.
- No shared subtree redesign inside scaffold migration commits.
- No one-pass broad naming churn; changes stay phase-bounded and parity-verified.
