# Contact resolution, pressure response and bounded mixture reference

This Main Edit source checkpoint repairs an independently demonstrated numerical
mixing defect. It does not qualify a reacting plume or enable reciprocal thermal
exchange. The retained Rusanov mode remains the default; `domain.flux="hllc"`
explicitly selects the contact-resolving model with a distinct checkpoint identity.
Changing flux policy refuses recovery from another model's checkpoint.

## Numerical change

`cfd_hotgas_flux3d` implements Toro's HLLC star states with min/max Davis signal
estimates. The separate domain module composes this flux with the existing
hydrostatic reconstruction, conservative gravity and conduction. Acoustic CFL
admission is unchanged. Nonpositive/nonfinite star states refuse without a silent
fallback; accepted state and output budgets stay unchanged. Impermeable slip-wall
mass, tangential momentum and advective-energy fluxes are constrained exactly,
including star-state subtraction roundoff. Normal pressure force and conducted
heat remain in their ledgers. This is first-order compressible Euler, not an
all-speed method, viscous burner flow, or strong-shock qualification.
Method reference: [Toro's HLLC derivation, equations 37–39](https://www.prague-sum.com/download/2012/Toro_2-HLLC-RiemannSolver.pdf).

## Independently checked behavior

A stationary isobaric 300/900 K contact has exactly zero density change after
1 ms with HLLC. The matched retained acoustic Rusanov flux spreads it, with a
maximum density error of 0.298277 kg/m³. There is no physical conduction in this
case. This isolates numerical mixing from gravity and boundary effects.

A 0.1 Pa right-moving periodic acoustic eigenmode uses exact cell-average initial
values and an independent linear phase speed. At 1 ms its pressure L1 errors,
normalized by the 0.1 Pa amplitude, are 0.219878, 0.121047 and 0.063204 for
16, 32 and 64 cells along the propagation direction. Refinement ratios are
1.8165 and 1.9152. Rusanov has essentially the same acoustic-wave errors: HLLC
removes stationary-contact diffusion, not all acoustic diffusion.

A separate pulse checks boundary response. The reflected incoming-characteristic
L1 amplitude divided by the initial outgoing amplitude is approximately 0.999987
at a slip wall and 2.45e-7 at a reservoir. This only establishes the tested weak
pulse behavior; a reservoir remains a specified ghost state, without a general
nonlinear nonreflecting-outlet qualification.

## Matched buoyancy assessment

The same localized 900 K density perturbation in a 300 K atmosphere has mean
upward velocity 0.0247986 m/s at 10 ms with HLLC, compared with 1.37736e-5 m/s
with Rusanov. These share geometry, initial state, gravity and timestep; recovery
is exact for both. This large difference is evidence that the old field was
strongly affected by numerical mixing, not an experimentally calibrated speed.

At 2 ms, the same physical hot-region support gives:

| Grid | Mean velocity in initially hot region (m/s) |
| --- | ---: |
| 6×6×16 | 0.00835200 |
| 12×12×32 | 0.01117844 |
| 18×18×48 | 0.01285133 |

The medium-to-fine change is 14.97%, so physical buoyant accuracy remains
unqualified. At the medium mesh, halving the timestep changes velocity by
0.0107%; doubling height while retaining cell spacing changes it by 0.0223%.
Those small differences apply to this 2 ms test, not a long-domain validation.
Independent gas-plus-gravitational-potential residuals are below 2.1e-10 J.

The original full-width slab and original Eulerian-region metric are retained.
Its HLLC mean velocity still reverses, ending at -2.95258e-6 m/s at 10 ms,
compared with -1.64299e-4 m/s for Rusanov. Pressure/geometry effects are a plausible
remaining contributor, but the reversal's cause is not independently established.
It has not been reclassified as a passing physical-plume benchmark.

A matched prescribed-heat smoke applies 18,546.8 W for 10 ms with the earlier
measured-fuel power packet. HLLC reaches 746.579 K, compared with the earlier
Rusanov 305.768 K. Final circular-support conduction is +176.983 W at the
300 K flat wall and -35.1594 W across 13 mm; energy closes to 2.51e-11 J and
recovery is exact. These are transient heat-only transport results. No fuel,
chemistry, radiation, burner lip or gauge absorption is modeled; neither result
is a prediction of the measured flame or an identified burner loss.

## Mixture reference, not yet the native EOS

`coupling/gas_mixture.py` evaluates NIST Shomate Cp, piecewise-integrated sensible
enthalpy/internal-energy differences, Cv, R, frozen-composition gamma and sound
speed, and bounded temperature inversion. Four physical checks compare displayed NIST
Cp/H tables, independent midpoint quadrature and energy inversion, and refuse
unsupported species, composition and extrapolation. The selected data are curated
numerical transcriptions, not downloaded HTML-byte snapshots.
Sources: [nitrogen](https://webbook.nist.gov/cgi/cbook.cgi?ID=C7727379&Mask=1&Type=JANAFG&Table=on),
[oxygen](https://webbook.nist.gov/cgi/cbook.cgi?ID=C7782447&Mask=1&Type=JANAFG&Table=on),
[carbon dioxide](https://webbook.nist.gov/cgi/cbook.cgi?ID=C124389&Mask=1&Type=JANAFG&Table=on),
[water vapor](https://webbook.nist.gov/cgi/cbook.cgi?ID=C7732185&Mask=1&Type=JANAFG&Table=on).

The explicitly specified dry-air mixture is N2:O2=3.76:1, with no argon. Complete
stoichiometric methanol products are CO2:H2O:N2=1:2:5.64; this is a reference
composition, not measured evolving flame composition. Dry air is bounded to
298–1500 K; the products are bounded to 500–1500 K because the selected water
fit starts at 500 K. No extrapolation to the 300 K wall is permitted. At 1290 K,
dry-air Cp is 1195.73 J/(kg K), gamma 1.31755; specified-product Cp is
1424.48 J/(kg K), gamma 1.26881. The native solver still uses gamma=1.4, R=287.
No variable transport coefficients, chemical formation energy, condensation or
caloric-EOS integration is claimed.

## Verification and reproduction

Fresh strict and ASan/UBSan domain workers pass 16 solver/observation test methods
per build. The sanitized native harness checks byte-preserving refusals and
isolated moving contact/shear fluxes in all three axes and both directions.
Five mixture-reference methods pass after final molecular-mass readback.
Seven periodic regressions pass using a retained worker whose source hashes
were freshly verified; three held wind-return regressions pass. Desktop/package,
remote, combustion/radiation and physical experimental-acceptance gates were not
run: this is a standalone source numerical gate outside those surfaces.

From the Physics source checkout, use fresh output roots:

```sh
python3 scripts/build_hotgas.py --domain --output build/my-contact/strict
python3 scripts/build_hotgas.py --domain --sanitized --output build/my-contact/sanitized
python3 scripts/build_hotgas.py --domain --sanitized --harness --output build/my-contact/harness
HOTGAS_DOMAIN_WORKER="$PWD/build/my-contact/strict/physics_sim_hotgas_domain_worker" \
  PYTHONDONTWRITEBYTECODE=1 python3 tests/test_hotgas_contact.py -v
PYTHONDONTWRITEBYTECODE=1 python3 tests/test_gas_mixture.py -v
```

`scripts/fire_lowmach_qualification.py` accepts explicit `--worker`,
`--sanitized-worker`, `--harness`, `--retained-slab` and fresh `--output` paths.
The retained slab is the sealed prior-domain `diagnosis/slab.json` fixture.
Evidence: `_private_workspace_artifacts/fire-fluid-lowmach-20261009/`.
Its `mixture-reference-current.json` supersedes the initial evaluator snapshot
with the displayed NIST water molecular mass of 18.0153 g/mol and JSON-list
normalization before sealing. A fifth regression verifies serialized identity; numerical-flow
controls and results were unchanged by that narrow reference correction.

Next boundary: establish spatial/low-Mach pressure–velocity accuracy and an
independent buoyancy reference; connect a continuous caloric EOS with valid
cool-to-hot mixture data, species energy and independently specified transport.
Then qualify fuel/combustion, radiation and matched surface/gauge absorption
before reciprocal thermal feedback or authoring controls. Canonical branches,
VERSIONs, released packages and installed workers are unchanged. Physics minor
release recommended; approval required before version bump or release actions.
