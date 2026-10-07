# Open-reservoir atmosphere policy and qualified bounded buoyancy

Implemented model: `xy_periodic_z_reservoir_projection3d_v1`. This is a body-free,
constant-density, first-order MAC projection model: horizontal XY periodicity and
open reservoirs at both Z ends. It is a separately named model; no existing
periodic or tunnel solver/checkpoint is silently changed or migrated.

## Closed boundary policy

`physics_sim_open_reservoir_boundary/v1` requires exactly:

```json
{
  "schema": "physics_sim_open_reservoir_boundary/v1",
  "horizontal": "periodic_xy",
  "vertical": "open_bottom_and_top",
  "predictor_velocity": "zero_normal_gradient",
  "pressure_datum_pa": [0, 0],
  "inflow_temperature_k": [300, 300],
  "inflow_smoke_concentration_kg_m3": [0, 0],
  "scalar_diffusion": "zero_normal_flux"
}
```

Pa/K/kg-m3 vectors are ordered bottom,top. Pressure is dynamic pressure relative
to a base hydrostatic reference, sampled on the end planes; half-cell pressure
Dirichlet gradients match the Poisson operator. Predictor velocities use zero
normal-gradient ghost closure. This pressure/velocity projection boundary is not
an exact full natural-traction Navier–Stokes boundary. Momentum convection is
explicit first-order upwind, viscosity explicit, pressure projection uses the
existing app Cartesian/sparse-MG solve, and each candidate must pass true pressure
residual and independently checked incompressibility. CFL/diffusion rate controls
reject unsuitable fixed steps. There is no ground, obstacle or periodic Z wrap.
A ground/impermeable/free-slip policy needs its own operators and tests.

X/Y velocity arrays each contain N lower faces. Z contains N+Nx*Ny faces, including
the explicit top layer; X-fast component blocks total3*N+Nx*Ny. Scalars/pressure
are cell-centered. The volume remains all-fluid. Fire's stationary surface is a
source-deposition/visual plane, not an imposed solid boundary or conjugate heat
transfer interface.

Each boundary face independently chooses scalar direction from its corrected
outward normal velocity. Inflow, including backflow, uses that reservoir's authored
T and smoke concentration. Outflow uses the adjacent cell's upwind value. Heat is
sensible energy above Tref: incoming rho*cp*(Tamb-Tref) times volume flux; smoke is
incoming concentration times volume flux. Tamb>=Tref is required by this model's
nonnegative-energy representation. Diffusion has zero normal boundary flux;
ambient Dirichlet thermal diffusion/radiative exchange is not implied. There is
no outflow clamping, periodic recirculation or implicit reinjection.

## Explicit extensive receipts and conservation

Native checkpoint boundary_fluxes contains four blocks, each2*Nx*Ny values:
energy inflow J, energy outflow J, smoke inflow kg, smoke outflow kg. Each block is
bottom faces then top faces, X-fast. Counters are nonnegative, cumulative and
monotonic. They are computed at every scalar subcycle, then published with the
candidate fields. Public boundary_receipts exposes per-face arrays and totals for
z_min (normal-Z) and z_max (normal+Z). XY periodic internal fluxes cancel.

For each quantity, public admission independently reconciles both chunk and
lifetime budgets:

`stored = initial + source_input + boundary_inflow - boundary_outflow`.

No fitted loss bucket is used. Thermal and smoke budgets are separate. Source
quantities, receiving properties and optical normalization do not change each
other. Native per-step balances, nonnegative fields, true pressure residual and
divergence are gates before publication. No total kinetic/thermal energy coupling,
viscous heating, compressibility, combustion chemistry, settling or ash is claimed.

## Opt-in buoyancy and applicability

Closed policy keys: enabled (boolean), gravity_m_s2 (0..100, gravity is-Z),
expansion_per_k (0..1/Tref), max_temperature_contrast_fraction (0..0.1, positive).
At corrected Z faces the model adds +g*beta*DeltaT acceleration, averaging the two
adjacent cell temperatures (one adjacent cell at an end face). Base hydrostatic
gravity is removed; pressure datums are perturbations to that base reference.
Thermal energy remains advected/diffused while it drives this Boussinesq force.
The force uses the previously accepted temperature (explicit first-order split).

