# Periodic evolving atmosphere: native checkpoint and joint publication

Implemented model: `periodic_evolving_passive3d_v1`. This first atmosphere-flow
milestone couples the existing arbitrary-initial-velocity periodic Navier–Stokes
solver to the qualified passive sensible-energy/smoke solver. It is body-free,
constant-property and fully periodic in XYZ. Heat and smoke do not yet affect
momentum. It does not claim an open atmosphere, combustion chemistry or buoyancy.

## Native stepping and rollback

`CfdAtmosphere3d` owns one `CfdPeriodic3d` and one `CfdPassive3d`. Each fixed step
constructs a private candidate with the complete accepted solver storage/history,
advances unforced momentum, then transports integrated scalar sources using the
new projected velocity. Both states move into the accepted owner only after both
numerical gates pass. A scalar failure after a successful momentum solve, a CFL
rejection or numerical allocation failure preserves the accepted arrays, BDF
history, clocks, counters, budgets and failure flags. Failed candidates are freed.
The scalar/momentum split is first order; the existing momentum discretization is
unchanged. No viscous/kinetic work is transferred into sensible thermal energy.

Momentum dt is immutable (1 microsecond through0.1s). Every public operation must
cover an integral number of those steps; variable-step BDF formulas are not
implied. Source boundaries may split a momentum interval: all covered source
fractions are conservatively accumulated before that scalar step. All portions
must already be admitted. Admission uses the integer momentum-step clock, with
machine-precision tolerance at matching source boundaries; native floating-clock
accumulation does not reject the next contiguous frame. Source-free steps cannot bypass an unfinished source.
The source allocator's clipped dual-area and cumulative-difference semantics remain
unchanged. Injection is volumetric cell deposition, not a solid-surface heat flux.

## Checkpoint and source journal

`evolving_atmosphere.py` admits
`physics_sim_evolving_atmosphere_request/v1` and returns
`physics_sim_evolving_atmosphere_experiment/v1`. Its sealed
`physics_sim_native_atmosphere_state/v1` carries exact configuration identity,
worker SHA256, the full27-array native flow storage (including previous velocity,
pressure and solver warm-start arrays), scalar fields, cumulative inputs, step
count/clock, scalar work count and kinetic diagnostic history. Operators and owned
pointers are reconstructed; no raw pointer dump is imported. Native restore checks
finite fields, counters, clocks, nonnegative scalars and accepted divergence.
Python independently checks checkpoint identity, chunk/lifetime scalar budgets,
field/state identities and SI temperature/concentration conversions.

Continuation executes only the new chunk from that state. It does not replay the
source's or receiver's whole earlier history. Split/restarted execution is tested
for exact equality of the native checkpoint and physical arrays against one
uninterrupted run under identical worker bytes and fixed dt. Configuration/worker
changes reject; upgrades require future explicit migration. The raw private worker
is not a public untrusted upload endpoint.

`coupled_atmosphere.py` publishes
`physics_sim_evolving_coupled_checkpoint/v1` in its own SQLite history. It reuses the
existing admission/DB conventions, source validation and allocator without editing
the prescribed-flow journal. Full frames remain retained. The native state, source
consumption ranges, physical result, new revision and idempotent operation receipt
publish in one FULL-synchronous transaction. Native failure, timeout or SQL
publication failure leaves accepted history untouched. Copy a closed DB with its
configuration and pinned bytes to restore consumption and flow together. A frozen
checkpoint JSON is provenance/readback; the source journal is required to resume
source consumption. Initialization also runs native admission before publishing.

Configuration keys: `policy`, `properties`, `initial_face_velocity_m_s`,
`momentum_dt_s`, `worker`. Properties retain the five passive SI quantities and add
positive `dynamic_viscosity_pa_s`. CLI:

```sh
make evolving-atmosphere-worker
python3 -B scripts/coupled_atmosphere.py --config CONFIG --store STORE init
python3 -B scripts/coupled_atmosphere.py --config CONFIG --store STORE admit SOURCE.json
python3 -B scripts/coupled_atmosphere.py --config CONFIG --store STORE step \
  --revision 0 --operation-id first --dt .02
python3 -B scripts/coupled_atmosphere.py --config CONFIG --store STORE inspect
```

Resource envelope:4..64 nodes per axis, at most32768 cells,256 new native steps per
request,10000 lifetime momentum steps,64 journal revisions,256 source events,
64MiB native request,128MiB numerical allocation budget and conservative SQLite
admission estimate,100-million cumulative scalar subcycle-cells and120s worker
execution timeout. Candidate cloning/reconstructed operators add setup/memory cost;
this bounded implementation establishes correctness, not production throughput.

## Proof and scene integration

`make test-evolving-atmosphere test-coupled-atmosphere
test-evolving-atmosphere-sanitize` covers momentum parity against the unchanged
solver, independent discrete viscous shear decay, exact split native restart,
uniform/zero-source cases, bad checkpoint/config/divergence rejection, whole-pair
rollback and retry, allocation failure, source gaps, integer-step admission clocks, within-step source boundary splitting, fixed-dt admission, copied DB
restore, equal retries and failed SQLite publication. Four numerical Python tests,
five journal tests and native ASAN/UBSAN controls pass.

GrowthSim's `run_coupled_scene.py --evolving` consumes two real native Fire bundles
with a synthetic divergence-free shear plus through-flow initial condition and
synthetic fluid properties. Ten0.01s momentum steps are published through four
reopened/retried journal operations. This is a laminar coupling diagnostic, not a
validated real-fire plume. It preserves89.624811988kJ and0.01194997493kg smoke.
The combined scene adapter now maps evolved face velocity to cell centers and
exports solved pressure Pa into VF3D. Temperature and physical budgets stay in the
SI sidecar; authored optical density normalization is unchanged. Native RayTracing
preflight/render checks the same surface/volume attachment.

## Next numerical boundaries

1. Specify a separate atmosphere boundary policy: ambient inflow/backflow scalar
   values, pressure/traction, normal velocity constraints and outflow. A likely
   first bounded case is horizontal periodicity with a ground boundary and open
   top. Do not repurpose tunnel Y/Z no-slip or manufactured X traction silently.
2. Implement/qualify open momentum and scalar boundary fluxes together. Explicit
   per-face extensive inflow/outflow receipts must close energy and smoke budgets.
   Independent uniform, pulse exit, zero-source, backflow and refinement controls
   precede source-driven runs. Checkpoint identity includes the boundary policy.
3. Add opt-in Boussinesq buoyancy only within a declared small-temperature-contrast
   envelope, face-interpolated acceleration and pressure/hydrostatic conventions.
   Qualify zero-feedback parity, uniform thermal control, hydrostatic balance,
   directional thermal response and time/grid refinement. Large fire-temperature
   contrasts need a different admitted model, not clipping or hidden rescaling.
4. Requalify native restart, joint rollback and paired scene clocks for each model;
   only then expose GUI/MCP controls and optimize candidate/operator reuse.

Reuse: app Cartesian/memory/periodic momentum/passive components adopted; shared
core_space/core_units scene/SI meanings retained. New scheduler/time service,
core_pack/core_memdb storage and generic shared thermal abstraction deferred:
this is a synchronous bounded app-owned solver with existing SQLite publication.
No shared APIs, library versions, package/release pointers or RayTracing sources
change in this slice.
