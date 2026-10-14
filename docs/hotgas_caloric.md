# Native continuous caloric properties and fixed-mixture energy transport

The source-only domain worker has an opt-in, frozen-composition ideal-mixture EOS
for N2/O2/CO2/H2O over **200–1500 K**. It is connected to native pressure and sound
speed, primitive reconstruction, HLLC/low-Mach fluxes, RK stage copies, reservoirs,
conductive stability admission and experimental-plane temperature/enthalpy
observations. The constant-gamma modes remain available with their prior meanings.
Reciprocal thermal return remains disabled.

## Property model and energy convention

Nitrogen uses published [NIST Shomate bands](https://webbook.nist.gov/cgi/cbook.cgi?ID=C7727379&Mask=1&Type=JANAFG&Table=on).
Oxygen, carbon dioxide and water vapor use the Cp terms from the published
[GRI-Mech NASA7 data distributed by Cantera](https://raw.githubusercontent.com/Cantera/cantera/main/data/gri30.yaml).
The older NIST water fit starts at 500 K; it is retained as an independent overlap
reference, not extrapolated to ambient temperature. The common 200–1500 K bound
stays within the published inputs.

Coefficients are combined on a mass basis from specified mole amounts. At each
500/1000 K fit join, the upper-band constant is corrected by the rounded Cp
residual against the preceding band. This deterministic matching is below
0.003 J/(kg K) for the supported pure gases and does not fit experimental results.
Cp is continuous; integrating it across the joins gives continuous h/e and
monotone temperature recovery. Native and Python reference implementations are
separate, and the native property harness independently emits 66 sample rows.

Sensible enthalpy is referenced to `h(298.15 K)=0`:

- `e(T)=h(T)-R*T`;
- `Cv(T)=Cp(T)-R`;
- `p=rho*R*T`;
- frozen-composition sound speed squared is `Cp/Cv*R*T`.

Total conserved gas energy is `rho*e(T)+rho*|v|^2/2`; it can be negative because
of the declared reference. Positivity admission checks density, temperature,
pressure and Cv, not an arbitrary absolute energy sign. Chemical formation energy
is excluded. Reaction heat cannot be added consistently until the species and
formation-energy contract is implemented and verified.

## Configuration and source identity

Add `caloric_moles: [N2, O2, CO2, H2O]` to the gas configuration. Positive total
amount with finite nonnegative entries is required; only this fixed composition
is supported. Reference gamma/R metadata must match the mixture at 298.15 K;
use `coupling.caloric.gas_config` to construct it. Native sound speed and energy
use temperature-dependent properties rather than this reference gamma.

The native domain protocol takes the flux argument followed by `caloric`, then
four mole amounts before the ordinary domain input. The periodic-only Python
adapter refuses this EOS instead of silently invoking its old protocol. Domain
checkpoint identity binds the model, composition, caloric/reference recipes,
worker bytes, source packet and clock. Changing the EOS or composition refuses
recovery. Existing energy arrays must be converted explicitly; they cannot be
reinterpreted under the new reference.

Native caloric implementation is in `cfd_hotgas_caloric.c`; solver routing remains
in the existing narrow domain/reconstruction/RK modules. Shared Core Sim timing
and exchange ownership is unchanged.

## Qualification results

Prescribed closed-volume heat uses independently integrated NIST sensible energy:

| Mixture / range | Target | Native result |
| --- | ---: | ---: |
| Specified N2/O2 air, 300 K heating | 1300 K | 1300.0026 K |
| Specified N2/CO2/H2O products, 500 K heating | 1300 K | 1299.7763 K |
| Products, 1300 K cooling with the declared caloric energy | 300 K | 300 K |

These are caloric benchmarks, not independently measured flame predictions.
Native Cp checks agree with the independent NIST overlap within the 0.5% gate;
quadrature verifies integrated Cv, and join/temperature recovery checks pass.

A smooth periodic entropy wave transports the fixed product mixture at 900 K,
10 m/s for 1 ms. Normalized density-disturbance errors on 16/32/64 cells are
1.3764%/0.3111%/0.07768%, with refinement ratios 4.42 and 4.00. Finest spurious
pressure is 0.000118 Pa. Stationary 300/1300 K isobaric contacts, hydrostatic rest,
known reservoir enthalpy, prescribed wall conduction, transactional refusal and
exact restart are also checked. Heat-flux observations distinguish sensible
enthalpy plus kinetic transport from absorbed surface heat; radiation is absent.

A 900 K product-gas acoustic wave runs for 1 ms with a 0.1 Pa pressure
disturbance. Pressure errors on 16/32/64 cells are 4.1013%/1.3747%/0.4071%
against the declared caloric sound speed, with refinement ratios 2.98/3.38.
The finest error against the independently evaluated NIST sound speed is
0.4119%. The declared/reference speeds are 593.4708/593.4568 m/s. This checks
wave propagation and the frozen-composition sound-speed closure, not combustion.

Fresh strict and ASan/UBSan domain builds pass the five caloric test methods and
native property/domain harnesses. The acoustic method passes all three meshes
on the strict build and the finest mesh on the sanitized build. Sixteen prior
domain/contact/plane methods pass on each build; eight prior low-Mach pressure
methods and seven periodic methods also pass. macOS LeakSanitizer is unavailable;
the sanitizer runs use address/undefined checks with leak detection disabled.

The retained 900 K air parcel at 2 ms has selected upward velocities
0.0202662/0.0204793 m/s on medium/fine meshes, a 1.04% difference relative to
the fine result. The original slab still reverses at 10 ms:
`-6.66562e-5 m/s`. Its exact nontrivial checkpoint restart passes. Conservation
residuals are at most 3.64e-12 J for the parcel and 6.82e-13 J for the slab.
These are geometry assessments, not independently qualified finite-plume results.

For these EOS comparisons, the original temperature support and velocity are
retained, but hydrostatic pressure is renewed with mixture R and density/energy
are rebuilt consistently. Old conserved arrays are not reinterpreted. The fine
parcel exceeded the native 60-second call bound in 250-step batches; checked
50-step batches completed with unchanged total duration and timestep. The
sealed assessment leaves physical plume accuracy and thermal exchange false.

Build into a fresh root using `scripts/build_hotgas.py --domain`. The
`--caloric-harness --sanitized` variant checks native property inversion and
source/refusal semantics. `tests/test_hotgas_caloric.py` runs the independent
property, heating, contact/transport, boundary and recovery tests.
`scripts/fire_caloric_qualification.py` pins the recipes/worker and records matched
thermal geometry checks alongside these references. Evidence belongs to the
existing private coupling lane, not a release or installed-runtime claim.

## Physical limits and continuation

Composition is fixed across space and time. There is no independent evolving
species transport, species diffusion, fuel chemistry, formation-energy tracking,
combustion, radiation or condensation. Water is a prescribed ideal vapor;
continuing its gas-phase fit to ambient temperature does not qualify a wet
product mixture's equilibrium phase behavior. Conductivity is still prescribed,
not a temperature-dependent calibrated mixture transport coefficient.

The next physical boundary is conservative evolving species and formation energy,
with supported methanol fuel/air/product states and qualified transport. Then
combustion, radiation and surface absorption/gauge response need separate
independent references before reciprocal thermal exchange can be admitted.

The subsequent [evolving-species checkpoint](hotgas_species.md) adds a separate
periodic mixture kernel with formation energy and conservative prescribed
species/enthalpy diffusion. It does not change this frozen domain worker's
sensible-only energy meaning or qualify reacting open-domain transport.
