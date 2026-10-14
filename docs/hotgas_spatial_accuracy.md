# Spatial reconstruction and independent buoyancy reference

This Main Edit checkpoint adds opt-in `domain.flux="hllc_muscl"` with
`solid_reservoir_gravity_conduction_muscl_rk2_v1` identity. It combines primitive
monotonized-central slopes, the existing local isothermal equilibrium mapping,
HLLC face fluxes and two-stage SSP Runge–Kutta time integration. Missing physical
neighbors flatten slopes, so the periodic smooth-wave order does not establish
second-order accuracy at every physical boundary or discontinuity. The prior
Rusanov default and first-order HLLC policies remain available. Changing model
policy refuses checkpoint recovery across identities.

Reconstruction and time integration live in separate native modules. Both stages
must pass positivity and the physical acoustic/diffusive CFL limit. The final
state is committed only after success; a second-stage failure preserves accepted
state and output-budget bytes. Face, conduction, gravity and potential-energy
budgets are averaged with the same RK2 weights. A constant source packet is
integrated once per accepted physical substep, not twice. No reduced sound speed,
low-Mach flux correction, pressure projection or positivity fallback is used.

## Independent reference and measured numerical error

The new reference solves a linear small-disturbance problem independently of the
native discretization: `v_t=-grad(pi)+b ez`, `pi_t=-a^2 div(v)`, with
`b=B cos(kx x) sin(kz z)`, `kx=2 pi/L`, `kz=pi/L`, `omega=a sqrt(kx^2+kz^2)`.
For initially zero velocity and pressure disturbance, the horizontal velocity
amplitude is `B kx kz/(kx^2+kz^2) (-t+sin(omega t)/omega)`, vertical amplitude is
`B/(kx^2+kz^2) (kx^2 t+kz^2 sin(omega t)/omega)`, and pressure-over-density
amplitude is `-B kz/(kx^2+kz^2) (1-cos(omega t))`. An independent direct RK4
integration of the three modal ODEs checks these coefficients to 1e-12.

Native comparisons use periodic horizontal directions, impermeable vertical
walls, a 0.1 m domain, 300 K hydrostatic background, density contrast 0.001,
0.5 microsecond substeps and 2 ms duration. Initial and analytic fields use
cell-average trigonometric factors. The frozen-force approximation has density
contrast 0.001, stratification 1.14e-5, displacement/domain 3.92e-7 and velocity
Mach scale 5.65e-8. This is a controlled linear-limit assessment; the full nonlinear
compressible evolution is not exactly this frozen-forcing solution.

At 32x4x32 cells, the first-order HLLC relative velocity L1 error is 52.43%,
compared with 8.67% for MUSCL/RK2. New velocity errors on 8, 16 and 32 cells per
resolved direction are 63.85%, 22.74% and 8.67%. Pressure-disturbance L1 errors
are still 150.43%, 68.11% and 24.15%. A separately defined pressure-balanced
initial condition gives 8.75% velocity and 28.50% pressure error at 32 cells.
Initial acoustic imbalance therefore does not account for all remaining error.
These percentages normalize tiny perturbation fields, not the atmospheric
absolute pressure; they must not be read as 24% error in 101325 Pa pressure.

In the retained 0.1 Pa periodic acoustic-wave test at 1 ms, normalized pressure
L1 errors on 16, 32 and 64 cells are 2.862%, 0.915% and 0.235%. Refinement ratios
are 3.13 and 3.89, supporting second-order behavior for this smooth periodic
case. First-order HLLC gives 21.988%, 12.105% and 6.320%.

