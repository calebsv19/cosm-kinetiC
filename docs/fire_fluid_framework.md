# Native Fire–Fluid execution framework

The opt-in source-development host in `scripts/fire_fluid.py` runs native
GrowthSim Fire and PhysicsSim periodic or solid-floor/open-top atmosphere solvers under the pinned
Core Sim 0.8.1 exact-time, schedule, integrated-exchange and joint-progress APIs.
It leaves the existing offline coupling entrypoints available.

Core Sim validates a rational model clock, aligned windows and participant
cadences, uniform-rate cumulative transfer fractions, explicit Fire-before-Fluid
phase availability, and consecutive joint transitions. The host verifies full
SHA content/configuration identities, spatial mapping and physical budgets.
A single SQLite transaction publishes both native states, the joint checkpoint
and replay receipt. Failed staging/publication retains the previous accepted
head. Persisted history is revalidated before reuse. Opaque Core Sim IDs are
52-bit projections of host SHA identities; full SHA values remain authoritative.

Build fresh source workers in caller-selected roots inside each checkout/build;
never run `make clean` on a retained lane. From the PhysicsSim checkout:

```sh
make BUILD_DIR=build/fire-fluid-dev-001 coupling-framework-worker evolving-atmosphere-worker
make BUILD_DIR=build/fire-fluid-dev-001 test-fire-fluid-framework \
  FIRE_FLUID_GROWTH_ROOT=/absolute/selected/growth_sim \
  FIRE_FLUID_FIRE_WORKER=/absolute/fresh/growth_sim_fire_source_worker
python3 -B scripts/fire_fluid_regression.py \
  --growth-root /absolute/selected/growth_sim \
  --fire-worker /absolute/fresh/growth_sim_fire_source_worker \
  --fluid-worker build/fire-fluid-dev-001/evolving-atmosphere/physics_sim_atmosphere_worker \
  --clock-worker build/fire-fluid-dev-001/coupling/physics_sim_coupling_clock \
  --output /absolute/fresh/fire-fluid-proof --refinement
```

Build the Growth worker with `make BUILD_TOOLCHAIN=clang
FIRE_SOURCE_BUILD_DIR=build/fire-fluid-dev-001/fire fire-source-build` in its
selected source lane. The worker's opt-in `clock` command and Python
`advance_exact` adapter validate a declared rational cadence before evolution.
Existing producer bundle/frame formats and numerical rules are retained.

The regression emits a reusable `baseline/config.json`. Explicit operations:

```sh
python3 -B scripts/fire_fluid.py --config /path/baseline/config.json --store /fresh/store init
python3 -B scripts/fire_fluid.py --config /path/baseline/config.json --store /fresh/store step --revision 0 --operation-id first
python3 -B scripts/fire_fluid.py --config /path/baseline/config.json --store /fresh/store inspect
python3 -B scripts/fire_fluid.py --config /path/baseline/config.json --store /fresh/store export --output /fresh/scene
python3 -B scripts/fire_fluid.py --config /path/baseline/config.json --store /fresh/store observe-velocity --height-m 0.0625
```

An operation ID binds its original revision. Reuse that exact request after an
uncertain result; changed requests reject. Restart requires the same configuration,
worker and adapter bytes. Source changes require a fresh run or a separately
specified migration. Set `PHYSICS_SIM_EXPERIMENT_ROOT` to an isolated evidence
root for native acceptance diagnostics; the regression sets it beneath its output.

The reference is 0.2 model seconds, Fire 0.02 s, Fluid 0.005 s, exchange 0.04 s,
a 9x9 Fire grid over a 0.75 m patch and an 8x8x4 periodic fluid domain of
1x1x0.5 m, with a fixed 0.125 m physical injection depth across grids.
Refinement halves both timesteps, the Fire spacing separately, and
all three fluid spacings separately. Source calibration is authored. Periodic
transport conservation does not establish combustion calibration, open-boundary
plume behavior or buoyancy accuracy. Two-resolution differences are diagnostics,
not an observed convergence order. This small reference complements the retained
longer plume experiments; it does not replace their physical acceptance.

Export creates a read-only combined-scene/VF3D snapshot, a derived compatible
scene checkpoint, the original `joint_checkpoint.json` and a digest-bound
`joint_projection.json`. It does not modify an existing authored scene. Numerical
feedback must use native fields, never the render carrier.