Every reservoir and accepted/candidate field must satisfy
`0 <= (T-Tref)/Tref <= max_temperature_contrast_fraction` when enabled. beta is
bounded so relative density perturbation is also<=0.1. A late source-driven
contrast violation rejects the entire candidate, including flow and source
consumption. No clipping, calibration rewriting or hidden heat rescaling occurs.
This is a small-contrast laminar control, not a large-temperature fire-air model.

Independent controls include hydrostatic dynamic-pressure balance for uniform
thermal force; uniform thermal acceleration at two dt/grid sizes; spatially
varying thermal acceleration with a known face response; zero-expansion/disabled
feedback parity; pulse translation refinement; uniform reservoir exchange;
reversed and simultaneous per-face backflow; nonnegative scalar conservation;
exact checkpoint restart; and contrast failure. A nonlinear thermal checkpoint
run restores pressure/flow/scalar/flux history exactly under pinned worker bytes.

## Atomic checkpoints and source consumption

Public CLI: scripts/open_atmosphere.py admits
physics_sim_open_atmosphere_request/v1 and returns a sealed
physics_sim_open_atmosphere_experiment/v1. Configuration includes properties,
fixed momentum_dt_s, initial face velocity/energy/smoke, boundary_policy and
buoyancy. physics_sim_native_open_atmosphere_state/v1 stores accepted fields,
pressure warm start, per-face flux counters, source/initial budgets, integer-step
clock and scalar work count. Pressure operators/scratch are reconstructed; each
step seeds its pressure candidate from accepted pressure. Only new steps execute
on restore. Configuration, policy and worker identity changes reject.

scripts/coupled_open_atmosphere.py publishes
physics_sim_open_coupled_checkpoint/v1 in a separately bound SQLite history. Its
config keys are policy,properties,initial_face_velocity_m_s,momentum_dt_s,
boundary_policy,buoyancy,worker; initial source-receiver scalar fields are zero.
Existing source validation, clipped-area allocator and publication conventions
are reused. Source boundaries split fixed momentum steps conservatively. Native
fields, flux counters, source ranges, revision and idempotent receipt publish in
one FULL-synchronous transaction. Failed worker/timeout/SQL publication/contrast
checks leave accepted history unchanged. Copy a closed DB and matching config/
pinned bytes to continue source consumption. Checkpoint JSON alone is provenance;
it does not replace retained source frames. Equal retries preserve original
receipts. Source-free transport cannot bypass unfinished admitted intervals.

```sh
make open-atmosphere-worker
python3 -B scripts/coupled_open_atmosphere.py --config CONFIG --store STORE init
python3 -B scripts/coupled_open_atmosphere.py --config CONFIG --store STORE admit SOURCE.json
python3 -B scripts/coupled_open_atmosphere.py --config CONFIG --store STORE step \
  --revision 0 --operation-id source-first --dt .02
python3 -B scripts/coupled_open_atmosphere.py --config CONFIG --store STORE inspect
```

Limits remain bounded:4..64 cells/axis,32768 cells total, fixed dt1e-6..0.1s,
256 new steps/request,10000 lifetime steps,64 journal revisions,256 source events,
64MiB request,128MiB numerical allocation budget/conservative journal estimate,
100-million scalar subcycle-cells,120s worker timeout. Numerical budget excludes
JSON/Python/process RSS. Pressure setup is rebuilt per step to isolate failed
candidates; performance optimization is a subsequent stage.

## Retained real-source and renderer acceptance

GrowthSim run_coupled_scene.py --open-atmosphere consumes the same two frozen native
Fire bundles, with synthetic properties and an explicitly prescribed initial
1m/s upward through-flow. With --transport-after-source-s 2, the later source-free
state at2.1s records62.924017394kJ outflow and26.700794594kJ stored from89.624811988kJ
input. Smoke records0.009029123857kg outflow and0.002920851075kg stored from
0.011949974932kg input. Balance residuals are at floating-point scale. Per-face
receipts identify where quantities left. The synchronized scene stays at source
end0.1s; later transport-only fields are separate and are not combined with a held
or relabeled Fire surface.

