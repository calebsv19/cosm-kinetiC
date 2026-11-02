# Opt-in scalar transport candidate

The native open-atmosphere worker accepts one explicit experimental profile:

```json
"scalar_transport": "mc_ssprk2/v1"
```

It selects the existing monotonized-central (MC) reconstruction and SSPRK2
scalar integration independently of momentum reconstruction. The request's
`transport_scheme` continues to select momentum as before. Leaving
`scalar_transport` absent preserves the existing scalar and native checkpoint
shape. This is source development, with no default, installed-app, or release
activation.

MC scalar reconstruction uses the existing stricter 0.4 scalar CFL bound, even
when momentum uses upwind. No momentum CFL, pressure projection, source budget,
physical heat law, or acceptance tolerance changes. SSPRK2 reuses the projected
velocity over each scalar step; it does not make momentum or time-varying
coupling second order. Constant-in-substep source and boundary-flux stage weights
remain those of the native SSPRK2 operator.

The profile is bound in the request configuration digest and echoed in native
fields and state. Host acceptance checks that identity. Restart cannot add,
remove, or change the profile, and a worker that silently ignores the request
is refused when the native profile receipt is missing. Sparse qualification and
frozen-momentum diagnostic modes remain separate.

## Reference evidence

The independent compact-pulse reference uses exact finite-volume cell means of
a positive 125 mm pulse translated at 0.05 m/s for 3.2 seconds on zero background.
At 64 cells, relative field errors are:

| Scalar operator | Relative L2 error |
|---|---:|
| Default upwind | 64.45% |
| Previous minmod benchmark | 46.69% |
| MC + SSPRK2 | 34.33% |

The candidate improves error by 46.7% against actual upwind default and 26.5%
against minmod. Its 32/128-cell errors are 55.47/12.75%; halving the 64-cell
timestep leaves error at 34.33%. Smooth combined transport at 64 cells has
0.0964% perturbation-relative error. Optimized and ASan/UBSan profiles agree
exactly, including binary64 sign bits. Heat balance and nonnegativity hold.
The localized spatial error remains substantial; these results are not a general
physical or wake convergence claim.

Native controls cover both upwind and MUSCL momentum, MC/Euler and MC/SSPRK2,
constant/sharp/reversed fields, reservoir fluxes, obstacles, and rejection rollback.
Default worker data matches the retained worker; candidate split restarts and
sanitizer data match continuous optimized execution. A retained Fire pulse gust
A/B uses unchanged source, geometry, material properties, clocks and momentum.
Its current outcome is recorded in `current_truth.md`.

## Source and verification entrypoints

- `coupling/scalar_profile.py`: closed request/state/field profile validation.
- `physics_sim_scalar_profile_io.h`: native profile selection and restore/field IO.
- `cfd_open_atmosphere3d.c`: independent scalar MC reconstruction/CFL selection.
- `test_scalar_profile.py`, `test_scalar_profile_native.py`: identity, default
  parity, native restart, missing-feature refusal, source and work controls.
- `make test-scalar-candidate-native test-scalar-candidate-sanitize`: existing
  native bounds controls expanded to both momentum settings; use a fresh owned
  `BUILD_DIR` and preserve existing outputs.
- `fire_fluid_scalar_candidate.py`: one admitted fixed-input 64³ gust candidate.
  Fresh output is required. `--resume-initialized` is restricted to an explicitly
  bound zero-step journal after post-publication lifecycle failure, with the old
  producer terminal. It is not a general retry or partial-trajectory resume.

The object model is still the authored finite-capacity support exchange, not a
material wall-gradient/radiation model. A gust A/B measures changed behavior;
without a better-resolved candidate reference, changed object exposure or wake
heat must not be labeled more physically accurate.

## Matched larger-grid capacity

`capacity_profile: "scalar-wake80/v1"` admits only80³ in the1m cube withdt0.005,
`scalar_transport: "mc_ssprk2/v1"`, upwind momentum, quiescent exterior backflow,
ambient-open XY/solid-ground open-top boundaries, and the low stationary box
lower[.375,.375,.0625]m / upper[.625,.625,.3125]m. It preserves ordinary limits and
the separate `wake80/v1` profile. Capacity selection does not change the solver.
Current grid/JSON capacities do not admit128³. Native packetization is separately
bound in replay configuration; physical clocks and source delivery stay fixed.

The matched80³ reference explicitly requests these resource limits (bytes are
upper bounds, not allocations):

```json
{
  "max_cells": 512000,
  "scalar_work_cells": 1000000000,
  "numerical_bytes": 536870912,
  "cache_pressure_operator": true,
  "native_request_bytes": 268435456,
  "native_output_bytes": 268435456
}
```