`physics_sim_fire_velocity_observation/v1` samples native lower-face
MAC velocity with double-precision trilinear interpolation and projects it onto
orthonormal Fire U/V tangents. It binds the native/joint checkpoint, model ticks,
maximum age, m/s units, world frame, sample height and source dimensions. Sampling
outside the domain, at a boundary height, or from stale/future state rejects.
The solid-floor model uses periodic XY, an impermeable no-slip bottom and open
top, with the native separate top normal face and odd tangential floor ghosts.
It supports a body-free domain; internal solid obstacles are outside this model.
The emitted observation reports `applied_to_fire: false`: the existing timed Fire
default profile remains zero-wind. The separately versioned response law and
explicit prior-state hold channel are described below. Neither public worker activation nor live desktop/session attachment
is part of this source-development host.

## Matched spatial and solid-floor qualification

`scripts/fire_fluid_spatial.py` saves one 17x17 native Fire timeline and applies
those identical frames to 8x8x4, 16x16x8 and 32x32x16 Fluid meshes. All cases retain
0.125 m physical injection depth, 0.005 s Fluid steps and 0.2 s duration. Compare
extensive energy and smoke after conservative restriction onto the same 8x8x4
observation partition. Pair-native partition errors are separate diagnostics;
ratios measured on different partitions do not establish a convergence order.
The current common-partition energy L1 difference decreases from 3.5713% to
1.0825%, and smoke from 4.2062% to 2.0516%. Fine-detail differences do not yet
establish the asymptotic regime, and these are transport rather than physical
combustion calibration results.

The script then uses the joint framework for a no-slip-floor/open-top case with
native buoyancy enabled and authored horizontal shear. Checkpoint conservation
includes initial content, integrated source, reservoir inflow and outflow. Return
sampling uses a fixed 0.0625 m physical height and the accepted joint checkpoint.
This observation remains `applied_to_fire: false`.

Build `open-atmosphere-worker` into a fresh `BUILD_DIR`, then invoke:

```sh
python3 -B scripts/fire_fluid_spatial.py   --growth-root /absolute/growth-main-edit   --fire-worker /absolute/growth_sim_fire_source_worker   --fluid-worker /absolute/physics_sim_atmosphere_worker   --open-worker /absolute/physics_sim_open_atmosphere_worker   --clock-worker /absolute/physics_sim_coupling_clock   --output /absolute/fresh/spatial-proof
```

`--reuse-spatial /absolute/previous/spatial-proof` reuses the three native matrix
results after exact source-frame, result/state digest, configuration, worker and
budget validation. It writes hashed references instead of copying the large
fields. The ground joint store is always fresh. New framework adapter bytes
change the store binding; previous checkpoint stores remain retained and require
their original source controls for continuation.

Tests: `test_coupling_convergence.py`, `test_coupling_velocity.py`,
`test_coupling_ground.py` (explicit `FIRE_FLUID_GROWTH_ROOT`,
`FIRE_FLUID_FIRE_WORKER`, `FIRE_FLUID_OPEN_WORKER`, `FIRE_FLUID_CLOCK_WORKER`),
plus the existing framework and open-atmosphere contracts. Run native worker
consumers sequentially because each worker's build root has an ownership lock.


## Development: accepted-prior held wind channel

The explicit `return_channel` config is:

```json
{"schema":"fire_fluid_prior_velocity_hold/v1","sample_height_m":0.0625}
```

It requires Fire profile `physical_rates_heat_advection_v1` and the qualified
solid-floor/open-top Fluid policy. Absence retains the existing one-way profile;
profile/channel mismatches reject before solver work or store creation.

At each window begin, the host samples the prior accepted Fluid checkpoint,
including the accepted initial checkpoint at tick0. That native observation is
held unchanged across every Fire tick in the window. Core Sim admits the third
channel as POINT/HOLD, Fluid→Fire, with maximum age equal to one exchange window.
The transfer binds the prior Fluid checkpoint, the held payload and the staged
Fire consumer checkpoint; its end-of-window request proves availability through
the full hold interval. Phase order remains Fire then Fluid. A current staged
Fluid field is not available to that Fire window. No lookahead or interpolated
staged feedback is used. Each later window samples its newly accepted predecessor.

