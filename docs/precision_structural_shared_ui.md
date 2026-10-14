## 2026-10-13 — Precision and structural shared UI qualified

Main Edit completes the Precision Editor and structural preset/runtime window
owners against the existing immutable shared pin. Shared window observation,
foreign/stale event filtering, transient cancellation and fullscreen handling
preserve local drafts, selection, canvas and application apply/save/solve policy.
Hidden/minimized windows suspend GPU presentation. Structural dynamics continue
in their existing numerical owner while presentation is suspended.

The six painted structural editor actions use clipped shared control snapshots,
exact action identity, matched pointer/key release and native focus/pressed feedback.
Rectangular controls and scientific colors remain intentional. Read-only hints,
HUD, button labels and canvas tooltips use the shared measured/contained text seam;
Precision commit/cancel hints remain visible at compact sizes. Borrowed editor
fonts retain their existing shared roles; runtime font retirement waits for GPU
completion. The numerical structural-state header retains only a forward-declared
UI pointer and stays independently testable.

Final production Clang/headless builds, seven focused contracts, all 16 fast and
62 stable targets pass. Actual public host loops pass 24 native lifecycle stages
plus apply/cancel, action/save/cancel/solve/reopen and dynamic hide/resume/step
replays. Validation is enabled with zero diagnostics through complete process
cleanup. Twenty-four pixel-equal PNG captures are retained and representative
compact/normal captures were inspected.

Remaining rollout owners are the simulation window/HUD and F8 live inspection.
The former uses `scene_controller.c` with the Vulkan `renderer_sdl.c` compatibility
owner; simulation timing/export must remain independent of presentation suspension.
F8 is a real SDL renderer in `session_workspace.c`: it needs its own painter and
capture adapter, preserving live-session/control/attach and viewport ownership.
The new Vulkan painting helper cannot be cast onto that renderer. Combined mode,
platform and packaged-app/human acceptance remain final qualification boundaries.

Source is uncommitted Main Edit3028140 / VERSION0.5.0. Coupling source, frozen
probe, active workbench and imported shared bytes are preserved. Canonical and
installed/package/release surfaces are unchanged.
