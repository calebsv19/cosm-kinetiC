# Controlled source-to-object transport

This development-only native diagnostic verifies a measurable causal path:
controlled SI heat source → ordinary gas transport → existing inert exchange.
It is separate from native Fire and the application/worker protocols.

## Geometry and law

Unit channel with full X inlet, opposite pressure outlet, no-slip floor and
slip sides/top. The no-slip stationary box is[.375,.625]³. Only its upstream
face exchanges heat, with h=2W/m²K,C=10J/K and the existing fixed.125m gas
support x[.25,.375), y/z[.375,.625). It is an authored nonlocal Robin support,
not a calibrated wall-gradient convection model.

Gas and object start at300K. The heater occupies x[.125,.1875), with the same
y/z footprint. A.0625m unheated gap separates it from exchange support. Its
100W pulse ends at.4s; total40J; all runs end at1.2s. Grid-aligned physical
bounds and total power stay fixed at16³/32³. Buoyancy is disabled to isolate
prescribed0.5m/s transport; thermal diffusion remains enabled. Wind-off changes
only prescribed inlet speed. Source-off and exchange-off change only their
named control. No source cell belongs to solid or exchange support.

## Standalone command

Use a fresh task-owned BUILD_DIR and preserve retained output:

```sh
make BUILD_DIR=build/<fresh-owned-root> source-object-probe source-object-sanitize
build/<fresh-owned-root>/flow-validation/source-object 32 .0025 0 /absolute/fresh/history.csv
```

Mode0 enables source/exchange/wind;1 disables source;2 disables exchange;
3 disables wind. Existing CSV paths refuse overwrite. Output includes support
and object temperature rise, input, stored gas energy, signed object transfer,
net outflow and total-energy defect. Object state uses compensated temperature
rise. Existing gas contrast, pressure/divergence, scalar and allocation gates
remain unchanged. The probe is not exposed through a new worker JSON mode.

## Measured outcome and declared acceptance

## 2026-10-29 — Measurable source-to-object transport completed

The new standalone native diagnostic starts gas/object at300K, deposits a
controlled100W/.4s pulse upstream, leaves an unheated.0625m gap before the
existing fixed.125m exchange support, and evolves ordinary gas transport.
No heat is inserted directly into support or solid. At32³/dt=.0025s, support
peak is1.743671K above reference and object rise0.013465K, with0.134650J transferred.
Source-off support/object stay exactly zero; exchange-off object/transfer stay
zero while support warms. Moving flow heats the object1.116e+06 times more than the
wind-off diffusion control at32³/dt=.005s. Maximum total-energy defect6.39e-14J.

Nine scientific cases, three16³ASan/UBSan cases, and three previous inert
controls pass. Transfer timestep-half difference0.194423%;16³→32³difference6.9966%
is reported as sensitivity, not convergence. Fresh strict diagnostic builds
pass. App build/smoke is not repeated for this standalone probe-only addition;
prior full app gates remain retained. Numerical/physical diffusion means the
first small support threshold is not a sharp physical arrival time.

The controlled SI heater is not native Fire. Prescribed flow disables buoyancy;
authored h=2W/m²K,C=10J/K and fixed support are unchanged. No solver, native Fire,
shared, installed/default runtime or persistent participant changes. Next is
atomic live thermal checkpoint/restart, followed by native Fire source intake.
No commit, VERSION, release, installation, Desktop or remote effects. This
standalone qualification tool does not recommend a release version change.

Evidence: `_private_workspace_artifacts/fire-fluid-source-object-20261029/RESULT.md`.
Reference: `docs/fire_fluid_source_object.md` in PhysicsSim Main Edit.


Declared minimum signal is.1K support peak and.001K object rise for moving
source-on; source-off stays within1e−12K, exchange-off transfer is exactly
zero. Moving/still object signal must be at least10x. Total defect<=1e−7J,
object-energy/transfer mismatch<=1e−9J, source input40J within1e−10J and
32³ timestep transfer change<=5%. Every gate passes without changing the
original source/exchange law or weakening thresholds.

The next step is durable object state and energy receipt integration into the
existing live atomic checkpoint/restart. Native Fire source intake follows
that boundary. No ignition, combustion, radiation or live object coupling is
claimed by this controlled-heater test.

The subsequent [durable live object](fire_fluid_inert_checkpoint.md) adds native
checkpoint/restore and live Fire intake while preserving this controlled-heater
reference. Its gas operators and exchange law are unchanged.
