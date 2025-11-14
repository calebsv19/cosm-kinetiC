# `include/input/`

Defines the shared input data structure passed between SDL event handling and the scene logic.

- `input.h` – contains the `InputCommands` struct (quit flag, mouse state, and current brush mode) plus the `input_poll_events()` prototype implemented under `src/input/`. Discrete actions (pause, clear, export) are emitted through the `CommandBus` referenced by the polling function, and the `InputContextManager` API lets individual modules (menu, editor, runtime) push their own pointer/key handlers without touching SDL glue code.

Extend this header whenever you add new analog state, handler hooks, or brush metadata that needs to be consumed each frame.
