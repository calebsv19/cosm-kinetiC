# Conditional aqueous glucose data

`aqueous_glucose_v1.json` transcribes numerical observations from Isbell and Pigman,
US Bureau of Standards J.Res.10,337–356(1933), RP534, sectionIV.4,p356:
https://nvlpubs.nist.gov/nistpubs/jres/10/jresv10n3p337_A2b.pdf .
It is a government-work numerical transcription, not new experimental data.
The local/spatial configs select the data by SHA256; a changed pack needs explicit
regeneration/requalification. Source locator, recipe, missing metadata and the
training/comparison split are retained in the pack. No PDF is bundled.

Selected effective network: alpha-D-glucopyranose <=> beta-D-glucopyranose, both
C6H12O6 and neutral. Intermediate/minor forms are unresolved, so these first-order
channels are phenomenological rates, not microscopic ring-opening constants.
Only the documented water recipe at274.65K is admitted. The reported±0.5K describes
the experiment, not a validated interpolation interval. pH, water purity, ionic
strength and pressure are unreported. No catalyst or salt is added in the listed
preparation. Density was measured at2C rather than exactly the kinetic temperature;
concentration derived from the masses/density is nominal, not known to six-digit
physical accuracy. Atomic weights used for inventories give180.156g/mol.

`scripts/chemistry_aqueous_data.py` fits a natural-log relaxation constant to the
first four rotations relative to the4min reading and measured long-time endpoint.
The121min and240min readings do not enter that fit. The endpoint and calibration
rotations determine the initial mixture and equilibrium concentration ratio;
they are calibration inputs, not held-out predictions. Forward/reverse constants
are lambda*K/(1+K) and lambda/(1+K), with K=beta/alpha. This enforces detailed balance.
The paper's common-log rate is converted by ln(10) and60s/min before comparison.

The reported specific-rotation calibrations imply an initial alpha fraction near
0.9619 at the first measurement. We infer that initial mixture from the observation,
rather than assume100%alpha at dissolution. No claim is made that impurity, prehistory
or calibration error has thereby been measured. The local reference clock starts
at4min; engineered initial fractions are explicitly distinguished in reports.

The approximate oxidation-based beta fraction near0.64 is a separate method in the
same paper under different near-zero-C conditions. It is only a coarse consistency
comparison, not an independent matched-condition equilibrium measurement. Conditional
K is not extrapolated to other activities, concentrations, temperature or solvent.
`RT*sum(n_i*log(x_i/x_eq_i))` is the effective ideal-mixture relative Gibbs/Lyapunov
function at this fixed condition; it is not measured full solution Gibbs energy.
Conditional deltaG0=-RTln(K) is not deltaH. Reaction heat remains unavailable.

All reported rounding bands vary only the listed rotation digits, holding time
stamps fixed. K and initial-fraction calibration bounds are also rotation-only.
Unknown time precision, temperature dependence, preparation errors, purity, pH,
systematics and reduction error are not included. The held-out discrepancies exceed
these narrow bands. Engineering comparison tolerances are not confidence intervals;
the protocol was chosen after exploratory source inspection and is not blinded.
This is bounded empirical qualification, not a metrology-certified predictive model.

Empirical kinetic support is restricted to the observed4..240min window. Reports
flag longer elapsed time as extrapolation. The later constant reading is an
asymptotic calibration input; there is no validated timed24h trajectory or late
endpoint temperature protocol. Mathematical long-time equilibrium tests verify
the effective equations, not that unobserved laboratory trajectory.
