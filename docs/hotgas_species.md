# Conservative evolving species and formation-energy checkpoint

The source-only native mixture worker now evolves N2/O2/CO2/H2O/CH3OH partial
densities, momentum and total gas energy in a **periodic 3D domain**. This is a
separate bounded transport kernel, not a replacement for the retained solid/open,
gravity domain worker or an enabled Fire thermal return. First-order HLLC
advection shares one mass/contact flux across species. Prescribed equal-D Fick
diffusion and Fourier conduction are included with conservative energy transport.

## Thermodynamic and source contract

Gas properties are supported over 200–1500 K. The four earlier species reuse the
[continuous caloric model](hotgas_caloric.md). Methanol uses the published Cp
coefficients in [Cantera's GRI30 data](https://raw.githubusercontent.com/Cantera/cantera/main/data/gri30.yaml).
The rounded fit mismatch at 1000 K is matched deterministically, then Cp is
integrated continuously. This is not empirical calibration.

Species total enthalpy includes its GRI NASA7 formation reference at 298.15 K;
N2/O2 elemental standard-state formation enthalpy is exactly zero. Total energy
is `rho * sum(Y_i * (h_i(T) - R_i*T)) + rho*|v|^2/2`, including formation energy.
The previous frozen-mixture solver retains its sensible-only reference. These
energy arrays and checkpoint schemas are not interchangeable.

Species order is N2, O2, CO2, H2O, CH3OH. Molar masses derive consistently from
N=14.0067, O=15.9994, C=12.0107, H=1.00794 g/mol. This uses water 18.01528 g/mol,
rather than the rounded 18.0153 in the earlier four-species model. Atomic-consistent
masses close `CH3OH + 1.5 O2 -> CO2 + 2 H2O` without a fictitious mass source.
The reused water coefficients are scaled onto this mass basis.

Each cell stores rho, three momenta, energy and five partial densities. Species
must be nonnegative and sum to rho within relative 1e-11 numerical admission;
there is no clipping or renormalization. Temperature and pressure must be valid.
Source packets are integrated increments per substep and include all ten fields.
Fuel injection adds density, fuel partial density and formation+sensible internal
energy consistently. A prescribed species conversion with formation in energy
must not add the same chemical heat a second time.

Checkpoint identity binds worker bytes, species/energy model, adapter/property
recipes, config, source and rational tick rate. Source/config drift, malformed
checkpoints and unsupported states refuse; failed native steps preserve accepted
state. Source-owned element ledgers record C/H/O/N amounts in mol, alongside
mass, momentum, species and total-energy ledgers.

## Supported transport closure

The present coefficients D (m2/s) and k (W/(m K)) are prescribed, nonnegative
constants. At a face, raw species diffusion is `J_i=-rho_face*D*grad(Y_i)`.
A barycentric correction removes its rounded total; the largest present species
closes the sum to zero. Energy flux is `-k*grad(T) + sum(h_i(T_face)*J_i)`, with
full formation+sensible enthalpy. Faces use the arithmetic mean temperature and
density. Equal/opposite transfers preserve all transported quantities globally.

[Cantera's governing-equation reference](https://cantera.org/stable/reference/onedim/governing-equations.html)
describes zero-sum diffusion correction and species enthalpy transport. This
implementation uses a simpler equal-D, mass-gradient closure: it is **not**
Cantera's species-dependent mixture-averaged or multicomponent transport model.
No viscosity, Soret/Dufour, pressure diffusion, calibrated D/k or phase change is
included. Admission uses a combined physical acoustic and prescribed diffusive
Courant screen, then validates every candidate; refusal does not prove a general
positivity theorem or stability for arbitrary composition gradients.

Native modules separate mixture thermodynamics, face flux and periodic stepping.
The HLLC star flux is evaluated directly to preserve the sign of near-zero contact
mass flux; subtracting nearly equal star/physical densities had caused a trace
species refusal in the stationary-contact test. No species floor hides it.

## Bounded qualification

Fresh strict and ASan/UBSan workers plus a sanitized native harness pass.
Six Python methods pass on both workers: independent property/formation data,
zero-sum diffusion and energy-flux oracles, diffusion increment refinement,
evolving species/energy transport, stationary/moving contacts, and prescribed
conversion/injection/exact recovery/refusal. Strict advection uses 16/32/64
meshes; sanitizer advection uses 32. Both use all three diffusion meshes and both
contact cases. macOS leak detection is disabled; ASan/UBSan remain enabled.

Independent [NIST gas methanol Cp values](https://webbook.nist.gov/cgi/cbook.cgi?ID=C67561&Mask=1)
agree within **0.4997%** over the 15 selected temperatures from 200 to 1500 K.
The GRI-anchor gas-water reaction enthalpy is **-676.218 kJ/mol**, compared with
**-675.686 kJ/mol** from the explicitly selected independent NIST formation
values (-393.5224 CO2, -241.8264 H2O, -201.49 CH3OH kJ/mol). The methanol value
is NIST's simple calorimetry-derived formation estimate, not an uncertainty-free
preferred standard. The difference is 0.0787%; no coefficients were fitted to it.
NIST's -763.68 kJ/mol combustion row is not conflated with gas-water products.

| Smooth five-species advection, 1 ms, 10 m/s | 16 | 32 | 64 |
| --- | ---: | ---: | ---: |
| Normalized fuel-fraction L1 error | 0.7628% | 0.3900% | 0.1959% |
| Energy-disturbance L1 error | 1.1982% | 0.6127% | 0.3077% |
| Maximum pressure error (Pa) | 0.00732 | 0.00371 | 0.00200 |

This is first-order spatial transport; no second-order mixture reconstruction is
claimed. Energy error is normalized by the initial energy disturbance, not by
its large formation-energy background.

The one-step uniform-rho/p sinusoidal Fick increment errors are
1.2783% / 0.32067% / 0.080096%, with refinement ratios 3.99/4.00. These compare
one full Euler step with continuum sine decay; they do not hold rho fixed during
a sustained compressible diffusion experiment. The independent face oracle
checks enthalpy transport, zero species sum, Fourier flux and orientation.

A uniform, prescribed 10% stoichiometric methanol conversion at fixed volume
raises T from 500 K to **802.570616 K** with **zero energy source**, conserving
mass, total energy and all four element ledgers. A separate reference energy
root agrees. This is chemical accounting, not a rate/ignition/flame prediction.
Exact nontrivial recovery with diffusion/conduction also passes.

The stationary fuel-air/product contact has maximum pressure error
**1.60e-10 Pa**. The same sharp contact moving at 10 m/s has **178.02 Pa** maximum
pressure error after 1 ms despite conservation. Conservative mixing of distinct
caloric properties does not guarantee exact isobaric moving-contact behavior.
This assessment is retained as an unresolved mixing-accuracy gate, not a passing
physical flame or sharp-interface claim. The previous slab reversal remains
unresolved; a periodic mixture test does not repair that domain result.

## Reproduction and next boundary

Use fresh output roots:

```sh
python3 scripts/build_mixture.py --output <fresh-strict>
python3 scripts/build_mixture.py --output <fresh-harness> --sanitized --harness
ASAN_OPTIONS=detect_leaks=0 <fresh-harness>/native_test > <probe.json>
MIXTURE_WORKER=<fresh-strict>/physics_sim_mixture_worker \
MIXTURE_NATIVE_PROBE=<probe.json> MIXTURE_TEST_OUTPUT=<fresh-results> \
python3 tests/test_mixture.py -v
```

The CLI takes grid XYZ, length XYZ, dt, steps, D and k, then ten conserved fields
per cell and an equally sized integrated source array on stdin. It emits a JSON
conserved array. `properties` and `flux axis D k spacing` are bounded probe modes.
The Python adapter validates and independently reads back every native state.

Next: species-aware wall/open-reservoir/gravity/source and observation integration,
sharp moving-contact pressure accuracy, and independently qualified temperature-
/composition-dependent transport. Then select a combustion mechanism supporting
intermediate species and its validated temperature range, qualify radiation and
surface/gauge response independently, and finally assess reciprocal exchange.
Five end-state species alone are not a validated methanol kinetic mechanism.
Combustion, radiation, condensation, measured burner prediction and thermal
return remain disabled/unqualified. No release or installed-runtime claim.
