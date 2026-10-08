# Core Sim reference host

See the module README's **Standalone reference host (T5)** section for commands,
expected values and boundaries. These files are host examples, not library APIs.

- `reference_model.c`: deterministic native toy participants, exact source
  allocation, real fine-step observation samples and T1–T4 orchestration.
- `reference_persistence.c`: example-owned canonical text checkpoint and atomic
  single-writer POSIX publication; restores native state and reconstructs the
  deterministic versioned metadata recipe without executing accepted steps.
- `reference_main.c`: bounded run/resume/failure CLI and JSON model/wall metrics.
- `reference_host.h`: private example declarations, not an installable header.

The entire profile is fixed except mode 0/1/2, observation FPS 5/25/60, stop
window and controlled fault injection. Fault phases: 1 after source computation,
2 after receiver computation, 3 after durable publication and before memory
adoption. Faulted runs exit 75; rerun with `--resume` and omit fault arguments.
A phase-3 checkpoint includes the completed window, so reread it rather than
assuming submission failed. Phases 1/2 retain the prior accepted checkpoint.

The v1 format has three canonical decimal lines (profile/window/trajectory/plan,
current native counters, previous native counters) plus an FNV checksum line.
All active joint metadata derives deterministically from this fixed profile,
window and native states; changing the recipe requires a new format/profile.
Native checkpoint/payload evidence references derive from counters and producer
identity. The loader verifies envelope, plan/profile, counts and last-window
conservation, then runs T4 restart validation. FNV and opaque references are not
hostile-content authentication; a production host needs its own verified content
store and durable ownership controls. No generic persistence ABI is proposed.

The source holds the accepted return signal for one exchange window. The
receiver saturates its return at one after any input. This bounded toy law makes
zero/disabled parity and a known nonzero response independently calculable;
it is not a proposed wind response or physically calibrated coupling model.