Each joint checkpoint stores the full observation, held window, wind input and
its SHA in `evidence.held_return`; restart recomputes that payload from its prior
accepted state, verifies the producer operation digest and exact binary32 native
wind fields, and replays Core Sim joint acceptance. Both solvers and the receipt
still publish in one transaction. Sample-only observations report
`applied_to_fire:false`; the consuming held-return evidence reports true.

Run the bounded enabled/disabled/time-half qualification using explicit workers:

```sh
python3 -B scripts/fire_fluid_feedback.py --growth-root /absolute/growth-main-edit --fire-worker /absolute/growth_sim_fire_source_worker --fluid-worker /absolute/physics_sim_open_atmosphere_worker --clock-worker /absolute/physics_sim_coupling_clock --output /absolute/fresh/feedback-proof
```

The 0.2s baseline has five0.04s exchanges, ten Fire steps and forty Fluid steps.
Enabled feedback transfers2.2618267103J, versus2.2591471206J with the channel
absent; both timesteps halved changes the enabled total by0.023474%. Each case
passes exact replay/recovery, failure rollback, stale refusal and scene/VF3D
validation. These qualify the bounded numerical response and causal channel,
not physical calibration, long-duration/high-wind accuracy or live activation.
New adapter/worker bytes require fresh stores; retained earlier stores keep their
original bindings and cannot silently resume with this implementation.


## Sensitivity, stronger/longer cases and authoring review

`scripts/fire_fluid_qualification.py` runs nine bounded native cases with exact
replay/recovery, stale refusal, staged-failure rollback and scene/VF3D validation.
At fixed0.4s duration it tests return heights0.03125/0.0625/0.125m and exchange
windows0.02/0.04/0.10s. Native cadences stay0.02/0.005s and source depth0.125m.
The stronger case multiplies authored horizontal shear by20 (initial tangent
wind0.5/0.125m/s at0.0625m), while return fields continue evolving natively.
Long cases reach1s and2s; the2s strong case is repeated with1m domain height,
preserving0.125m Fluid spacing and the same physical source and shear gradient.
All cases respect the existing32-window and native step/work bounds.

Measured energy differences: height half−0.179284%, height double+0.338989%;
exchange half−0.0000344%, slow+0.0000745%; stronger wind+5.06474%. Doubling domain
height for the2s strong run changes stored energy−0.0001275%. These establish
bounded sensitivity diagnostics and conservation/recovery, not a prescribed
physical-accuracy tolerance or convergence order. Longer horizons are unqualified.

Authoring review (`scripts/fire_fluid_review.py`) reads accepted stores without
mutation and emits interval SI observables, native field maps, a ReportLab source
response chart, and compact wind/height/hold/checkpoint provenance. Run with the
bundled document Python runtime for Pillow/ReportLab. Fire heat maps use clipped
nodal dual support; temperature maps use native cell fields. No render carrier
supplies numerical authority. Existing source scenes/layouts are preserved.

The strong2s case accumulates49.52% of area-weighted normalized Fire heat proxy
in the right-edge nodal column. The current versioned Fire patch is closed to
heat outflow; this is a boundary-policy effect. A taller Fluid domain barely
changes it. Physical interpretation needs a declared appropriate Fire thermal
boundary/model before calibration. Exports also declare surface-field shading
false: native review maps are available, but the exported plane remains static
and no live authoring controls or human visual acceptance are established.

