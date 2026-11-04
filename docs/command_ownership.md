# Commands and state ownership

The legacy scene editor owns a working preset. Name-field confirmation accepts a
name into that draft; it does not rename the caller's preset. Apply publishes the
working preset and name into caller-owned storage when the editor returns. Cancel
preserves the caller's preset and name. Without caller-owned name storage, Apply
preserves the original name pointer instead of publishing temporary editor memory.

| Command family | Owner and completion boundary |
| --- | --- |
| Legacy editor fields, objects, emitters and imported shapes | Working preset. Native keys and pointer controls reach editor helpers. Apply publishes to the caller; Cancel closes without publishing. Escape cancels an active field or clears selection/gesture state; the Cancel/Menu button closes the draft. |
| Picker add and drop | Resolve/validate a native asset before admitting the working import and selection. A centered drop and keyboard add create equivalent imports. Dropping an existing path selects its existing instance without relocating or duplicating it. The asset library is a shared derived cache; generated conversion files and cache refresh are not authored undo. |
| Retained scene and physics overlays | Editor session owns validated overlay edits; Apply Overlay and Save have separate feedback. Retained source geometry is not rewritten by legacy preset commands. Save publication/refusal and recovery follow the persistence contract. |
| Native runtime shortcuts | Scene-local command bus copies value payloads, assigns ordered IDs and drains on the scene owner before its simulation step. Growth failure refuses the new command without evicting earlier commands. OS key-repeat does not duplicate toggles/export commands; separate key presses remain separate actions. |
| Runtime painting, physical controls and stepping | Scene/runtime backend owns accepted simulation state. Stroke samples and timestep transitions are distinct from preset editing. Command bus shutdown retires pending commands with that scene; IDs are local order, not durable retry receipts. |
| View, selection, text composition and overlays | UI owner controls focus and transient interactions. Shared press/release controls invalidate pending activation when their scope changes; text entry retains its own repeat/composition behavior. Presentation does not acknowledge a physical step. |
| Headless CLI and trusted-local session service | Independent process/session, not a connection to an open desktop scene. Session command identity and scene revision bind ordered execution; inspect the same receipt after an uncertain reply. Job completion/cancellation and artifacts are separate from admission. |

Legacy preset editing offers Apply/Cancel, not a general undo/redo stack.
Simulation traces/checkpoints are not edit history. Native in-process commands
need no durable retry ID because their payload remains owned within a single
scene lifetime; remote/local-session protocols have separate durable identities.

Verification includes `test-scene-editor-command-ownership`,
`test-scene-editor-shared-ui-contract`, `test-physics-sim-ui-host-contract`,
`test-command-delivery`, `test-agent-session`, the existing retained/editor and
runtime contracts in `test-stable`, and the headless CLI gate. Native SDL tests
exercise real input adapters with a dummy video backend; they are not a visible
Vulkan desktop acceptance session.