--buoyancy-control explicitly selects a separate synthetic receiving material:
cp100000J/(kg K) (ten times the prior diagnostic), the same remaining properties,
zero initial velocity and buoyancy enabled. The material-control sidecar records
this choice; it is not named/qualified air. Fire calibration and89.624811988kJ/
0.011949974932kg source quantities stay unchanged. PeakT320.94855K is inside the
10% bound from300K. Velocity develops from zero to max0.00844657m/s; the otherwise
identical disabled-feedback source control stays at zero. This proves causal
source-to-thermal-to-flow operation within the authored model, not realistic fire
plume acceptance. The original material/source can exceed this envelope and is
not silently admitted as a buoyant air calculation.

Acceptance roots in Growth Main Edit: build/open-atmosphere/outflow-proof,
buoyancy-control and zero-feedback-control. The combined scene envelope carries
boundary/buoyancy policies, pressure-reference meaning, explicit top-face velocity
mapping and actual feedback capability. Native RayTracing scene/VF3D inputs are
unchanged. Temperature/flux/budget authority stays in physical sidecars; optical
density remains authored unitless normalization. Renderer preflight/render is
native ingestion proof, not artistic surface/flame/plume acceptance.

## Subsequent stages and reuse

Bounded analytic momentum, heating-time and pressure/thermal superposition
qualification is now complete; see the qualification section below. Stop further
atmosphere expansion until a concrete consumer needs it. Pressure/operator reuse
optimization requires measured cost evidence and must preserve rollback.
Ground/body and thermal surface-flux policies, realistic air/large-temperature
models, time-varying surface/volume scene sequences, surface materials/emission,
GUI/MCP exposure and installed-product acceptance remain separate milestones.

Reuse adopted: existing app Cartesian geometry, numerical allocator, pressure
linear/sparse-MG solve, source validator/allocator and SQLite publication; existing
core_space/core_units/scene_runtime/VF3D meanings. Runtime scheduler/core_time
services and core_pack/core_memdb storage are deferred for this synchronous app
solver. Generic shared fluid policy is deferred until multiple qualified consumers
need it. No shared API/version/adoption changes, release pointers, canonical writer
edits or RayTracing source changes occur in this batch.

## Bounded convergence qualification and stopping boundary

Run `make test-open-atmosphere-convergence`. Three analytic controls produce
`build/open-atmosphere/convergence/metrics.json` without changing worker or adapter
bytes, existing journals, boundary policies or scene ABI.

- An exact advected viscous shear solution satisfies the open Z closures and
  isolates horizontal momentum advection/diffusion. At 8, 16 and 32 horizontal
  cells, velocity RMS errors are 0.00481747, 0.00248429 and 0.00125281 m/s at
  fixed dt 0.0025 s and end time 0.1 s. Ratios 0.5157 and 0.5043 support the
  declared first-order spatial behavior for this solution.
- Constant source heating at 3 K/s, beta 1/300 per K and deliberately small
  gravity 0.001 m/s² has continuous velocity g beta heating t²/2. Errors at
  dt 0.02, 0.01 and 0.005 s halve: 1e-8, 5e-9 and 2.5e-9 m/s. Boundary cooling
  is separately bounded below 1e-8 K. This isolates source/force temporal lag;
  it is not a realistic-gravity plume convergence test.
- Authored pressure gradients oppose, cancel or reinforce uniform thermal force
  at two resolutions/time steps. Independent analytic velocity and linear
  pressure predictions agree to within 1e-11 m/s and 1e-10 Pa. Cancellation
  qualifies hydrostatic rest; reinforcing pressure qualifies driven flow.

These are targeted independent reference controls, not general nonlinear 3D
plume, joint convergence or ground-surface acceptance. Existing pulse transport,
restart, budget, hydrostatic and rollback controls remain required. No solver
correction was needed. The useful next consumer milestone is bounded reusable
coupling orchestration and synchronized scene sequences. Ground/body boundaries,
real-air large contrasts and solver optimization should follow an explicit use
case or measured limitation rather than expand this diagnostic lane indefinitely.


## Atomic source batches and 32³ corner receiving qualification (2026-10-05)

The importable `CoupledOpenAtmosphere.admit_many(values)` admits 1..256 source
frames/bundles in one transaction, reusing every existing provenance, mapping,
sequence, source-clock and byte-budget gate. Equal retries return original
receipts. Mixed old/new intervals preserve their order; a late conflict or gap
rolls back all newly inserted intervals. The worker and adapter identities are
rechecked before commit. The existing `admit(value)` delegates a one-element
batch and its response remains unchanged. This is a Python API used by the
contained experiment runner; no new CLI subcommand is implied. Source admission
alone does not consume quantities or advance a numerical state.

