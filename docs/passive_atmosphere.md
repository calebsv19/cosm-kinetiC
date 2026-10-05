# Qualified offline passive atmosphere transport

Source-only model: `periodic_constant_property_passive3d_v1`. This is body-free,
constant-density, one-way sensible thermal-energy and smoke-tracer transport.
It reuses Cartesian cell/face conventions and the numerical allocation budget;
no existing momentum, pressure, visual dye, buoyancy or force equations change.

## Run and qualify

```sh
make passive-atmosphere-worker
PASSIVE_QUALIFICATION_DIR=build/passive-atmosphere/qualification make test-passive-atmosphere
make test-passive-atmosphere-sanitize
python3 -B scripts/passive_atmosphere.py --request /path/to/request.json --output /new/path/result.json
```

GrowthSim owns the native vertical-slice driver:

```sh
python3 -B /path/to/growth_sim_main_edit/scripts/run_physics_passive_experiment.py \
  --physics-root /path/to/physics_sim_main_edit \
  --output /new/path/experiment
```

Choose fresh output paths. The driver produces native source bundles, frames,
admission and allocation receipts through the existing CLIs. It binds the declared
receiving grid to the actual scalar domain before depositing quantities. It emits
both emission-end and post-emission fields, original requests and a sealed final
acceptance receipt. Independent reproduction starts a new isolated world using the
same frozen source identity; it does not continue or consume an existing live run.

## Model and units

Accepted domain: origin [0,0,0] m, right-handed Z-up, 4..256 cells per axis,
0.001..1000 m lengths, at most 262144 cells, periodic XYZ boundaries. Cell ordering
is X-fast. Velocity has three contiguous component blocks; each stored velocity
is the lower face of that cell. All velocities are finite and have discrete
divergence below 1e-8/s. They are borrowed read-only and fixed over each scalar
step. Periodic transport is a qualification domain, not open outdoor atmosphere.

Author explicit rho (kg/m³), cp (J/kg/K), reference temperature (K), conductivity
(W/m/K), and tracer diffusivity (m²/s). Fields are extensive sensible thermal-energy
excess E (J/cell) above reference temperature, and passive smoke mass M (kg/cell).
T = Tref + E/(rho cp V), and smoke concentration = M/V. Properties are constant;
thermal diffusivity alpha = conductivity/(rho cp). Smoke does not add bulk fluid
mass or momentum. This first model admits only nonnegative energy excess and
sources; below-reference cooling, variable properties and negative sinks need a
new reviewed formulation. The synthetic fixture is not a named-air property set.

Each shared face transfers equal/opposite extensive amounts between neighboring
cells. Upwind advection and centered diffusive face fluxes form a first-order
explicit update. These solve the constant-property passive advection/diffusion
model with volumetric sources. Integrated source J/kg is spread uniformly through
that scalar step, consistent with the source contract. No ambient-energy transport,
radiation, chemical feedback, settling, ash, buoyancy or density expansion is modeled.
Cell deposition onto the declared receiving layer is not a wall heat-flux model.

The outgoing rate at each cell sums positive outward face speeds divided by spacing
plus 2*max(alpha,D)/h² for each axis. Scalar subcycles ensure dt_sub*max(rate)<=0.8.
There are at most 4096 subcycles per step, 100 million subcycle-cell updates per
experiment, 256 supplied steps, dt<=1s, 64MiB request JSON, 120s worker deadline and
128MiB numerical-owner budget. JSON/process RSS is separate from numerical allocation.
The fluid timestep and its residual/divergence gates are never changed by this model.
Candidate scalar work does not allocate after initialization. Nonfinite, negative,
unstable or budget-failing candidates reject without changing accepted fields,
time, counters or input totals. No clipping or global mass correction hides errors.

## Public experiment request and retained result

The public request schema is `physics_sim_passive_transport_request/v1`. Exact keys:
`schema`, `grid`, `length_m`, `properties`, `initial_energy_j`, `initial_smoke_kg`,
`steps`. Each step contains only `dt_s`, `face_velocity_m_s`, `energy_j`, `smoke_kg`.
Source arrays are integrated amounts for that whole step, never W or kg/s.
Closed-key/type/domain checks, duplicate-key/nonfinite rejection and resource bounds
run before native execution. The native worker is a trusted-local private bridge.

`physics_sim_passive_experiment/v1` carries the original request, native worker
hash, fields, derived temperature/concentration, total budgets and numeric digest.
The CLI independently recomputes final sums and temperature/concentration conversions.
The periodic experiment has zero boundary outflow and modeled loss. Fields are
available in SI with grid/space declarations for later renderer adaptation; there
is no renderer scene/cache contract or visual acceptance in this milestone.
Create-only result publication uses a synced temporary file and link; an uncertain
post-link directory-sync failure may leave a valid artifact. Inspect that exact
result rather than treating it as a consuming restart. Existing output is preserved.

The allocation journal still records plans, not consumption. Applying a copied
plan in a fresh isolated experiment does not mark that journal physically applied.
Results are not restartable checkpoints. Future coupled persistence must atomically
include momentum/scalar histories, applied event identity and interval remainder.

## Independent qualification and acceptance

Native controls cover energy/tracer conservation, negative/nonfinite/divergent/
resource rejection, alias rejection, accepted-state rollback, exact cleanup,
no scalar step allocation and eight synchronized steps each for manufactured and unforced flow of byte-equal velocity,
pressure and momentum history with/without passive transport. ASAN/UBSAN also pass.
Nine Python controls independently test uniform-volume temperature, periodic
translation/subcycle flux, continuum spatial convergence for advection/diffusion,
semi-discrete temporal convergence for both, tracer shape, half rate/twice duration,
unequal steps, zero source, malformed input and immutable CLI publication.
Refinement thresholds are predeclared in source tests: spatial ratios<0.6 advection,
<0.3 diffusion; temporal ratios<0.55. Spatial smearing is expected at first order.
No reference is formed by applying the production matrix to its expected solution.

The native source-driven fixture is a 7x7 patch in a 2m cube, 16³ cells, two three-tick
intervals then 1s source-free transport, velocity [.2,.05,.1]m/s, rho 1, cp 10000, Tref 300,
k 500, D .002 (synthetic units as above). It transfers 89624.81198832394J and
0.011949974931776524kg. Both final budgets reconcile; emission-end peak temperature
447.78243745292815K decreases to 310.21644596117375K. Smoke distribution changes while
its total remains. Two isolated runs reproduce emission-end and final field bytes.
This qualifies the bounded passive model, not combustion yields or a buoyant plume.

## Ownership and next boundary

Reuse-adopted: existing app Cartesian geometry, face convention, memory budgets,
body-free velocity backend and versioned Growth source validator; shared core_space/
core_units meanings remain. Reuse-deferred: generic shared thermal transport and
core_pack containers until consumer semantics and restart are proven. core_sim
control semantics do not supply conservative scalar physics; core_math primitives
are not a finite-volume transport operator. No shared API/version/minimum changes.
PhysicsSim owns transport/material/receiving policy; GrowthSim owns native source.

Next: transactional coupled histories/consumption, independently qualified open
atmosphere boundary fluxes, then buoyancy with declared applicability and its own
momentum/energy controls. Ash needs a separately specified particle/species model.
Live MCP, installed packaging, distributed worker activation and renderer adoption
remain distinct subsequent acceptance boundaries. Earlier obstacle startup/force
research stays paused; body-free qualification does not certify obstacle behavior.