Relevant local fixtures are demonstration-only. The NIST medium-scale pool-fire
reference (https://www.nist.gov/el/fcd/structure-medium-scale-pool-fires) provides
fuel-specific measured data and uncertainty, but source geometry, material,
forcing and thermal regime do not match the present synthetic case. No parameters
were fitted and physical calibration remains unqualified. A compatible measured
reference, uncertainty and independent validation subset are required.

Repeatable commands (explicit selected workers, fresh output roots):

```sh
python3 -B scripts/fire_fluid_qualification.py --growth-root /absolute/growth-main-edit --fire-worker /absolute/growth_sim_fire_source_worker --fluid-worker /absolute/physics_sim_open_atmosphere_worker --clock-worker /absolute/physics_sim_coupling_clock --output /absolute/fresh/sensitivity-proof
/path/bundled/python3 -B scripts/fire_fluid_review.py --suite /absolute/sensitivity-proof --output /absolute/fresh/authoring-review
```


## Development: open Fire patch and measured material reference

`physical_rates_heat_outflow_v1` is an explicit alternative to the retained
closed `physical_rates_heat_advection_v1`. It interprets heat as a transported
ignition proxy near a stationary fuel patch, not a solid temperature. Interior
upwind fluxes use the same clipped nodal dual support. At each patch edge, outward
velocity uses interior proxy and inward velocity uses zero ambient proxy. Timed
reaction/cooling and reflecting (zero-flux) diffusion remain the existing law.
Wind bounds, positivity CFL and work caps remain unchanged. No fuel moves across
patch edges; the proxy outflow is not an energy transfer into Fluid.

Native output supplies a separate `growth_sim_fire_proxy_budget/v1` diagnostic
in normalized-proxy m²: initial/final integral, advective outflow, net change from
reaction/diffusion/cooling, and accumulated transport rounding. The adapter binds
the observations to native states and validates the balance and rounding bound.
The usual SI fuel/energy/smoke quantities remain separate. Profile identity binds
config/checkpoints; no historical checkpoint is silently migrated.

`scripts/fire_fluid_boundary.py` compares closed/open strong 2s cases and an open
case with both timesteps halved. All three pass native admission, conservation,
exact replay/restart, stale refusal, failure rollback and scene/VF3D export.
Downwind edge proxy fraction decreases from49.5216% to8.29355% (half-time8.30919%).
Integrated Fluid source is36.9762735J closed versus33.7620167J open (−8.69276%).
Half timesteps give33.6925516J (−0.205749%). These are numerical diagnostics;
no prescribed physical accuracy or convergence order is established.

The first material benchmark selected for further physical development is the
NIST/MaCFP 30cm methanol pool fire, TN2162r1
(https://doi.org/10.6028/NIST.TN.2162r1). The downloaded CSVs are pinned to MaCFP
commit213206ec45b91b2bd7d5f344ebd519662404892b with SHA readback.
`scripts/fire_fluid_reference.py` validates bytes and units, converts centerline
temperature/velocity and surroundings heat flux to SI, and reports compatibility
against a selected configuration. The HRR file is an idealized steady600s interval
at18.6kW with0.000932kg/s mass loss; it is not a measured ignition transient.
Radiative gauge data outside the pool are not fuel-surface heat feedback.

This is a reference selection and compatibility assessment, not calibrated
methanol simulation. Reported gas temperature reaches1290K, beyond the current
low-contrast Boussinesq admission. Geometry, warm-up/steady conditions, material
state and radiative/convective feedback also differ. A liquid surface energy and
vaporization law, incident heat flux/loss accounting, circular fuel support and
an appropriate gas-temperature/density model are required before comparison.
Reserve centerline temperature/velocity and surroundings flux as validation
observables; no wind-response calibration can be inferred from this steady case.
No parameters have been fitted. Live authoring controls remain a later step.

Repeat the boundary proof with explicit selected workers and fresh outputs:

```sh
python3 -B scripts/fire_fluid_boundary.py --growth-root /absolute/growth-main-edit --fire-worker /absolute/new/growth_sim_fire_source_worker --fluid-worker /absolute/physics_sim_open_atmosphere_worker --clock-worker /absolute/physics_sim_coupling_clock --output /absolute/fresh/boundary-proof
python3 -B scripts/fire_fluid_reference.py --reference /absolute/pinned-csv-root --config /absolute/boundary-proof/open-strong/config.json --output /absolute/fresh/reference-assessment.json
```

## Native methanol liquid surface qualification

Growth Main Edit now contains a separate native SI replenished liquid pool model.
`scripts/fire_liquid_qualification.py` compares its prescribed-flux energetics with
NIST/MaCFP methanol data, retaining property and measurement provenance. Native
heating/boiling/feed-heating/cooling, budget binding and exact restart pass. A600s
prescribed steady case gives1.24476g/s versus NIST report0.925052g/s, +34.56%.
No loss coefficient is fitted; the missing-loss diagnostic is not an input.
The imposed boiling property is within the measured thermal-drift envelope, not
a validated independent surface-temperature prediction.

This is surface numerical qualification only. It neither changes the existing
wind-only return channel nor admits measured1290K gas in the constant-property
Fluid model. Independent thermal boundary/loss data, reciprocal gas-to-liquid
energy transfer and hot-gas qualification precede physical coupled calibration
and authoring controls. Existing proxy baseline remains separate and reproducible.
Evidence: `_private_workspace_artifacts/fire-fluid-material-20261009/`.

## Experimental boundary and prescribed-source gas admission

`coupling/methanol_boundary.py` curates the primary2019 apparatus: 0.301m pan,
0.010m lip, 293.15±2K cooled bottom, and gauge0.013m above the liquid. It explicitly
refuses treating this gauge measurement as exact liquid-surface forcing. The2019
surface comparison uses that study's13.5g/m²/s mass flux and1.6kW gauge reading;
2024 MaCFP mass/plume measurements remain a distinct case. NIST-hosted author-
manuscript property tables differ from the published version, so no latent-heat
value is silently imported. Public web inspection supports curated facts; local
PDF download returnedHTTP403, and no PDF byte pin is claimed.

Growth's new separate native boundary accounts for reflection, net surface
radiation and fixed cooling-reservoir transfer, with integrated linear Cp and
backward-Euler temperature solve. Five2000s prescribed synthetic boundary cases
pass native energy budgets and exact recovery. Actual surface flux, optics,
conductance, stratification and temperature-dependent material coefficients
remain unresolved; experimental surface energy closure is not qualified.

`coupling/prescribed_pool.py` builds an exact circular cell-overlap support and
hash-pinned measured fuel packet:0.000932kg/s and19.9MJ/kg give18,546.8W potential
complete-combustion power. Fuel is never carried in the smoke tracer. Its heat-only
transport probes do not implement combustion or full fuel admission. The current
native gas worker passes the0.1% power ordinary smoke and a full-power5ms step
(peak323.053K), but refuses the two-step full-power case. Its admitted330K ceiling
cannot cover the reference1290K peak. Native error text groups several refusal
causes; no more specific terminal diagnosis is claimed. Fuel/species, radiation,
variable-density hot-gas dynamics, burner geometry/open lateral entrainment and
steady measured plume statistics are missing. Gas qualification is **not passed**.

Run `scripts/fire_thermal_qualification.py` with explicit `--growth-root`,
`--thermal-worker`, `--fluid-worker`, `--reference` and a fresh `--output`.
It retains ordinary success, the first full-power step, failed full-power request,
accepted surface states/receipts and sealed assessments. Reciprocal thermal
exchange and authoring controls remain disabled pending physical qualification.
Latest evidence: `_private_workspace_artifacts/fire-fluid-thermal-20261009/accepted-final/`.

## Conservative hot-gas numerical foundation

The next separate native module now evolves density, momentum and total energy
with a calorically perfect ideal-gas EOS and acoustic CFL checks. It passes
hot-state, conservation, XYZ acoustic transport, contact refinement and exact
restart tests, including sanitized builds. Four independent Growth surface-law
checks pass. See [the formulation, measured-power transport proof and remaining
physical boundary](hotgas_foundation.md). This does not replace the existing
Boussinesq proxy or connect thermal return: periodic Euler numerics are qualified,
but open buoyant burner flow, mixture properties, physical surface transfer,
combustion and radiation remain unqualified.

The subsequent [domain and conductive sampling checkpoint](hotgas_domain.md)
implements solid/reservoir faces, conservative gravity and explicit conduction.
Boundary/hydrostatic/manufactured-plane checks and exact recovery pass. A longer
hot slab reverses its selected-region velocity; a localized region stays upward
but is strongly damped. Physical buoyant accuracy remains unqualified. Native
observations at the surface and 13 mm distinguish conduction from bulk enthalpy;
they contain no radiation or modeled gauge absorption and do not enable thermal
return. Existing wind-only held return remains unchanged.

The later [contact/pressure and bounded mixture checkpoint](hotgas_lowmach.md)
adds explicit HLLC mode, isolates stationary-contact numerical mixing and checks
acoustic/boundary response. It improves localized buoyancy but retains the failed
slab and material mesh sensitivity. The NIST mixture reference is not yet the
native EOS; reciprocal thermal return remains disabled.
