# Fire–Fluid local heat and passive surface checkpoint

The 2026-10-10 checkpoint **does not admit burning box faces**. It reuses the
retained production-solver fields from [the source/transport checkpoint](fire_fluid_accuracy.md),
with identical 32 prescribed Fire windows, 3.2-second duration, physical geometry
and separate grid/time refinement. No new native solver or live coupled run is
claimed.

## Local observations

| Observable | No box, 32³→64³ | Insulated box, 32³→64³ |
| --- | ---: | ---: |
| Source-footprint mean height profile, L1 | 9.620% | 9.496% |
| Source-region heat on native 32³ partition, L1 | 16.812% | 16.712% |
| Proposed flux diagnostic maximum absolute difference | 3.415 W/m² | 2.723 W/m² |
| Fine-grid maximum proposed flux diagnostic | 0.492 W/m² | 0.279 W/m² |

The mean height profile uses 32 fixed physical slabs across x/y 0.25–0.75 m.
Its 16³→32³ differences are 25.710% and 25.549%. The finer spatial check covers
that footprint up to z=0.375 m and conservatively restricts extensive energy to
32³ cells. The unchanged 10% heat gate fails there despite passing for the
height-averaged profile.

Six gas observation volumes sit 1/16 m outside the box faces, with thickness
1/32 m and inset tangent bounds 0.40625–0.59375 m. Exact geometric overlap
integrates the solver's piecewise-constant cell averages; no subcell temperature
reconstruction is claimed. The no-box control samples the same physical volumes.
The diagnostic is `q = h (Tgas - Ts)` with explicitly synthetic `h=10 W/m²/K`
and `Ts=300 K`. This coefficient is not inferred from wind or a measured material.
The existing native box is insulated: **actual exchanged gas-to-box heat is zero**.

The diagnostic flux does not pass spatial convergence. Relative differences are
695% without the box and 978% with it because the fine-grid signal is small.
For the box, the strongest face falls from 3.002 to 0.279 W/m². The absolute
differences above are necessary context; neither metric establishes a reliable
surface heating input. This is consistent with sensitivity of the weak plume
tail to numerical transport, not proof of the cause.

Separate no-box 32³ time refinement gives height-profile differences
0.1462%→0.07284% and diagnostic flux differences 1.909%→0.9486% for
dt=0.005→0.0025→0.00125 s. This does not qualify box time refinement or transient
flux/exchange-window behavior.

## Passive response boundary

`coupling/passive_surface.py` implements the exact constant-window solution of
`C dTs/dt = h A (Tgas - Ts) - G (Ts - Tambient)`. It records prescribed gas
transfer, cooling and stored-energy change separately. The manufactured fixture
uses A=0.0625 m², C=100 J/K, h=10 W/m²/K, G=0.2 W/K, Tgas=310 K and initial/
ambient temperature 300 K for 3.2 seconds. These are synthetic coefficients.

The independent analytic temperature comparison differs by 1.14e-13 K;
window energy residuals are below 7.5e-15 J, and checkpoint replay is exact.
Cooling, zero transfer, invalid-input refusal and solid-excluding observation
geometry also pass. This qualifies the **prescribed-input response numerics**.
It does not implement reciprocal native gas debit, radiation, material ignition,
measured thermal calibration or a physical box surface boundary.

The admission helper requires local heat, actual gas/surface flux, response
numerics, reciprocal exchange and material ignition qualification. It is used by
this qualification harness, not integrated into a burning-face runtime; that
runtime has not been enabled.

## Reproduce

Run `python3 -B -m unittest discover -s tests -p test_passive_surface.py -v`.
Run `scripts/fire_fluid_surface_qualification.py` with `--accuracy-root` pointing
to the retained accuracy checkpoint, `--prior-root` to the isolated `plume`
checkpoint and `--output` to a fresh task-owned directory. It emits compact JSON
and hashes its field inputs, verifies source/time-step identity and available
native field digests, and refuses burning admission. It never rewrites those
inputs. Native-endian arrays require the original compatible architecture.

The evidence is `_private_workspace_artifacts/fire-fluid-surface-20261010/summary.json`.
The next boundary is reducing local source/plume transport error with the same
source and domain, then testing transient face flux and exchange-window
sensitivity. Only after that should a reciprocal passive gas-loss/surface-gain
adapter and material-specific response be qualified. No release version change
is recommended for these development qualification tools.