The change avoids repeatedly decoding a large accepted 32³ checkpoint for every
small source interval. Numerical equations, native worker, clock, projection and
contrast gates remain unchanged. Adapter identity changes require the original
adapter bytes for an existing journal; there is no implicit checkpoint migration.
All numerical/journal limits above remain in force.

GrowthSim's `scripts/fire_corner_plume.py` now runs an explicit native corner
ignition, 100 one-tick intervals, and matched 32³/16³ buoyancy and 32³ disabled
feedback lanes in a 2 m body-free cube. Source coordinates [0.5,0.5,0.25] m map
to receiving centre [0.53125,0.53125,0.28125] m on the fine grid. The cp100000
refinement candidate rejects; a separate explicitly synthetic cp800000 material
is selected from a conservative source-energy bound, with identical source bytes.
All eight samples through 5 s pass source clocks, lifetime/chunk conservation,
projection, equal-operation replay, checkpoint reopen and exact partitioned
native restart. Initial velocity is zero; disabled feedback stays exactly zero.
The buoyant 32³ smoke centroid rises 0.07962 m, about 0.07741 m above the matching
control at 5 s. Peak T is 309.804 K, within the authored small-contrast envelope.
The early dt0.01→0.005 s comparison through 0.4 s changes the maximum velocity
component by 0.1624%; this does not establish full temporal/spatial convergence.

Focused open-atmosphere, transactional and analytic convergence gates pass;
new batch controls cover consumption, replay, mixed retries, late rollback and
bounds. Retained evidence lives in GrowthSim's ignored
`build/fire-corner32-20261005/experiment-v2/`; the original rejection is retained
in `experiment-v1/`. GrowthSim `docs/fire_corner_plume_qualification.md` owns the
source/visual audit and model limitations. The VF3D render-only half-cell origin
mapping preserves physical payload bytes; authoritative cell-centred arrays and
physical origins remain unchanged. Body-free reservoirs, synthetic properties,
visual flame proxies and authored optical normalization are not production hot
fire-air physics or a solid-ground plume. Movie and 64³ qualification remain open.

## Grounded plume policy (Main Edit, 2026-10-05)

The additive `solid_bottom_open_top` vertical policy returns the separately named
`xy_periodic_ground_open_top_projection3d_v1` model. Pair it with
`predictor_velocity: no_slip_bottom_zero_gradient_top`. The existing two-open-end
policy and its defaults remain available. Ground has no reservoir: bottom
pressure datum must be zero, bottom ambient temperature must equal the reference,
and bottom ambient smoke must be zero. The top retains the existing open
pressure/reservoir policy. XY is still periodic; this is not a closed room.

The stationary bottom uses odd-reflected tangential ghosts (no-slip), exactly zero
normal face velocity, a homogeneous Neumann pressure-correction boundary, and
zero advective/diffusive heat and smoke flux. The pressure operator reuses the
Cartesian sparse multigrid path with a mixed-boundary extension. Scalars occupy
cell centres above the floor; a ground Fire source deposits into layer zero.
No clipping a previously projected normal velocity is used to conceal leakage.

Native and independent Python admission reject nonzero bottom normal velocity or
bottom cumulative flux. Ground hydrostatics, tangential wall dissipation, localized
buoyant transport/diffusion, exact restart, invalid state rejection, and late
failure rollback are covered by `tests/test_ground_atmosphere.py` and the native
sanitizer test. Run `make test-open-atmosphere-sanitize` and
`python3 -B -m unittest discover -s tests -p test_ground_atmosphere.py -v`.

The low-contrast Boussinesq envelope is unchanged. This floor addition does not
qualify flame-temperature air, thermal radiation, conjugate floor heat transfer,
room side walls, variable density, turbulence closure, or hot reacting combustion.

The coupled journal retains a default 128 MiB budget. A caller can explicitly
request `journal_budget_bytes` between 16 and 512 MiB; the selected budget is
bound into journal identity and cannot be changed on reopen. The grounded review
uses 256 MiB to preserve fine source events and selected complete 32-cubed native
checkpoints. Native request (64 MiB), event-count, one-second transaction and
substep limits remain independent gates. An internal prepared-mapping allocation
path reuses the validated source geometry per transaction; admitted source bytes,
config and producer binding are checked before consumption.
