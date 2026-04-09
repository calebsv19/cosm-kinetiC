# PhysicsSim Keybinds And Input Audit

This document is the top-level keybind reference for PhysicsSim and an audit of where input is handled.

## Input Pipeline

- SDL events are polled in `physics_sim/src/input/input.c`.
- Discrete runtime actions are pushed to `CommandBus` (`physics_sim/include/command/command_bus.h`).
- Pointer/keyboard/text events are also forwarded to the active input context (`InputContextManager`) in:
  - `physics_sim/src/input/input_context.c`
  - `physics_sim/include/input/input_context.h`
- Contexts are used by:
  - scene menu (`physics_sim/src/app/scene_menu.c` + `physics_sim/src/app/menu/menu_input.c`)
  - scene editor (`physics_sim/src/app/editor/scene_editor.c` + `physics_sim/src/app/editor/scene_editor_input.c`)
  - structural runtime/editor (`physics_sim/src/app/structural/*.c`)

## Runtime Simulation Keybinds

Source: `physics_sim/src/input/input.c`, dispatch in `physics_sim/src/app/scene_controller.c`.

- `Esc`: quit simulation loop.
- `P`: pause/resume.
- `C`: clear smoke.
- `E`: export snapshot (`.ps2d`).
- `1`: density brush mode.
- `2`: velocity brush mode.

Overlay enable/disable:
- `V`: toggle vorticity overlay.
- `B`: toggle pressure overlay.
- `S`: toggle velocity vectors.
- `Shift + S`: toggle velocity vector mode (magnitude/fixed-length).
- `L`: toggle particle flow overlay.

`kit_viz` path toggles:
- `K`: toggle density render path (`kit_viz` vs legacy).
- `J`: toggle velocity render path (`kit_viz` vs legacy).
- `Shift + V`: toggle vorticity render path (`kit_viz` vs legacy).
- `Shift + B`: toggle pressure render path (`kit_viz` vs legacy).
- `Shift + L`: toggle particle render path (`kit_viz` vs legacy).

Other runtime toggles:
- `G`: toggle object gravity behavior.
- `H`: toggle elastic collisions behavior.

Global text zoom shortcuts (handled in `input_poll_events()` and consumed before context callbacks):
- `Cmd/Ctrl +` (`=`, `+`, keypad `+`): text zoom in.
- `Cmd/Ctrl -` (`-`, `_`, keypad `-`): text zoom out.
- `Cmd/Ctrl 0` (main `0` or keypad `0`): text zoom reset.

Runtime note:
- zoom requests are persisted to `data/runtime/app_state.json` by active UI loops.

## Scene Menu Input

Sources:
- `physics_sim/src/app/scene_menu.c`
- `physics_sim/src/app/menu/menu_input.c`

Pointer handling:
- Left-click selects slots/buttons/panels.
- Scroll wheel drives preset list scrollbar.
- Dragging works with menu scrollbar.

Keyboard handling:
- During headless run: `Esc` cancels the headless batch.
- Active text fields (rename, headless frame count, inflow, viscosity, input root, output root):
  - `Enter`: confirm edit
  - `Esc`: cancel edit
  - other key/text input routed to text-input widget
- `Cmd/Ctrl + I`: open native input-root folder chooser.
- `Cmd/Ctrl + Shift + I`: open typed input-root edit mode.
- `Cmd/Ctrl + O`: open native output-root folder chooser.
- `Cmd/Ctrl + Shift + O`: open typed output-root edit mode.
- `Cmd/Ctrl +/-/0`: text zoom shortcut (ignored while menu text fields are active).

Notes:
- Menu actions are mostly pointer-driven, with keyboard focused on text editing and cancel/confirm flow.

## Scene Editor Input

Sources:
- `physics_sim/src/app/editor/scene_editor.c`
- `physics_sim/src/app/editor/scene_editor_input.c`
- `physics_sim/src/app/editor/scene_editor_model.c`
- `physics_sim/src/app/editor/scene_editor_precision.c`

Core keys:
- `Enter`: apply and exit editor.
- `Esc`: cancel/close context or clear current selection depending on current state.
- `Tab` / `Shift + Tab`: cycle selected emitter.
- `Delete`/`Backspace`: remove selected import/object/emitter (context-dependent).
- `+` / `-` (and keypad variants): grow/shrink selected object/emitter.
- Arrow keys: nudge selected item.
- `G`: toggle gravity on selected object/import (with emitter-bound safety guards).
- `Cmd/Ctrl +/-/0`: text zoom shortcut (ignored while editor text fields are active).

Contextual keys:
- boundary-selected edge:
  - `E`: cycle boundary emitter
  - `R`: set boundary receiver
  - `X`: clear boundary assignment
- selection/mode-related editing paths use additional handlers in `scene_editor_precision.c`.

## Structural Runtime And Editors

Sources:
- `physics_sim/src/app/structural/structural_controller.c`
- `physics_sim/src/app/structural/structural_editor.c`
- `physics_sim/src/app/structural/structural_preset_editor.c`

Structural runtime (`structural_controller.c`) highlights:
- `Space`/`Enter`: solve.
- `E`: toggle dynamic mode.
- `P`: play/pause dynamics.
- `S`: single-step dynamics (when paused in dynamic mode).
- `Z`: integrator toggle.
- `R`: reset runtime simulation view.
- `T`/`B`/`V`: choose stress/bending/shear overlay.
- `6`/`7`: time scale down/up.
- `A`/`F`: damping alpha down/up.
- `H`/`J`: damping beta down/up.
- `U`: gravity ramp toggle.
- `0`: cycle gravity ramp duration.
- `I`/`C`/`L`/`O`: IDs/constraints/loads/deformed-view toggles.
- `-`/`=`: deformation scale down/up.
- `Q`: combined stress toggle.
- `Y`: percentile scaling toggle.
- `K`: scale freeze toggle.
- `G`: cycle gamma.
- `X`: thickness scaling toggle.
- `Cmd/Ctrl +/-/0`: text zoom shortcut (live HUD/runtime font reload + runtime persistence).

Structural editor (`structural_editor.c`) highlights:
- `1..5`: tool selection.
- `8`/`9`/`0`: support presets (pinned/fixed/roller).
- `X`/`Y`/`Q`: axis/rotation constraint toggles on selected nodes.
- `W`: weld selected nodes.
- `K`: split selected edge.
- `D`: duplicate selection.
- `H` (+ modifiers): toggle edge releases.
- `Delete`/`Backspace`: delete selection.
- `M` (`Shift+M` variant): material cycling/active material selection.
- `[` / `]`: load-case cycle.
- `-`/`=`: deform scale.
- `I`/`C`/`L`/`O`/`T`/`B`/`V`: visualization toggles.

Structural preset editor (`structural_preset_editor.c`) adds:
- `Ctrl + S`: save preset.
- `Ctrl + O`: load preset.
- `N`: add load case.
- `F`: attach selected to ground.
- plus pass-through to `structural_editor_handle_key_down`.

## Notes For Future Input Work

- Global key processing currently happens before context callbacks in `input_poll_events()`.
- Because of that ordering, single-letter globals can collide with context-specific bindings.
- If collisions become problematic, move to explicit mode-gating in `input.c` or route more actions through context-local handlers first.
