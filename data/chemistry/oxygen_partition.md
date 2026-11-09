# Oxygen gas/water partition reference data

`oxygen_partition_v1.json` records a selected reference from Rolf Sander's2023
compilation, https://www.henrys-law.org/henry/casrn/7782-44-7 , note8. It supplies
a fitted Henry solubility coefficient based on Rettich/Battino/Wilhelm2000,
DOI10.1006/jcht.1999.0581. Full original experimental records are not bundled or
claimed reanalyzed. Coefficients are attributed to the maintained compilation;
this numeric transcription is not a new measurement.

At298.15K, Hcp is1.256219475e-5mol/m3/Pa. Its fitted inverse-temperature derivative
is1451.777809K. The reference adapter uses only298.15K despite the underlying paper's
broader range. The rounded NIST mol/kg/bar entry0.0013 maps to1.2961e-5mol/m3/Pa using
nominal water density997kg/m3. Agreement within5% checks units/coarse consistency,
not a precision error estimate. Note molarity versus molality, Pa versus bar, and
solubility versus volatility Henry conventions; they cannot be interchanged.

The Millero/Sherwood rounded measured entries on the same compilation page provide
alternative sensitivity scenarios. Their paired H/slope values are preserved;
scenario bounds are not statistical confidence intervals or independent held-out
validation. Source-fit uncertainty, interface kinetics and calorimetric uncertainty
are incomplete. The interface coefficient is an operator input, not fitted to
measurements. Rates shown by the model follow that selected constitutive law.

Fixed gas/liquid occupied volumes, pure-water solvent, no added salts/cosolvent,
no O2 chemistry, and a fixed-T external bath define the selected model. The liquid
carrier and inert gas background are implicit/constant; conserved material here is
O2, with unchanged carrier baselines. Concentrations are per occupied phase volume;
pressure is ideal O2 partial pressure, not a solved mechanical fluid pressure.
The phase-volume ratio determines equilibrium capacity; assigned area/permeance
determine relaxation speed. Uniform dual continua are not reconstructed bubbles,
droplets, moving interfaces or free-surface geometry.

Energy reference is relative phase internal energy: gas O2 has u_g=0 at fixedT;
conditional deltaH=-R*dln(Hcp)/d(1/T), deltaU=deltaH+RT for ideal gas and zero liquid
partial-volume work. This inferred caloric approximation is not measured calorimetry.
Aqueous excess energy U=deltaU*nl travels with aqueous O2. Only phase exchange enters
the bath ledger; transport conserves that excess energy globally under periodic/sealed
boundaries. This is not full thermofluid enthalpy/pressure-work or carrier-energy
validation. Dissolution lowers material U and releases heat to the bath; degassing
reverses the sign. No extra heat source is added to the phase energy change.

`oxygen_partition_local_v1.json` is a single finite pair, and
`oxygen_partition_spatial_v1.json` uses64co-located finite pairs. Both bind the data
pack SHA. Interface area is per cell, not one domain area implicitly duplicated.
When refining a fixed dual-continuum experiment, preserve area per bulk volume
unless intentionally changing the physical interface model. Spatial phase velocities
and diffusivities are separate imposed inputs; they are not measured oxygen D or
solved-fluid momentum feedback.
