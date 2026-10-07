# PhysicsSim shared UI rollout

Status: preparation checkpoint; UI integration blocked by retained-evidence loss.
Date: 2026-10-06 Pacific.

## Source and preservation

Functional lane is the existing `codex/physics-sim-main-edit` checkout. Canonical
is unchanged at `3fa1ad5`. Completed icon repairs are checkpointed at `f553e94`;
existing grounded/plume development is checkpointed at `d226201`. Focused package,
native atmosphere and 35 Python checks passed before shared import. Canonical
release drift was reconciled at `f75b87e`, inheriting VERSION 0.4.0 and worker
0.3.4; no new version decision was made.

Managed import `ea42aab` is exactly shared
`09ff89a0a32d80981010b15a7d45a2cdf8e5fd9f`. Versions are kit_ui 0.18.0,
kit_pane 0.5.0, vk_renderer 1.7.0 and vk_runtime 0.6.0. The full delta covers 160
source files. Non-UI changes include additive authored surface mapping,
Memory DB read-only/CLI APIs and scene compiler provenance/payload APIs. Scene
compiler source/link declarations require review during consumer qualification.
Live dirty shared mesh/authored-texture changes were excluded.

## Evidence interruption

The agent incorrectly ran the existing runbook's broad `make clean` before
inventorying ignored evidence. Its recursive deletion was stopped; compilation
never began. Documented `build/c3d-box`, `build/c3d-native-cube-pressure`,
`build/c3d-wall-shear`, `build/cfd-reference-venv` and `build/open-atmosphere`
are now absent. The complete lost-file set is unknown. Historical source docs
remain, but those missing receipts/fields cannot currently be independently
read back. Recomputed results would be new evidence, not recovery.

Related temporary box assessments and independent GrowthSim plume evidence
exist; full recovery is not established. No UI implementation, native capture,
consumer clean-build acceptance, package rebuild or Desktop refresh occurred.
Canonical/stable product source and bundles were not changed by this task.
Maintainer incident and survivor manifest are held in the task evidence directory
named by the private PhysicsSim work-status entry. Recover retained CFD evidence
before continuing UI integration. The Main Edit runbook now prohibits broad clean
and requires fresh task-owned build output.

## Region acceptance ledger

Imported means shared source is present. Wired means this actual owner uses the
new interaction/presentation contracts. Tested and visually accepted require
fresh owner-level and native evidence; legacy tests do not qualify the new pin.

| Region | Existing source owner | Imported | Wired to new contracts | Actual-owner tested | Visually accepted |
| --- | --- | --- | --- | --- | --- |
| Startup controls | `src/app/menu/menu_input.c`, `menu_render.c`, common `src/app/ui/physics_sim_ui_button.c` | Yes | Pending; current activation tests release position without shared press ownership; common frame is rectangular | Pending | Pending |
| Settings | `menu_settings_input.c`, `menu_settings_render.c`, `menu_settings_layout.c` | Yes | Pending; retain draft/apply/save/reset policy and provider geometry | Pending | Pending |
| Scene editor | `src/app/editor/scene_editor_input*.c`, pane/viewport owners | Yes | Pending; retain selection, undo and scene publication | Pending | Pending |
| Runtime HUD | runtime controller/overlay owners under `src/app/` and `src/app/structural/` | Yes | Pending; inventory all populated modes before cutover | Pending | Pending |
| Structural/preset views | `src/app/structural/structural_controller*.c`, `structural_preset_editor*` | Yes | Pending; separate controller loop | Pending | Pending |
| Pickers | `src/app/platform/physics_sim_file_picker.c`, retained scene/preset catalogs | Yes | Native panels remain platform-owned; catalog surface adoption pending | Pending | Pending |
| Text/modal owners | `src/ui/text_input.c`, `menu_state_text_edits.c`, editor text keys, `session_workspace_text.c` | Yes | Pending; bounded field storage, eligibility and submission lifetime need audit | Pending | Pending |
| Scrolling | `src/ui/scrollbar.c`, editor scroll owners, menu list | Yes | App gesture policy retained; window/takeover cancellation pending | Pending | Pending |
| Viewport gestures | editor viewport and runtime camera/input owners | Yes | Legacy shared viewport semantics retained; cancellation/lifecycle adoption pending | Pending | Pending |

## Next bounded implementation contract

Outcome: dependency/window parity followed by startup/settings shared matched
press-release controls and rounded measured presentation, preserving product
geometry, configuration, domain commands, undo and persistence. Use a small
menu adapter subfile and the existing common painter/input seams; no new UI
framework. Register stable semantic controls, cancel on window/modal/text/
authoring takeover, and retain app-owned continuous gestures. Thin visible
slider/divider geometry must stay independent of input bounds. Queued text
must have frame-lived backing storage.

After evidence recovery, use a fresh task-owned output directory:

```sh
make BUILD_DIR=build/<fresh-task-proof> clang-build
make BUILD_DIR=build/<fresh-task-proof> test-physics-sim-ui-button-contract test-menu-settings-shell-contract test-scene-menu-layout-contract test-shared-theme-font-adapter
make BUILD_DIR=build/<fresh-task-proof> test-fast test-stable
make BUILD_DIR=build/<fresh-task-proof> run-headless-smoke
make BUILD_DIR=build/<fresh-task-proof> main-edit-package-contract-checks package-desktop-main-edit-self-test
```

Existing tests must be supplemented with production-linked menu ownership
replay and actual-loop dark/light captures, resize/fullscreen/hide/minimize/
restore recovery and interrupted continuous gestures. Isolate runtime data,
verify source/binary/signature/icon and Main Edit namespaces, preserve stable
kinetiC.app, and refuse refresh of a running Main Edit bundle.

Stop after startup/settings actual-owner and visual checks pass, or at a real
ownership/enforcement/recovery blocker. Editor/HUD/panes/text adoption remains
later ledger work. No canonical promotion, release, push or remote worker work.
