# Local grounded plume transport candidate

`transport_scheme` is an optional open-atmosphere request field. Its default is
`upwind`; `muscl_minmod` enables limited piecewise-linear spatial reconstruction
for momentum advection and conservative interior energy/smoke fluxes. Boundary
scalar donors retain their original policy. Time integration remains forward
Euler, first order. MUSCL uses the tighter combined advection/diffusion CFL
bound 0.4; legacy upwind retains 0.8. There is no turbulence closure or combustion
model added by this option. Candidate negative scalars, divergence, conservation,
contrast and work-budget failures reject the step without accepting its fields.

The grounded policy retains exactly zero bottom normal velocity and scalar flux,
periodic XY and an open top. Temperature is bounded by the existing low-contrast
Boussinesq applicability gate. Grid refinement does not bypass that gate.

Explicit request `resource_limits` permits a contained larger experiment:

```json
{
  "max_cells": 262144,
  "scalar_work_cells": 400000000,
  "numerical_bytes": 268435456,
  "native_request_bytes": 134217728,
  "native_output_bytes": 134217728,
  "cache_pressure_operator": true
}
```

`max_cells` and `scalar_work_cells` are required when the object is present.
Defaults remain 32768 cells, 100 million scalar cell updates, 128 MiB numerical
allocation and 64 MiB native request/output. Optional numerical bytes are bounded
at 128–512 MiB; request/output bytes at 64–256 MiB. Cell and work opt-ins are capped
at 262144 and one billion respectively. The one-billion explicit work ceiling
supports the full eight-second 64³ / 0.0025-second qualification (838860800
scalar cell updates); the default remains 100 million. These limits are configuration-bound in
checkpoint identity. Numerical allocation is not total process RSS: JSON and
Python objects remain separate costs. Source bundles retain their 64 MiB policy.
The ordinary adapter's 120-second timeout, 256-step request bound and
accepted-state gates remain unchanged. A separately named local sparse
qualification mode has its own closed bounds; see
[plume_full_duration_qualification.md](plume_full_duration_qualification.md).

Pressure-operator reuse is opt-in and defaults off. It retains the same mixed
Poisson operator within a worker request; it never serializes a solver cache as
physical state. Ground-policy changes after building the cache are rejected.
Independent nonuniform-buoyancy tests require exact accepted native-state parity
with rebuilding and exact split-request restart parity. Memory stays charged to
the existing numerical allocation scope and is freed with the atmosphere.

The open-atmosphere adapter streams the existing canonical tagged-binary64 digest
instead of materializing a dictionary for every numeric element. Protocol bytes,
number validation and digests are unchanged. Reference equivalence tests cover
mixed/nested values, signed zero, Unicode, many numeric values and invalid
nonfinite or inexact integers. Strict native-output loading rejects duplicate
keys and nonfinite constants and applies the explicit artifact byte limit.

`CoupledOpenAtmosphere` accepts optional keyword `transport_scheme` and
`resource_limits`; legacy positional calls and their configuration digests remain
unchanged. Its journal budget and revision limits remain separate. The large
comparison uses sparse offline native snapshots, not a claim of transactional
journal acceptance for every source interval.

`tests/test_plume_transport.py` exercises translation, positivity/boundedness,
conservation, spatial/time refinement, exact restart, pressure-cache parity,
streamed digest identity and invalid resource/transport policies. Existing open,
grounded, coupled and native rollback/memory gates remain required.
