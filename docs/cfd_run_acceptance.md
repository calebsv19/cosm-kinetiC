# Per-run CFD numerical acceptance

`run_assess` is a read-only MCP/CLI operation for `incompressible_open2d_v1`.
It reports `passed`, `failed`, or `not_established` for each check. Missing
history, unstarted projection, missing energy/force values and missing
refinement runs never become a pass. Failed/cancelled runs cannot pass.

```json
{"run_id":"fine","spatial_coarse_run":"coarse-grid","temporal_coarse_run":"coarse-dt"}
```

Optional comparison runs must be completed, have the same immutable scene,
fluid, model, worker hash and physical endpoint as the target. Spatial comparison
requires nested grids and identical timestep; temporal comparison requires
identical grids and a larger coarse timestep. Rejected comparisons remain errors.

## Independent checks

- Mass mismatch divided by inlet mass flow: at most 1e-6.
- Maximum divergence scaled by domain height / inlet speed: at most 1e-6.
- Pressure projection must have completed successfully.
- Steadiness: every velocity face is examined on every simulation step. Each
  one-second window retains per-face minima/maxima, so oscillations returning to
  the same endpoint are still detected. Two consecutive full windows starting
  after five seconds must have relative span <= 1e-6 of inlet mean. A change in
  the current partial window immediately revokes a pass. Paused wall-clock time
  does not advance these windows. This fixed policy is scoped to the current
  bounded lab; it is not a universal physical settling timescale.
- Energy residual / net incoming power: at most 2%, using independent physical
  fluid-cell quadrature and a one-step kinetic-energy rate.
- An authored body requires surface/control-volume force difference <= 2%.
  Missing body measurements remain not established.
- Spatial and temporal comparisons require <= 2% endpoint changes in energy,
  flux and, for bodies, both total surface and surrounding-control-volume force.

`physical_accuracy_certified` is always false. Numerical readiness does not
establish reference accuracy, separately accurate pressure/friction forces,
arbitrary geometry or 3D CFD. No reference error or convergence order is inferred
from two nearby resolutions. A coarse and fine run can share the same bias.

## Evidence

`make test-cfd-run-acceptance` checks window aliasing, startup/recovery,
nonfinite rejection, independent report gates and real worker integration.
The existing session protocol now advertises eleven tools.
`build/s3-run-acceptance/` records completed eight-second channel runs and a
passing assessment with both refinement comparisons. Short obstacle integration
tests demonstrate that completion and force availability alone do not establish
steady state or qualification.

Shared reuse decision: existing scene/session/history/artifact contracts are
reused. Acceptance thresholds and CFD monitors stay app-owned (`reuse-deferred`
for generic shared numerical policy). No core/kit APIs or versions change.
