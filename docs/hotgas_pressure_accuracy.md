# Bounded low-Mach pressure and velocity checkpoint

The source-only Main Edit domain worker supports opt-in `lhllc_muscl` alongside
Rusanov, first-order HLLC and MUSCL/HLLC. This adds a pressure correction and
SSP-RK3; it retains the physical acoustic timestep limit and constant gamma/R EOS.
Reciprocal thermal exchange remains disabled.

## Method and boundary policy

The hydrodynamic pressure correction follows equations 15/16 of
[Minoshima and Miyoshi (2021)](https://arxiv.org/pdf/2108.04991):
`chi=min(1,max(|vL|,|vR|)/c)` and `phi=chi*(2-chi)` reduce normal momentum
pressure dissipation. Physical signal speeds and contact velocity stay unchanged.
The corrected star flux is evaluated directly, including energy flux; the
ordinary Rankine-Hugoniot flux expression would restore uncorrected dissipation.
Primitive MC reconstruction and three-stage SSP-RK3 remain separate native modules.
Stage-weighted boundary/gravity/conduction budgets use weights 1/6, 1/6, 2/3;
source increments integrate exactly once. Failed stages leave accepted state and
output budgets unchanged. No positivity fallback silently changes the method.

An unguarded candidate reflected 86.02% of a weak outgoing pulse at a reservoir.
It is rejected. Physical wall/reservoir faces now use ordinary HLLC; mode-specific
wall slopes reflect normal velocity. An interior acoustic-impedance guard restores
pressure damping for compressional jumps while a Ducros anti-vorticity ratio
suppresses that guard in rotating flow. The ratio uses divergence squared over
divergence squared plus curl magnitude squared, as described in equation 11 of
[Fernandez et al., A physics-based shock capturing method](https://www.mit.edu/~cuongng/project/les4/les4.pdf).
The implementation scales before squaring and returns zero for a constant field;
there is no dimensional epsilon or tuned threshold. This combined guard is an
application-specific extension, not a qualification of the complete published
scheme or of multidimensional shocks.

## Independent numerical references

The stationary Gresho variant uses exact radial Euler force balance, constant
density, peak velocity 1, gamma 1.4, zero translation and four-point product
cell-average quadrature. It differs from the translated published benchmark.
Changing the background pressure changes Mach while preserving geometry, velocity
and the specified duration. The pressure metric removes the spatial mean and
normalizes by the initial pressure disturbance, not by the large background.
The independent radial derivative is checked against `v_phi^2/r`.

At 0.02 seconds on 16x16x4 cells:

| Method / Mach | Velocity L1 error | Pressure-shape L1 error | Kinetic energy retained |
| --- | ---: | ---: | ---: |
| MUSCL/HLLC / 0.001 | 71.26% | 107.31% | 54.46% |
| Corrected guarded / 0.001 | 5.57% | 8.44% | 99.12% |
| Corrected guarded / 0.01 | 5.46% | 9.78% | 99.16% |
| Corrected guarded / 0.1 | 3.33% | 21.68% | 99.24% |
| Corrected guarded / 0.01, 32x32x4 | 1.64% | 7.04% | 99.84% |

The selected low-Mach gate requires velocity below 10%, pressure-shape below 15%,
kinetic energy above 98%, and velocity error changing less than 20% between Mach
0.01 and 0.001. Mach 0.1 is an assessment row, not a passing pressure gate.
At 0.1 seconds, Mach 0.001 has 6.79% velocity error, 10.65% pressure-shape
error and 96.79% kinetic energy retained. This longer bounded run shows continuing
dissipation and is not an all-time accuracy claim.

Mesh refinement improves the selected velocity and pressure metrics but does not
establish a universal convergence order for this piecewise-smooth vortex.

The separate small-disturbance, pressure-prepared buoyancy reference at 32 cells
has 1.36% velocity and 1.14% pressure-disturbance error, versus the retained
MUSCL/HLLC values 8.75% and 28.50%. This linear reference is distinct from the
finite 900 K parcel and original slab. The unprepared 32-cell case has 1.37%
velocity and 12.88% pressure-disturbance error, so preparation materially changes
what pressure accuracy is supported.

The smooth 0.1 Pa periodic acoustic errors at 1 ms are 2.8621%, 0.9142% and
0.2453% on 16/32/64 cells, with refinement ratios 3.13 and 3.73. The correction
therefore preserves the retained near-second-order acoustic control.

The retained weak 0.1 Pa pulse has incoming-characteristic ratios 0.2288% periodic,
0.2275% reservoir and 100.9784% slip wall. The first two are below the 1% spurious
reflection gate; the wall is within the expected 95–105% range. These results do
not establish nonlinear nonreflecting boundaries.

## Retained finite-temperature geometry checks

With exactly the retained initial arrays and timesteps, the 900 K parcel at
2 ms has selected upward velocities 0.019819 and 0.020099 m/s on medium/fine
meshes, a 1.39% difference (previous MUSCL/HLLC 7.60%, first-order HLLC 14.97%).
This is improved mesh sensitivity, not an independent physical plume reference.
The original slab still reverses at 10 ms: -8.13679e-5 m/s, versus the retained
MUSCL/HLLC -6.02910e-5 and first-order HLLC -2.95258e-6. Its response worsens by
that metric. Conservation and exact nontrivial restart pass; they do not resolve
the physical discrepancy or justify enabling reciprocal thermal exchange.

## Reproduction and limits

Build into a fresh task-owned root with `scripts/build_hotgas.py --domain`.
`tests/test_hotgas_lowmach_pressure.py` checks numerical references, acoustic
refinement, deterministic broadband noise, contacts, equilibrium, restart identity
and RK3 physical ledgers. The sanitized native harness independently checks
contact/shear flux, quadratic pressure correction, rotation/compression sensor
oracles, stage refusal and source accounting. LeakSanitizer is unsupported on the
Mac host; AddressSanitizer and UndefinedBehaviorSanitizer are used. Eight
pressure methods pass on strict and sanitized workers; sixteen existing domain,
contact and plane methods pass per worker. The earlier five spatial methods and
unchanged periodic seven/held-return three regressions also pass in this workstream.

`scripts/fire_pressure_qualification.py` takes `--worker`, a fresh `--output` and
`--retained-root`. It pins recipes and worker bytes, records longer/refined vortex,
prepared/unprepared buoyancy, acoustic/pulse/noise controls, matched finite-parcel
meshes and the original slab with exact restart. Resume requires unchanged full
controls and checked checkpoint identities. Source-build receipts pin all native
inputs. Evidence lives in the existing private coupling artifact lane.

This numerical checkpoint does not qualify native temperature-dependent caloric
properties, species energy/transport, combustion, radiation, finite-contrast plume
accuracy or measured burner predictions. The next physical boundary is continuous
native caloric EOS with supported cool-to-hot mixture data and energy/transport,
retaining finite-contrast and longer-time numerical controls before thermal return.

The subsequent [native caloric checkpoint](hotgas_caloric.md) connects bounded
continuous heat capacity and specified frozen-mixture sensible energy to the
domain worker. Evolving species, formation energy and transport calibration remain
explicit prerequisites for the following combustion/radiation/surface gates.
