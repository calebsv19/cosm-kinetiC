# Open-domain wind driving and live Fire–Fluid comparisons

This is an opt-in source-development lane. The native driver, state bridge,
strict/sanitizer references, packet restart and predecessor-default parity are
qualified. The bounded eight-case live campaign is accepted: 264 checkpoints,
48 native snapshots and 256 live feedback windows. Final qualification passes
94 aggregate checks (89 fresh system, four fresh geometry, one retained Fire
control with identities rechecked). No default UI, installed app or released
worker changes.

The driver applies X acceleration throughout the represented fluid domain.
Ambient-pressure X/Y/top reservoirs remain open, the ground and stationary box
retain their no-slip constraints, and incoming reservoir momentum is quiescent.
This is an explicit idealized forcing model, not a prescribed inlet or a
meteorological/turbulence model.

For each native momentum step, the existing advection/viscosity/buoyancy
predictor is followed by the force substep, then the ordinary pressure solve:

```
alpha = -expm1(-strength * dt / response_s)
u_candidate += alpha * (target_m_s * min(1, z / ramp_height_m) - u_candidate)
```

Only unblocked X faces change. The schedule samples absolute native step indices
at their left endpoints. `strength: 0` releases the fluid without rewriting its
velocity; `target_m_s: 0` with positive strength instead brakes it. Reversal uses
a negative target. Existing CFL, divergence, contrast, work and scalar budget
checks remain; a failed step preserves accepted physical state.

An example finite gust at native `dt=0.005s` is:

```json
{
  "open_domain_forcing": {
    "schema": "physics_sim_open_weather_x/v1",
    "ramp_height_m": 0.25,
    "response_s": 0.2,
    "interpolation": "hold",
    "momentum_backflow": "quiescent_exterior/v1",
    "knots": [
      {"step": 0, "target_m_s": 0.5, "strength": 0},
      {"step": 80, "target_m_s": 0.5, "strength": 1},
      {"step": 240, "target_m_s": 0.5, "strength": 0}
    ]
  }
}
```

This starts driving at0.4s and releases at1.2s. Up to16strictly ordered knots
are admitted. `hold` and `linear` interpolation are explicit. Prescribed
`wind_forcing` and fixed-MAC diagnostics are mutually exclusive with this mode.
Old no-policy checkpoints keep their original field set.

## Accounting and restart

`open_domain_forcing_state` is a separate versioned receipt containing signed
`external_work_j` and nonnegative cumulative `air_in_m3`/`air_out_m3`. Work is
the exact discrete MAC kinetic change of the force substep, with the existing
face dual-volume weights. It excludes pressure, advection, viscosity and thermal
heat. The ordinary ten-term kinetic ledger remains intact and its inlet-work
term is zero in this mode. Stored kinetic energy closes against that ledger
plus the separate external work. These are discrete mechanical accounting
terms; they are not all thermodynamic heat or a closed-universe energy model.

Air volumes integrate actual projected normal boundary velocities, including
both X/Y sides and the open top. No inlet volume is prescribed. Accepted work,
air, fields and ledger commit together; the existing inert-object rollback
captures the added state too. Native JSON restoration and independent host
validation enforce policy/configuration, native clocks, finite state and
mechanical/thermal identities.

## Bounded source workflow

Use the registered Main Edit checkout and fresh task-owned build/output roots.
The native build/test targets are `open-weather-workers`,
`test-open-weather-native` and `test-open-weather-sanitize`. The opt-in worker
boundary tests are `tests/test_open_weather_native.py`; their explicit fixture
inputs include a retained predecessor worker and retained Fire pulse. They
verify strict/sanitizer/packet equality, unchanged legacy DATA, quiescence,
sealed state/configuration refusal and missing-feature output refusal.

`coupling.open_weather_cases.configuration(workers, profile, grid)` provides
`none`, `continuous`, `gust`, `fade` and `reversal` at16³/32³ with fixed source
laws and object physics. `fire_fluid_open_weather.py` runs the bounded campaign:

```
--workers <retained-workers.json>
--qualification <passed-weather-worker-qualification.json>
--output <fresh-contained-root>
--budget-root <owning-task-root>
```

It runs all five16³profiles, then continuous/gust/reversal32³, serially. Each
case keeps Fire dt0.02s, Fluid dt0.005s, exchange0.1s, delivery cutoff1.6s and
horizon3.2s. Unique branch metadata distinguishes live histories without
changing the physical source. Actual source emissions vary with held feedback;
thermal comparisons therefore report raw input budgets rather than silently
normalizing them.

Lossless joint SQLite checkpoints are authority. Selected native f64 snapshots
are convenient views, with explicit layout and hashes. Wake observations use
the actual low box and fixed physical regions on both X sides; reversal does
not move or relabel them. Signed flow/vorticity observations are diagnostics,
not a spatial-convergence result. The authored object remains h=2W/(m²K),
C=10J/K and0.125m support; that nonlocal exchange is not a calibrated wall law.

## Accepted comparison envelope

At 3.2 s the 32³ continuous/gust/reversal cases have return mean X velocities
+0.210865/+0.094575/-0.148114 m/s. Gust force ends at 1.2 s and reversal drive
releases at 2.4 s; moving flow remains while external drive work stays fixed.
Signed reversal work and open-face air/thermal/kinetic accounting pass. Maximum
thermal/kinetic defects are 1.660e-15/4.648e-16 J. The all-five-open-face analytic
volume reference passes optimized and sanitizer builds.

Raw 16³/32³ warming-curve differences are 13.4605%, 20.2295% and 3.02818%;
per-final-input normalized values are 13.4907%, 20.2524% and 3.02985%. These
are grid sensitivities, not convergence. Actual source dose varies with live
feedback, so cross-profile heating comparisons do not isolate transport alone.
Next accuracy work needs an independent scalar/object reference, then a finer
matched-input wake comparison. No atmospheric inflow profile or calibrated
hot-fire/material qualification is implied.
