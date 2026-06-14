# kinetiC fisiCs Memory-Check Audit

This repo has a default-off fisiCs memory-check audit target for the soft-body
contract harness and a broad app compile under the combined
`physics-units,memory-check` overlay.

Run:

```sh
make -C physics_sim memory-check-audit
```

The target:

- rebuilds the fisiCs app path with memory instrumentation linked in
- builds `tests/soft_body_contract_test.c` and `src/physics/soft/soft_body.c`
  under the same combined overlay
- runs the soft-body harness with `FISICS_MEMCHECK_REPORT=always`
- writes reports to:
  - `physics_sim/build/memory_check/physics_sim.stdout`
  - `physics_sim/build/memory_check/physics_sim.stderr`

Current clean audit evidence from 2026-06-07:

```text
[fisics:memory-check] summary: active=0 leaked_bytes=0 allocs=61 frees=61 double_free=0 unknown_free=0 tracker_failures=0
```
