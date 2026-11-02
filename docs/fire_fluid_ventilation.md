## 2026-10-28 — Gust refinement exposes pressure-backflow sensitivity

Fixed-source8/16/32/64³diagnostics show late growth after gust shutoff even with
zero heat/smoke. Original64³hits the unchanged CFL gate at2.335s; smaller gas dt
has a much smaller effect than grid or boundary policy. A contained quiescent
backflow-momentum candidate completes64³/3.2s, but remains unadopted and spatially
sensitive. Existing ordinary solver/defaults are unchanged. See
[the diagnostic and boundary findings](fire_fluid_gust_diagnostics.md).

---

# Fire–Fluid wind sensitivity, ventilation and recovery

The uncommitted PhysicsSim Main Edit source supports native sustained and gust
inlets, bounded channel ventilation, repeated forcing and paired-boundary
reversal. Fire consumes accepted prior gas velocity through existing exact
Core Sim scheduling and HOLD. These are warm-flow integration experiments;
reciprocal thermal feedback, hot combustion and general room accuracy remain
unqualified. GrowthSim and shared source are unchanged.

## Boundary models

`physics_sim_channel_x_inlet/v1` prescribes a fixed grid-aligned Y/Z patch at
one X face. The opposite X face is a pressure outlet; Y sides, ceiling and
unused inlet area are impermeable slip boundaries. The floor stays no-slip.
The qualified domain is 1 m cubed, with a full inlet or an elevated opening
Y,Z = 0.25–0.75 m. The optional stationary 0.25 m cube is insulated and no-slip;
it does not receive heat or ignite. This is a fixed channel, not arbitrary
geometry or a qualified no-slip room model.

`physics_sim_balanced_x_wind/v1` prescribes the same signed normal X velocity
at both X ends (`inlet: x_pair`), with Y/top ambient pressure reservoirs.
It permits a within-run sign reversal using one immutable boundary operator.
It does not switch a pressure outlet into an inlet. Channel and paired-boundary
protocols are distinct. Hold/linear knots use absolute integer native gas steps.
The original `physics_sim_driven_x_inlet/v1` and unforced paths retain bitwise
native-field/checkpoint parity on four selected old/new worker controls.

New protocols cold-start the pressure solve, apply a correction with the same
operator and close only bounded floating-point outlet imbalance before scalar
transport. Existing pressure/divergence and air/heat/smoke gates are unchanged;
large imbalance is refused. Initial failing pressure/volume attempts remain
retained with their exact diagnoses.

## Measured comparisons

At unchanged 17×17 Fire, native Fire/gas steps 0.02/0.005 s and 3.2 s duration:

| 8³ → 16³ gas | Retained heat change | Fire heat center change | Fixed-probe velocity relative RMS |
| --- | ---: | ---: | ---: |
| Continuous | +1.7694% | +3.99 mm | 2.77% |
| Continuous with box | +1.2024% | +7.25 mm | 13.53% |
| Abrupt gust | +2.0740% | +47.60 mm | 54.21% |

The box remains impermeable and stores zero solid heat/smoke at both grids.
Common-partition field distributions still differ substantially; spatial
convergence is not established, particularly after gust shutoff. A 32³ run was
deferred before execution because the measured journal cost exceeds the
existing per-journal/storage envelope. No limit was raised.

The 0.1 versus 0.04 s exchange comparison at fixed native steps and 1.2 s
endpoint changes retained heat by +0.47818% with feedback and +0.0003401%
without. The feedback-off Fire fields are exactly equal at that endpoint;
feedback-on center moves +3.289 mm. Exchange changes both source packetization
and velocity hold, so this does not isolate a return-channel-only error.

All four ventilation cases prescribe 1.40 m³ inlet volume through 3.2 s:

| Case | Final Fire heat center x | Stored heat / actual supplied heat |
| --- | ---: | ---: |
| Full inlet, 0.5 m/s | 0.6855 m | 48.24% |
| Elevated inlet, 1.75 m/s | 0.6532 m | 88.82% |
| Elevated inlet plus box | 0.6918 m | 79.35% |
| Elevated inlet, feedback off | 0.5000 m | 90.38% |

Feedback changes source heat input, so use each case's measured input for
retention fractions. Prescribed inlet volume excludes any additional pressure
outlet backflow; total air-in/out accounting is separately enforced.

Through 4.8 s, one gust ends with Fire heat center x=0.6580 m; two gusts end at
0.6871 m. Matched paired-boundary forward/reversing histories each deliver
1.05 m³ absolute prescribed inlet volume, ending at x=0.6869/0.5439 m.
The reversal at 1.2 s first changes Fire at accepted 1.6 s; the second gust at
2.0 s first changes Fire at 2.4 s. Matching earlier Fire states verifies that
future commands do not bypass the accepted-prior return channel.

## Scenario controls

The existing [local browser workbench](fire_fluid_workbench.md) exposes ten
native presets: sustained, abrupt/ramped gusts, full/elevated channel inlet,
elevated inlet with box, single recovery, repeated gusts, paired forward and
paired reversal. `--initial-profile continuous` selects a fresh initial preset;
omitting it preserves the legacy baseline. Speed, gust event/ramp times,
feedback, fuel loading, sampling height, exchange and duration are editable.
Each created run fixes its settings, preserves accepted states and reopens paused.
The panel reports last/next commanded inlet speed, actual mean sampled Fire
input and prescribed inlet volume. Native kinetiC menus are unchanged.

Fine 17×17 surfaces export native texture alongside the same-clock VF3D gas
volume. Rendering exports just the viewed revision to fresh output. Texture,
source/time, scene, volume and renderer identities are checked. The display
uses channel cutaway walls; omitted walls remain closed in the solver.
Two narrow background cards are depth/attenuation markers, not solver objects.
Optics remain uncalibrated and the smoke stays close to the floor in these
warm-flow runs. Native images do not establish a flame or a mature rising plume.

## Verification and retained evidence

Fresh Clang application build, strict native and ASan/UBSan new/original
open/lateral/driven contracts pass. Thirty-seven Python protocol/sensitivity
checks plus seven workbench workflow checks pass; seventeen fast application
targets pass. All thirteen new coupled cases pass exact restart/replay,
source consumption, budgets, stale refusal and failed-publication rollback.
Four selected native images, three comparison videos, UI controls and a UI
render were visually reviewed; videos decode fully. Pre-extension histories
retain frozen original Python controls and their original pinned workers;
bindings are never rewritten.

Exact results, original fields, media, failure diagnoses and resource decisions:
`_private_workspace_artifacts/fire-fluid-refinement-20261028/RESULT.md`.
Source is uncommitted in Main Edit; no canonical adoption, VERSION, package,
release, remote execution or installed refresh was performed. Additive feature
scope warrants a minor release when separately approved. Next numerical work
is bounded gust/wake spatial qualification with a storage-compatible checkpoint
strategy, followed by calibrated plume/thermal behavior.
