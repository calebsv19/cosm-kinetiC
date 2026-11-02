## 2026-10-28 — Gust refinement exposes pressure-backflow sensitivity

Fixed-source8/16/32/64³diagnostics show late growth after gust shutoff even with
zero heat/smoke. Original64³hits the unchanged CFL gate at2.335s; smaller gas dt
has a much smaller effect than grid or boundary policy. A contained quiescent
backflow-momentum candidate completes64³/3.2s, but remains unadopted and spatially
sensitive. Existing ordinary solver/defaults are unchanged. See
[the diagnostic and boundary findings](fire_fluid_gust_diagnostics.md).

---

# Prescribed inlet wind and Fire–Fluid comparisons

This opt-in source/headless path drives the native gas solver through an X
boundary. It supports continuous wind, an abrupt finite gust and a linearly
ramped gust. Fire receives only the previously accepted gas velocity through
the existing HOLD return channel. No Fire wind field is directly authored by
the inlet controller, and interior gas velocities are never reset each tick.

## Native boundary contract

Use `ambient_open_xy` with `solid_bottom_open_top`. One X boundary prescribes
normal velocity; the opposite X boundary, Y boundaries and top retain zero
pressure ambient reservoirs. The inlet has homogeneous Neumann pressure
correction so projection preserves its prescribed normal velocity. Tangential
predictors retain the zero-gradient lateral policy; the floor retains its
no-slip ghost and exactly zero normal velocity. An optional aligned insulated
box uses the same mixed pressure equation and mask.

The normal inlet profile is
`signed_speed * min(1, z_cell_center / ramp_height_m)`.
Speed is nonnegative; `x_min` means +X and `x_max` means −X. The ramp height is
at least one gas cell and no greater than domain height. A zero command holds
the inlet normal velocity at zero; it does not convert that face into a
pressure outlet. Direction is fixed per configuration. Reversal through one
inlet, arbitrary apertures and wall geometry are outside this contract.

The optional `wind_forcing` member of an open-atmosphere request is:

```json
{
  "schema": "physics_sim_driven_x_inlet/v1",
  "inlet": "x_min",
  "ramp_height_m": 0.25,
  "interpolation": "hold",
  "knots": [
    {"step": 0, "speed_m_s": 0.0},
    {"step": 80, "speed_m_s": 0.5},
    {"step": 240, "speed_m_s": 0.0}
  ]
}
```

At a 0.005 s momentum cadence this gust switches on at 0.4 s and off at 1.2 s.
There are 1–16 strictly increasing integer step indices, beginning at zero,
and speeds in [0,10] m/s. Existing CFL, work, positivity, temperature contrast
and projection residual gates can refuse a configuration's actual evolution.
`hold` changes command at a knot; `linear` interpolates between knots. The last
value is held. Every native step samples its **left endpoint** using the absolute
accepted step counter. Wall time and floating-point event-time comparisons do
not choose events. An abrupt gust changes the boundary in one native step;
gas momentum and scalar transport still evolve, and Fire responds at a later
accepted exchange.

## Checkpoints and accounting

The schedule belongs to the immutable configuration digest. Native checkpoints
with forcing add `wind_air_in_m3`, `wind_air_out_m3` and
`wind_inlet_volume_m3`; the adapter independently verifies the integrated inlet
command, prescribed face values and lifetime air-volume balance. Air mass is
the constant reference density times these volumes. Existing per-face excess
heat and smoke inflow/outflow receipts remain authoritative; this is an
incompressible reference-density model, not oxygen/species combustion.

`wind_observation` reports the last and next command, inlet and integrated air
volumes. A restart keeps the absolute native step and counters, so it cannot
restart or shift a gust. Failed native staging does not publish fields, clocks
or air counters. Framework continues to verify accepted-prior gas identity,
Fire consumption, exact native clocks and atomic joint publication.

Without `wind_forcing`, the existing configuration/state shapes and default
boundary behavior remain in use. Native checkpoints remain bound to exact
worker bytes. Historical joint journals also retain their adapter-source
bindings; changing source does not silently migrate or rebind old evidence.

## Build and run

Use a fresh owned `BUILD_DIR` inside the selected checkout; never clean retained
build roots. The native targets are:

```sh
make BUILD_DIR=build/<fresh-owned-root> open-atmosphere-worker coupling-framework-worker
make BUILD_DIR=build/<fresh-owned-root> test-driven-wind-native test-driven-wind-sanitize
```

`make test-driven-wind` additionally runs the native-worker Python admission and
event/restart tests; set `FIRE_FLUID_GROWTH_ROOT` and `FIRE_FLUID_FIRE_WORKER`
to the selected Fire source lane and pinned binary.

`scripts/fire_fluid_driven_wind.py` accepts `--growth-root`, `--fire-worker`,
`--fluid-worker`, `--clock-worker` and a fresh `--output` directory. It compares
zero, continuous +X/−X, abrupt +X and ramped +X, with feedback off/on, followed
by a timestep-half control and a box case matched to continuous +X. Native
workers execute sequentially under the existing exclusive owner guard.
The abrupt and ramped gusts have equal integrated inlet flow. Their different
histories test response to command shape; sustained wind has a larger total
delivered flow over the same run.

The bounded recipe uses 17×17 Fire nodes and 8³ gas cells for 3.2 s, with 0.1 s
exchange, 0.02 s Fire and 0.005 s Fluid cadences. These are authored warm-flow
integration comparisons. They do not establish spatial convergence, calibrated
hot combustion, general ventilation, heated-object accuracy or ignition from
gas/solid thermal feedback. The desktop UI and installed app do not expose
this new source/headless control.

The explicit [quiescent-backflow policy and discrete kinetic ledger](fire_fluid_quiescent_backflow.md) add pressure-reservoir exterior momentum control and a bounded lossless journal for finer live-coupled comparisons. Legacy defaults remain available.
