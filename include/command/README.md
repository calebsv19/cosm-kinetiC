# `include/command/`

Headers for the command system. `command_bus.h` exposes the `Command` payload (with metadata/payload slots), the FIFO queue, and the dispatch helper used by the input layer and scene controller to pass discrete actions around without tight coupling or frame hitches.
