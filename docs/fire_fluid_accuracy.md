# Fire–Fluid source and transport accuracy checkpoint

2026-10-10, persistent Main Edit development. The original **common 8³
observation gate passes at 64³** for both the prescribed-source no-box and inert
box controls. This is a bounded low-temperature Boussinesq numerical baseline.
**Local surface heat transfer and burning-object admission remain unqualified.**

## Independent numerical ingredients

Native tests separate each ingredient from the plume:

- A fixed 0.125 m heating layer, with transport disabled, produces the exact
  SI heat and smoke amount and a 1 K temperature rise. Across 8/16/32/64 vertical
  cells, maximum heat error is below 3.2e-14 J.
- The same horizontally uniform layer with buoyancy stays hydrostatic. Pressure
  agrees with independent integration of the piecewise constant buoyancy force;
  velocity stays below 1e-10 m/s. Uniform heating with open vertical boundaries
  also reproduces the analytic acceleration, including the expected Euler lag.
- Uniform-heating velocity error halves for dt 0.01/0.005/0.0025 s. This verifies
  first-order time integration; it does not make the method second order.
- Smooth periodic cell-average translation, at fixed dt 0.0005 s, improves from
  3.20% to 1.44% to 0.470% L1 error across 16/32/64 cells. Heat and smoke agree.
- Fourier diffusion improves from 0.0524% to 0.0128% to 0.00291%. Its separate
  fixed-grid time study exposes temporal/spatial cancellation; a smaller time
  step alone need not monotonically reduce total error against the continuum.

Native and ASan/UBSan runs pass. The earlier 16³→32³ failed plume is preserved.

## Separate time and grid comparisons

All plume probes replay the exact same 32 accepted Fire frames. Source patch,
per-node heat/smoke amounts, 0.125 m support, 10% synthetic transfer, domain,
properties, 3.2 s duration and the 330 K applicability ceiling are unchanged.
No Fire worker evolves during these prescribed-source refinement probes.
The accepted source frames originally came from the live 8³ wind-feedback
control; this is **not a live 64³ coupled-feedback qualification**.

At fixed 32³, the full native scalar-field differences are:

| Fluid dt pair | Heat L1 | Smoke L1 |
| --- | --- | --- |
| 0.005→0.0025 s | 0.190% | 0.131% |
| 0.0025→0.00125 s | 0.0942% | 0.0651% |

Both ratios are approximately 0.497. The declared 1% temporal threshold passes.
Temporal error is much smaller than the previous spatial discrepancy.

Grid refinement keeps dt 0.005 s and restricts extensive fields conservatively
to the **same original 8³ observation partition**. The unchanged 10% gate is:

| Case | 16³→32³ heat / smoke | 32³→64³ heat / smoke | Coarse observation gate |
| --- | --- | --- | --- |
| No box | 22.11% / 16.64% | **9.14% / 7.44%** | Passed |
| Inert box | 22.30% / 16.66% | **9.19% / 7.47%** | Passed |

Fixed-world wind-probe differences also decrease. No-box L2 differences are
0.006020→0.002493 m/s; box differences are 0.005621→0.002247 m/s. Blocked
normal faces and solid heat/smoke remain exactly zero. Fine final accounting
residuals are below 9e-12 J and 4e-17 kg; final peak temperature is below
311.14 K. Native pressure/divergence criteria remain unchanged.

## Deliberate limit of the passing gate

On the **finer 32³ partition**, 32³→64³ differences are 16.81% heat / 9.95%
smoke without the box and 16.71% / 9.89% with it. The coarse gate passing does
not establish accurate fine thermal structure, point temperatures or surface
heat flux. The live 8³ authoring controls and existing 32³ previews retain their
previous accuracy-unqualified labels. No new flame/particle geometry was added.

Burning surfaces still require local heat-profile/flux convergence and an
explicit qualified surface-response boundary. No physical calibration,
reciprocal thermal feedback, material ignition or burning-object enablement
is claimed by this checkpoint.

## Implementation and verification

The unboxed lateral case now reuses the existing cached Cartesian multigrid
pressure solver. Masked box pressure uses a cached Galerkin preconditioner and
CSR iteration matrix. Acceptance still recomputes the independent matrix-free
true residual. Cached versus matrix-free 32³ box results agree within
4.5e-16 J, 2.7e-22 kg and 1.7e-16 m/s. No equation or physical tolerance changed.

The native probe avoids duplicating large JSON source requests for every
refinement batch. It executes the production solver with bounded local binary
SI arrays; it is not a package/worker protocol. The ordinary source-built
worker independently admits and returns a two-step 64³ box state.
Numerical allocation peaks at 162,542,448 bytes (155.01 MiB). Fine profiles
therefore explicitly select the existing 512 MiB numerical and 256 MiB request/
output envelopes; these are resource limits, not relaxed physics checks.

Verification order: fresh native/worker and full application compile; targeted
native and sanitizer tests plus 29 Python tests; broad `test-stable` passes.
Canonical remains unchanged. No commit, VERSION edit, package or publication
occurred. Patch release recommended for this solver/cache slice; approval is
required before a version bump. The broader unadopted coupling work has its own
release assessment.

## Reproduction

```sh
make BUILD_DIR=build/<fresh-accuracy> open-accuracy-probe open-atmosphere-worker \
  test-open-accuracy-native test-open-accuracy-sanitize \
  test-open-atmosphere-native test-open-atmosphere-sanitize \
  test-open-lateral-native test-open-lateral-sanitize
python3 -B scripts/fire_fluid_accuracy.py --frames <accepted-32-frames.json> \
  --probe build/<fresh-accuracy>/open-atmosphere/accuracy-probe \
  --grid 64 --dt .005 --output <fresh-no-box-case>
# Add --box for the matched inert box. Time refinement uses fixed --grid 32.
```

Use `fire_fluid_accuracy_report.py` against the completed case layout and prior
isolated checkpoint. Read the explicit `coarse_observation_gate_passed` status
and finer-partition limits; successful process exit alone is not qualification.