The remaining low-Mach concern is consistent with the documented limitations of
Godunov HLL-family discretizations; higher spatial order alone is not an all-speed
qualification. [Primary low-Mach HLL analysis](https://onlinelibrary.wiley.com/doi/10.1155/2020/7451240).
No particular published low-Mach correction is claimed to have been implemented.

## Retained finite-contrast cases

The matched 900 K bubble at 2 ms has initial-hot-region mean vertical speeds
0.0129613, 0.0167725 and 0.0181530 m/s on 6x6x16, 12x12x32 and 18x18x48 cells.
Initial conserved arrays, geometry, boundaries and timesteps were read back as
exactly matching the prior first-order fixtures. Medium-to-fine relative change
is 7.60%, versus 14.97% previously; this is improved numerical sensitivity, not
agreement with a measured plume. Gas-plus-potential residuals are below 1.9e-11 J.

The original full-width slab and original initially-hot-region metric remain
unqualified. At 10 ms the new mean is -6.02910e-5 m/s, compared with the retained
first-order HLLC -2.95258e-6 m/s and Rusanov -1.64299e-4 m/s. Thus the new scheme
is worse by this slab metric than first-order HLLC; its cause has not been
independently established. Exact checkpoint recovery passes. This conflicting
finite-contrast behavior is a reason to keep the method opt-in and the physical
gate open, not to promote it as a calibrated buoyancy solver.

## Verification and retained evidence

Fresh strict and ASan/UBSan domain workers compile; the sanitized native harness
passes, including second-stage transactional refusal and source accounting.
Five spatial test methods pass strict. Sanitized coverage passes the reference,
contact/equilibrium/restart, acoustic and conservative boundary checks, with the
buoyancy refinement reduced to 8 and 16 cells. The attempted 32-cell sanitizer
case exceeded the existing 60-second worker timeout; its failure log is retained.
The full 32-cell accuracy result comes from the strict build. Sixteen retained
domain/contact/plane methods pass on each worker. Seven periodic and three held
wind-return regressions pass using retained workers; the periodic worker's source
hashes were freshly checked. Desktop, release-package, remote-worker, measured
flame, radiation and reciprocal thermal acceptance are outside this numerical gate.

Run from this source checkout with fresh build roots:

```sh
python3 scripts/build_hotgas.py --domain --output build/my-spatial/strict
python3 scripts/build_hotgas.py --domain --sanitized --output build/my-spatial/sanitized
python3 scripts/build_hotgas.py --domain --sanitized --harness --output build/my-spatial/harness
build/my-spatial/harness/native_test
HOTGAS_DOMAIN_WORKER="$PWD/build/my-spatial/strict/physics_sim_hotgas_domain_worker" \
  PYTHONDONTWRITEBYTECODE=1 python3 tests/test_hotgas_spatial_accuracy.py -v
HOTGAS_SPATIAL_SANITIZED=1 \
  HOTGAS_DOMAIN_WORKER="$PWD/build/my-spatial/sanitized/physics_sim_hotgas_domain_worker" \
  PYTHONDONTWRITEBYTECODE=1 python3 tests/test_hotgas_spatial_accuracy.py -v
```

`scripts/fire_spatial_accuracy_qualification.py` takes `--worker`, a fresh
`--output` and the retained prior-domain `--retained-slab` fixture. It saves
sealed matched first/second-order linear comparisons and the original finite
900 K bubble and full-width slab. The separate checkpoint records those physical
geometry results; no failed case is silently removed. Evidence is under
`_private_workspace_artifacts/fire-fluid-spatial-accuracy-20261009/`.

Next boundary: independently qualify low-Mach pressure/velocity treatment,
including steady vortex pressure balance, Mach scaling and acoustic preservation.
Finite-contrast plume accuracy remains open. The continuous native caloric EOS,
valid cool-to-hot species properties, transport, combustion/radiation and matched
surface/gauge response remain subsequent physical gates. Reciprocal thermal
feedback remains disabled. Physics minor release recommended; approval required
before version bump or release actions.

The subsequent [bounded pressure/velocity checkpoint](hotgas_pressure_accuracy.md)
qualifies an opt-in correction and compression-aware acoustic safeguard for the
selected stationary low-Mach and prepared linear-reference cases. Its limits and
retained finite-temperature geometry checks remain explicit.
