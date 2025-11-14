# `src/command/`

Implementation files dedicated to command routing. Right now it only contains `command_bus.c`, the FIFO queue plus dispatch helper that decouples input-driven actions from the simulation loop and enforces the per-frame batch size defined in the config. Future command processors or batching logic should live here so they stay separate from app orchestration.
