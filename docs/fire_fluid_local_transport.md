# Fire–Fluid local transport qualification

<!-- reconstructed-flow-current:start -->
## 2026-10-11 — Reconstructed transfer and fine flow timing completed

Conservative bounded MC transfer, with original sources and 5 ms flow / 1.25 ms
scalar timing, gives **10.320860%** local heat difference, versus piecewise-
constant **9.590649%**. The unchanged 10% gate fails; the other five screens
pass. Parent heat is conserved, the coarse control is bit-identical, and the
independent array readback agrees. On the same fine grid, the transfer changes
local heat by 1.420622% and velocity by 1.232638%. The earlier narrow pass is
representation-dependent, not a robust qualification.

Original own-thermal 128³ flow at 5/2.5/1.25 ms, with scalar timing fixed at
1.25 ms, passes both adjacent temporal comparisons and shrinking differences.
Heat changes are **0.226292% / 0.112242%**; velocity changes are
**0.495013% / 0.240956%**. Absolute difference ratios are 0.4960 / 0.4869.
Against the fixed 64³/5 ms baseline, heat remains **13.153941%, 13.055688%,
13.006251%**. The latter two comparisons use different flow timing across grids;
they do not replace the original matched-timing spatial verdict. Time refinement
is stable and does not repair the 10% heat gate. No specific solver defect is
isolated. Burning surfaces and gas-to-surface response remain unqualified.

Next boundary: resolve spatial heat/flow accuracy in the source-adjacent plume
with a genuinely finer independent reference or a qualified spatial repair;
retain original sources, stabilized timing and the unchanged 10% heat criterion.
Evidence: `_private_workspace_artifacts/fire-fluid-reconstructed-flow-20261011/`
(`CHECKPOINT.md`, `transfer-verdict.json`, `timing-verdict.json`,
`independent-readback.json`). Canonical, versions and ordinary worker capability
remain unchanged; the qualification helpers are uncommitted in Main Edit.
<!-- reconstructed-flow-current:end -->

## 2026-10-11 — Shared old-time thermal forcing passes the diagnostic gate

The accepted 64³ driver and independently evolving 128³ momentum were run online
with original Fire source mapping at both grids, 5 ms flow and 1.25 ms heat/smoke
steps: 640 flow steps and 2,560 scalar cycles over 3.2 s. Before each flow step,
the coarse driver's old-time thermal cell averages were mapped to fine cells as
piecewise-constant temperature (extensive energy divided by eight). Only momentum
reads this prescribed array; fine accepted heat/smoke retain their own original
sources, transport and accounting. This is externally forced attribution, not a
replacement fully coupled solver. The fine **heat source** is not parent/8.

| Relative final-state difference | 64³ → 128³ under shared forcing | 10% screen |
| --- | ---: | --- |
| Local heat L1 | 9.590649% | Pass |
| Full MAC velocity L2 | 4.701925% | Pass |
| Native wind-probe norm | 0.147166% | Pass |
| Height temperature-rise profile L1 | 2.705214% | Pass |
| Full smoke L1 | 3.250541% | Pass |
| Peak temperature rise | 7.906094% | Pass |

All six diagnostic screens pass. The local heat margin is only **0.409351
percentage points**, so this is a narrow pass with a stated forcing representation.
The fully coupled independent-grid comparison remains **13.153941%** heat and
**10.025080%** full velocity: its gate has not been repaired or qualified by this
diagnostic. The corrected matched-velocity/original-source heat comparison remains
6.434321%. Differences are resolution sensitivities, not calibrated errors.

The prescribed case has 4.642995% heat-ROI center-velocity disagreement. In the
Z=0.125–0.25 m band it is 5.153980%, versus 12.123819% in the fully coupled pair;
X/Y components are each 9.144560%, versus 20.693301%. That band retains 68.589%
of prescribed-case heat difference. At the same native 128³ grid, changing from
its own thermal field to coarse prescribed forcing changes velocity by **8.006732%**
L2. This establishes material thermal-input sensitivity; coarse forcing representation
remains part of it, so it is not a pure feedback attribution or isolated solver defect.

The signed field chain closes within 1.74e-18 J. Heat difference norms are
10.170603 J for matched transport, 8.264334 J between matched-velocity and
prescribed-momentum heat, 10.398958 J for thermal-input choice, and 20.792188 J
for the full coupled comparison. These L1 norms are not additive percentage shares.

The final coarse fields are exactly the accepted baseline SHA-256
`aa64f89f439bd24216ff831cb2f1ded496aaeca3cc9cfaee6821058dfff64abf`.
Last old-time forcing samples at 3.195 s reproduce each child value exactly and
restrict exactly to coarse extensive heat. Each state retains 158.07203249 J and
0.00015807203249 kg smoke; maximum heat balance magnitude is 1.90e-11 J and
smoke balance 2.32e-16 kg. Maximum divergence is 1.29e-12 /s and pressure
residual 5.00e-12. Charged numerical memory peaks at 1,523,930,652 bytes within
2 GiB, with zero live bytes and no allocation refusals after destruction.
Native online replay takes 76.317 minutes. Independent NumPy field/divergence
readback agrees with the strict report.

Fresh contained compile, exact evolving-control/no-injection/refusal native and
ASan/UBSan tests, two new offline report tests, 74 native plus 74 identical
sanitizer accuracy observations, and 59 Python contracts with a freshly built
ordinary worker pass. Long full-grid replay is native, not sanitizer instrumented.
Ordinary, full-grid and fixed-scalar accepted preprocessing tokens match the
previous operator; 158 other native app/header inputs are unchanged. The new
compile-only hook is active solely in the scoped qualification helper. Full-app,
package/install and remote gates were not run because ordinary runtime behavior
and worker protocol are unchanged. No Desktop refresh or review rebind occurred.

Next boundary: verify conservative reconstructed coarse thermal forcing and its
sensitivity against this piecewise-constant case, preserving original sources,
fixed timing and the 10% heat gate. This addresses the remaining thermal-transfer
representation confound before interpreting feedback more strongly. Fine-flow
timestep sensitivity remains separate. The fully coupled heat gate, gas-to-surface
flux and material response must be qualified before burning faces are enabled.
No specific production solver defect has been isolated.

Evidence: `_private_workspace_artifacts/fire-fluid-prescribed-flow-20261011/CHECKPOINT.md`,
`verdict.json`, `independent-readback.json`, frozen source and terminal native fields.
Final arrays are comparison carriers, not restart checkpoints. Source inputs are
reused from prior verified roots; no full fine source or velocity histories were
copied. Retain fields, forcing samples, frozen probe/worker/source and compact
contract/recovery evidence for independent readback and the next transfer comparison.
Canonical and Main Edit branch heads and versions are unchanged. No commit,
adoption, VERSION, release package, Registry, publication or remote effect occurred.
Atlas was considered: this supports the existing unadopted coupling qualification
parent, with no new plan, topology or adoption claim. No memory write was requested.
Minor release recommended; approval required before version bump.

## 2026-10-11 — Fixed-scalar spatial verdict and corrected flow attribution

The accepted no-box solver was independently evolved at 64³ and 128³ with
identical 5 ms flow steps and 1.25 ms heat/smoke steps: 640 flow steps and
2,560 scalar cycles over 3.2 s. The frozen Fire source, original receiver
mapping at each grid, unit-cube domain and physical heat region (XY
0.25–0.75 m, Z 0–0.375 m) are unchanged. The coarse final native fields
match the prior accepted fixed-scalar 64³ baseline bit for bit.

| Relative final-state difference | 64³ → 128³ | 10% screen |
| --- | ---: | --- |
| Local heat L1 | 13.153941% | Fail |
| Full MAC velocity L2 | 10.025080% | Fail |
| Native wind-probe norm | 2.288144% | Pass |
| Height temperature-rise profile L1 | 4.556579% | Pass |
| Full smoke L1 | 5.005689% | Pass |
| Peak temperature rise | 7.558539% | Pass |

**The unchanged 10% local heat gate still fails.** The prior spatial heat
result was 13.145646%; matching scalar timing gives 13.153941%, a change of
only 0.008295 percentage points. Different scalar timing does not explain
this gap. Full MAC velocity also narrowly exceeds its supporting 10% screen.
The wind probes are outside the most discrepant source-adjacent band; their
agreement is not full-field agreement.

The Z=0.125–0.25 m band contains 73.112% of the local heat L1 difference.
Its velocity difference is 12.124%, with X/Y components each
20.693% and Z 9.688%. Component RMS differences are
0.005914 m/s horizontally and 0.009009 m/s vertically. The outer 1/32 m
source-footprint perimeter contains only 0.298% of heat difference; the
largest common-cell temperature difference is 3.146 K near Z=0.148438 m.
These are resolution sensitivities, not calibrated prediction errors.

A source cross-check found that parent/8 splitting changes the fine source
shape by up to 5.037617%, despite conserving totals and coarse cell means.
The preliminary run was stopped before fine evolution. Compact original
fine support reconstructs the prior full fine source hash exactly. The
older matched-velocity diagnostic also used that parent split, so its
6.228% result alone did not qualify original-source attribution. A corrected
online diagnostic uses the original source and the same 1.25 ms scalar time.
Its local heat difference is **6.434321%**, passing 10%, with exact coarse
heat parity at every step and exact accepted final driver fields. It follows
prolonged coarse flow rather than solving fine momentum. The heat difference
norms are 10.170603 J under matched flow, 16.657593 J between matched and
independent fine flow, and 20.792188 J overall. Their signed field decomposition
closes to 1.74e-18 J; the L1 norms are not additive percentage shares. The
remaining residual combines flow representation and evolving flow response.

An independent smooth manufactured buoyancy/mixed-pressure projection impulse
passes at32/64/128. Continuum velocity error at128 is 0.00292828%, shrinking
by almost exactly four per grid doubling. Discrete pressure/velocity references
agree within 3.51e-15 Pa and 2.54e-16 m/s. This checks forcing/projection
consistency, not nonlinear plume evolution or finite-time floor viscosity.

Both evolving cases retain 158.07203249 J and 0.00015807203249 kg smoke.
Maximum heat balance magnitude is 2.02e-11 J, smoke balance 2.23e-16 kg,
divergence 2.20e-12 /s and pressure residual 5.00e-12. Charged numerical
memory peaks at1,392,529,200 bytes, within2 GiB, and closes to zero with no
allocation refusals. Native paired replay takes85.512 minutes; the corrected
matched diagnostic takes14.700 minutes concurrently. Independent NumPy
heat readback agrees exactly with the fsum-based gate. The dimensional thermal
diffusion scale sqrt(alpha*t) is8.306 mm versus15.625/7.8125 mm cells; it is
not a measured front thickness or proof of adequate resolution.

Fresh compile, native/ASan/UBSan source/timing/matched-flow controls, seven
offline report tests,74 native plus74 identical sanitizer accuracy observations,
and59 configured Python contracts pass. Long full-grid replays are native,
not sanitizer runs. Newly built ordinary binaries have different hashes from
the prior checkpoint, so fresh regression proof replaces prior binary-proof
reuse. No ordinary numerical operator changed in this round. Full-app,
package/install and remote gates were not run for qualification-only tools.
Fixture/interpreter/output-format failures and the source correction are
recorded separately. 128³ admission and fixed scalar timing remain contained
qualification profiles; this is not a released worker capability.

Next bounded boundary: evolve fine momentum under a shared, prescribed thermal
forcing history from the accepted coarse driver, online, with original heat
sources and fixed timing. This separates differing thermal forcing/feedback
from flow spatial response without storing large histories. No specific
production numerical defect is isolated yet. Fine flow timestep sensitivity,
gas-to-surface flux, surface response and physical calibration remain
unqualified; burning faces and reciprocal thermal exchange stay disabled.

Evidence: `_private_workspace_artifacts/fire-fluid-flow-spatial-20261010/CHECKPOINT.md`,
`spatial-verdict.json`, `matched-original-verdict.json` and
`buoyancy-impulse/verdict.json`. Final fields are comparison carriers, not
restart checkpoints. Retain compact original source, fields, frozen code and
contract/failure capsules for the next comparison/recovery consumer. Task-owned
disposable builds are retired separately after active-consumer checks. The
review site is preserved. Canonical, versions and branch heads remain unchanged;
no commit, adoption, package, Registry or release action occurred. Atlas was
considered: this supports the existing unadopted coupling parent, with no new
plan, topology or adoption claim. No memory write was requested. Minor release
recommended; approval required before version bump.

## 2026-10-10 — Evolving 64³ flow timestep qualification passes

The accepted no-box plume is independently evolved at 5, 2.5 and 1.25 ms flow
steps, with heat and smoke held at 1.25 ms in every case (2,560 scalar cycles).
The frozen source, 1 m³ domain, 3.2 s duration, 0.1 s source windows and physical
heat ROI (XY 0.25–0.75 m, Z 0–0.375 m) are unchanged. This is local Main Edit
qualification; ordinary app/worker admission, numerical operators and timing
remain unchanged. A compile-only qualifier control refuses unsafe or nonintegral
scalar timing rather than adapting it.

| Relative final-state change | 5 → 2.5 ms flow | 2.5 → 1.25 ms flow |
| --- | --- | --- |
| Local heat L1 | 0.143431% | 0.071375% |
| Full MAC velocity L2 | 0.343055% | 0.170098% |
| Wind-probe vector norm | 0.586734% | 0.294917% |
| Height temperature-rise profile L1 | 0.094157% | 0.046793% |
| Full smoke L1 | 0.108380% | 0.053886% |
| Peak temperature rise | 0.040631% | 0.020269% |

Both temporal pairs pass the unchanged 10% local heat gate and 10% observable
screens. Absolute local heat and velocity differences shrink by factors 0.4976
and 0.4960. Independent NumPy native-array readback agrees. This supports retaining
5 ms flow for this 64³ baseline. It is final-state sensitivity, not an exact
solution, formal order claim, physical calibration or a 128³ temporal verdict.

All cases retain 158.072 J, with maximum heat balance magnitude 3.59e-12 J,
smoke balance 3.19e-17 kg, divergence 7.40e-13 and pressure residual 5.00e-12.
Charged memory peaks at 173,919,744 bytes and closes to zero. Total native replay
wall time is 49.585 minutes. Fresh build, native/ASan/UBSan timing/refusal tests,
15 focused report tests, 74 native + 74 identical sanitizer accuracy rows and 59
configured Python regressions pass. Full long replays are native, not sanitizer
instrumented. Ordinary worker/probe bytes match the prior accepted binaries;
full app/stable proof is reused for identical runtime bytes. Corrected fixture
and build-lock invocation failures are recorded separately.

The prior independent 64³→128³ coupled spatial heat difference remains 13.146%,
above 10%. A separate retained-field readback finds 10.079% full face velocity
and 9.976% heat-ROI cell-center velocity disagreement; the source-adjacent
Z=0.125–0.25 m velocity band differs 12.192%. Those older runs used adaptive
scalar counts 640/783, so this is localization context, not a new fixed-scalar
spatial gate. No surface heating or burning faces are enabled.

Next boundary: a paired 64³/128³ evolving-flow spatial comparison with the same
fixed scalar timing, using normal-face area restriction and the source-adjacent
velocity/heat bands to localize any remaining discrepancy before a repair.
Fine-grid temporal sensitivity and gas-to-surface flux remain unqualified.

Evidence: `_private_workspace_artifacts/fire-fluid-flow-time-20261010/CHECKPOINT.md`
and `flow-time-verdict.json`. Retain three terminal fields (31,752,192 bytes),
frozen qualifier/source/analysis and compact logs for the next comparison; reuse
the prior source input rather than copying it. The 23 lifecycle-marked contract
capsules total 14,549,486 bytes and remain recovery/comparison evidence. Both
task-owned disposable build roots were retired after consumer checks. The local
review page and state remain HTTP 200 and unchanged. Canonical remains clean;
no commit, adoption, VERSION, package, Registry or release action occurred.
Atlas was considered: this is supporting evidence under the existing unadopted
coupling lane, with no new plan, topology or adoption claim. No memory write was
requested. Minor release recommended; approval required before version bump.

## 2026-10-10 — Matched evolving flow heat gate passes

A separate native qualifier streams one accepted evolving 64³ no-box plume flow
into heat carriers at 64³/128³. It uses the existing minmod/Euler scalar operator,
flux/divergence-preserving velocity prolongation, fixed 5 / 2.5 ms scalar steps,
the same 3.2 s frozen-source replay and physical ROI, and the unchanged 10% gate.
Local spatial heat is 6.340% / 6.228%; halving the heat step changes local heat
0.329% at 64³ / 0.463% at 128³. At 2.5 ms, fixed-volume height profile differs
2.605% and peak temperature rise 7.047%. The diagnostic gate passes. The smaller z=0.25–0.375 m band still differs
18.84% (1.441 J over 7.648 J); this is a localization diagnostic, not a new gate,
and individual surface-plane flux accuracy remains unqualified. The prior
independently evolved coupled 64³→128³ result remains 13.146% and unqualified.

The heat control matches the production operator bit-for-bit at every flow step;
final 64³ heat/smoke/velocity bytes match the accepted reference exactly. All four
carriers retain 158.072 J, heat residuals below 4e-13 J, transferred divergence
below 7.45e-13 and maximum heat CFL 0.21754 < 0.4. Charged memory peaks at
1,070,399,664 bytes under 2 GiB and closes to zero. Full replay took 12.11 minutes.
A global-maximum qualification guard initially stopped an incomplete trial; it
was replaced by the accepted local heat stability rate, with no adaptive cycles
or product operator change. That incomplete trial and its binary are retained.

Fresh compilation, targeted native/ASan/UBSan including 128³ transfer/heat capacity,
19 report tests, 74 native + 74 identical sanitizer accuracy rows and 56 Python
regressions pass. The complete replay is native, not sanitizer-instrumented.
Ordinary worker/probe bytes and runtime sources are identical; full app/stable
proof reuses the prior identical-runtime checkpoint. The initial Python setup
and ordinary-profile capacity failures were corrected and recorded separately.

Next: qualify evolving flow/velocity representation and flow timestep sensitivity
before changing the heat scheme or enabling surface heating. The fine diagnostic
flow represents the coarse history, with linear normal and piecewise constant
tangential velocities; it is not an independently solved fine flow. The 17.051 J
L1 remainder against that fine-flow reference combines flow evolution,
representation and prior adaptive cycles, and cannot be treated as an additive
percentage contribution. Two heat timesteps are a sensitivity check, not a formal
order. Frozen prior 8³ Fire source, not live fine-grid feedback or calibration.
Burning faces and reciprocal thermal exchange remain disabled.

Evidence: `_private_workspace_artifacts/fire-fluid-matched-heat-20261010/CHECKPOINT.md`
and `matched-heat-verdict.json`. Final diagnostic fields, compact frozen source,
analysis and failure proof, plus 23 retained contract capsules are the next flow
comparison inputs. Task-owned disposable verification builds are retired after
consumer checks. Canonical, HEADs, versions and saved review bindings remain
unchanged. Minor release recommended; approval required before version bump.
Atlas considered: supporting qualification within the existing unadopted coupling
lane; no new top-level plan, topology or adoption claim. No memory-folder writes.

### Reproduce the matched-flow diagnostic

From Main Edit, use a fresh task-owned `BUILD_DIR` with
`make open-matched-heat-probe test-matched-heat-native test-matched-heat-sanitize`.
The tests include a 128³ capacity mode under the qualification-only profile.
Run `<BUILD_DIR>/open-atmosphere/matched-heat-probe <accepted-64-source.bin>
<fresh-output-directory>`, retaining stdout as `native.json` and stderr as
`native.log`. Sources are the existing 32 frames of extensive heat/smoke native
doubles at 64³. Each flow window holds the accepted projected velocity for
5 ms; the four heat carriers take exactly 640 or 1280 scalar steps. Fine source
amounts divide each coarse cell equally among its eight subcells. Coarse face
flux is copied to four fine subfaces, with intermediate normal faces interpolated.
No fine momentum/projection solve or fine smoke transport is claimed.

The report command is `python3 -B scripts/fire_fluid_matched_heat_report.py
--root <owned-evidence-root> --control <accepted-64-run> --coupled-fine
<accepted-128-run> --output <fresh-report.json>`. Its evidence root contains
`run/native.json`, the four heat fields and driver fields, successful
`run-terminal.json`, `run-identity.json` with source/executable digests, and
`qualified-matched-heat-probe`. Preserve the compact identity receipts and final
fields. The field files are native-endian doubles on the qualification host.


### Exact fixture replay helper

Use `python3 -B <this-evidence-root>/replay.py --probe <this-evidence-root>/qualified-matched-heat-probe --source /Users/calebsv/Desktop/CodeWork/_private_workspace_artifacts/fire-fluid-accuracy-20261010/no-box-64-dt005/sources.bin --output <new-evidence-root>`. This checks the accepted source digest, executes the frozen host-native qualifier with a one-hour cap, and produces the identity/terminal records required by the report. It refuses an existing output root. Then run the report command above against the accepted resolution references. Original input source and reference fields must remain available.

## 2026-10-10 — Full-grid resolution verdict: heat gate still fails

The accepted minmod/Euler scalar and conservative-momentum solver completed full
32³/64³/128³ no-box prescribed-source runs. The unchanged local heat difference
improves 16.206% → 13.146%, but the 10% gate still fails. On one fixed 32³ physical
observation partition, disagreement falls 25.616 → 18.190 J (ratio 0.710); that
partition still differs by 11.508%. The three-level indicator is only 0.494 and
difference-pattern cosine 0.558, so clean asymptotic convergence is not established.
Height-rise profile 4.548%, smoke 4.984%, wind 2.317% and peak-rise 7.416% all pass
the declared 10% stabilization screen; peak increments nevertheless grow slightly.
73.3% of the remaining local heat disagreement lies just above the source.

Source-volume restriction agrees within 1.401e-16; 32³/64³ controls and ordinary
worker/probe bytes are identical to the accepted checkpoint. Fresh builds, full
128³ native replay, native/refusal/report checks, 128³ capacity ASan/UBSan and
52 Python regressions pass. Four final report tests also pass after adding scalar
cycle metadata. Full app/stable reuses prior proof for identical runtime bytes;
the full 640-step fine replay is not sanitizer-instrumented. Accepted CFL control
uses 783 scalar cycles at 128³ versus 640 at 64³; independent fine-grid time
accuracy remains unqualified. Fine replay took about 68.24 local CPU minutes.

Next: bounded matched-velocity heat transport with explicitly controlled scalar
timesteps, separated from evolving buoyancy/projection, under the same physical
ROI and 10% gate. Do not enable surface heating/burning or assume 256³ will solve
this. No new solver defect is established. This is frozen prior 8³ Fire-source
replay, not fine-grid live feedback, calibrated combustion or release admission.
Evidence: `_private_workspace_artifacts/fire-fluid-resolution-20261010/CHECKPOINT.md`
and `resolution-verdict.json`. Three exact idle disposable builds and the
regenerated 1 GiB source were retired; final fields, frozen source/helpers,
compact proof and 23 CFD capsules are retained for that isolation. Canonical,
HEADs, versions and saved review bindings remain unchanged. Minor release
recommended; approval required before version bump. No release or memory-folder
effects. Atlas considered: supporting proof within the existing unadopted lane,
no new top-level plan, topology or adoption claim.

Reproduce with a fresh task-owned `BUILD_DIR` and `make open-full-grid-probe`.
Run `python3 -B scripts/fire_fluid_accuracy.py --full-grid --grid 128 --dt .005
--frames <matched-source-frames.json> --probe <full-grid-probe> --output
<fresh-directory>`; replay 32³/64³ controls with the same profile. After all
three runs, use `python3 -B scripts/fire_fluid_resolution_report.py --root
<three-run-root> --output <fresh-report.json>`. The root also needs the independent
`source-restriction.json` proof. Ordinary worker/receiver admission is unchanged.
Input arrays are extensive native-endian doubles. Preserve final fields and
frozen source frames; retire regenerated large arrays after digest/consumer checks.

## Source-edge scalar candidate rejected (2026-10-10)

An independent exact-characteristic fixture now tests a stationary 0.125 m
volumetric source transported upward at 0.2 m/s, including aligned and off-grid
edges. It checks 16/32/64/128 vertical cells, separate timestep refinement,
heat/smoke proportionality, conservation, positivity and the known 0.5 K peak.
The retained minmod/Euler solver converges to 1.69% aligned / 1.76% off-grid L1
error at 128 cells. These are transport verification results, not fire calibration.

A bounded WENO-Z face reconstruction, conservative flux correction and two-stage
scalar advancement were evaluated in a sibling module. This candidate improved
64-cell source-edge errors from 4.01% to 1.95% aligned and 4.53% to 1.77% off-grid;
128-cell errors reached 0.621% / 0.576%. The independently heated translating
packet improved from 6.65% to 2.01% at 64 cells. No global fifth-order claim applies
to the coupled solver. Initial peak and floating-point positivity failures were
recorded and repaired within the experimental candidate before the final replay.

**The candidate fails the unchanged 10% local heat gate and is rejected.**
Using the same sealed Fire amounts, domain, duration, dt, diffusivities, physical
ROI and 32³→64³ restriction, the no-box local heat difference is 16.631%, versus
16.206% for the retained momentum-corrected solver. Absolute ROI disagreement
increases from 25.616 J to 26.289 J; 69.8% lies in z=0.125–0.25 m immediately above
the source. Total heat/smoke balance is retained (residuals below 4.7e-12 J and
3.5e-17 kg); the remaining failure concerns spatial redistribution. The inert-box
candidate replay was not run after the prerequisite no-box gate failed.

All candidate runtime/header/probe/helper changes were restored byte-for-byte;
the accepted conservative momentum repair remains. Only the exact source-edge
fixture and its native/sanitizer registrations are retained. A fresh restored
worker, probe and 32³ plume field reproduce the previous qualified bytes exactly.
Fresh verification passes 74 native and 74 identical ASan/UBSan accuracy rows,
open-atmosphere/lateral/box native and sanitizer contracts, and 44 Python worker,
coupling, recovery and observation regressions. The candidate's 74 native rows
and 44 Python tests passed; its partial sanitizer run was stopped after rejection
and is not a full sanitizer pass. Full app/stable gates were not repeated because
runtime sources and binaries match the prior app/stable-qualified checkpoint.

The candidate buoyancy fixture used prescribed accepted temperatures to isolate
Euler momentum response from stage-dependent inflow; that fixture adjustment was
also restored. The original coupled heating fixture remains in the retained suite.
The source frames were frozen from the earlier 8³ coupled run; fine-grid cases
are prescribed-source replays, not live fine-grid Fire feedback or physical proof.

Evidence: `_private_workspace_artifacts/fire-fluid-source-edge-20261010/`
`CHECKPOINT.md`, `plume-comparison.json`, `source-edge-comparison.json`, frozen
candidate source/executables and restoration/parity receipts. Next: compare heat
transport under matched plume velocities against evolving buoyancy/projection,
with the source and scalar method held fixed, to isolate the coupled-grid error.
The 10% gate remains; surface heating, reciprocal heat debit and burning faces
remain disabled/unqualified. Canonical, versions and saved review bindings are
unchanged. The review remains HTTP 200; this candidate was not rerendered,
packaged, released or visually adopted.

## Conservative momentum repair (2026-10-10)

The active Main Edit solver now transports momentum with shared conservative
fluxes across MAC dual-cell faces. The previous limited advective derivative
created horizontal momentum in an independently divergence-free periodic flow:
one step drifted by -2.93646e-6 / +2.43217e-6 kg m/s without forcing or boundary
transfer. The same test now holds drift below 6.7e-16 kg m/s at 16/32/64 XY cells.
This fixes momentum balance; it does not claim kinetic-energy conservation for
the dissipative upwind scheme. Conservative staggered momentum discretization
is motivated by the [Morinishi conservation study](https://ntrs.nasa.gov/citations/19960022304);
these local tests qualify this implementation independently.

A native traveling viscous shear has nonzero momentum advection, an independent
continuum solution, and no velocity prescription after initialization. Velocity
perturbation L1 errors decrease from 1.870% to 0.5287% to 0.1351% at 16/32/64 Y
cells. Grid refinement and fixed-grid time refinement are separate. The change
leaves scalar reconstruction, Fire source amounts, diffusivities, domain,
physical ROI, duration and the 10% local heat threshold unchanged. The momentum
CFL bound now uses outgoing dual-face speeds consistently with the flux update.

| Matched 32³ → 64³ local heat | Prior solver | Conservative momentum |
| --- | ---: | ---: |
| No box | 16.813% | 16.206% |
| Unlit insulated box | 16.712% | 16.170% |

**The 10% local heat gate still fails.** The independently verified momentum
repair is retained in Main Edit; it is not a complete plume accuracy repair.
No-box fixed-grid timestep differences decrease from 0.2096% to 0.1042% in heat
and 0.1500% to 0.07454% in smoke for dt 0.005/0.0025/0.00125 s. This supports a
spatially dominated remaining mismatch at this resolution, without identifying
another operator defect. About 68% of the absolute local heat discrepancy lies
in z=0.125–0.25 m, the first layer above the source. Near-zero-temperature tails
must be assessed by absolute amounts, not large relative ratios alone.

All six native prescribed-source runs preserve heat/smoke budgets (maximum
absolute residuals 6.71e-12 J / 3.50e-17 kg), admitted temperature contrast and
pressure/divergence bounds. Both box grids preserve exactly zero blocked normal
velocity and solid heat/smoke. Actual box heat exchange remains zero. Diagnostic
box Robin flux differences are 3.844 W/m² and remain unqualified; this exceeds
the prior 2.723 W/m² discrepancy and is not a surface-response improvement.

Verification: fresh worker/probe compilation; 60 native and 60 ASan/UBSan accuracy
rows with exact agreement; open-atmosphere/lateral native and sanitizer contracts;
44 worker/coupling/observation/passive-surface regressions; fresh full app build
and broad stable suite. Initial build-owner refusals were corrected by sequencing
worker/app builds before their consumers; no guard was weakened. Exact native
restart is covered by the worker suite. Compact replay fields omit pressure and
are not restart checkpoints. The saved review uses its retained baseline worker,
remains HTTP 200, and has not been rebound to the new solver or rerendered.

Evidence: `_private_workspace_artifacts/fire-fluid-momentum-20261010/summary.json`
and `CHECKPOINT.md`, including frozen source/executables and baseline failure.
Next: independently separate source-edge/vertical scalar transport accuracy from
velocity-coupled heat redistribution, then qualify a correction against the same
local heat gate. Gas-to-surface flux, reciprocal energy debit, measured material
response and burning faces remain subsequent gates. No release/package effects.

## Earlier scalar limiter attempt

The 2026-10-10 repair attempt **did not clear local heat convergence**. A sharper
scalar limiter was tested with the same prescribed Fire frames, source geometry,
domain, 3.2-second duration, dt and insulated box. It was rejected as the repair
and removed from the active solver. Burning faces and reciprocal heat exchange
remain disabled. The earlier [surface checkpoint](fire_fluid_surface.md) remains
the active readiness assessment.

## Localized heated transport and directional refinement

The subsequent qualification keeps the minmod production solver, immutable
source amounts, 3.2-second plume, physical source depth and 10% local heat gate.
A separate test module transports a compact `cos^4` hot packet with continuous
heating under `u(t)=0.1+0.1t`, using analytic cell averages and exact characteristic
translation. All three directions give 37.72%, 19.71% and 6.655% L1 errors at
16/32/64 cells. Fixed-grid timestep differences fall from 0.3331% to 0.1661%,
ratio 0.4986. Native and ASan/UBSan results agree exactly. Positivity, peak and
heat/smoke accounting include recorded open-boundary outflow. This fixture
isolates scalar transport; its time-varying velocity is prescribed.

The native replay probe accepts rectangular grids up to 128 cells per axis and
524,288 total cells, within the existing solver admission. The ordinary JSON
worker/package admission is unchanged. Source preparation, field decoding and
fixed physical-volume observations support rectangular dimensions. Comparisons
require matching executable, source-frame identity and geometry plus bound field
hashes; manufactured report tests reject tampering and identity mismatches.

From 32³, refining X alone to 64×32×32 changes source-region heat by 4.3892%;
Y alone gives the same result, and Z alone to 32×32×64 gives 9.2829%. The original
combined 32³→64³ difference remains 16.812%. Individual directional passes are
not a combined-grid pass. A fresh 32³ probe replay is byte-identical to the
retained native baseline. Refined source arrays conservatively restrict to
original inputs within 7e-17 per-cell amounts and 2e-15 per-window budgets.

A second fixture evolves the shear through native momentum viscosity after
initialization: `u_x=0.3*cos(pi*z)*exp(-0.01*pi²*t)`. The localized packet follows
an independent characteristic solution with analytic X/Y cell integrals and
independently checked 16/32-point Gauss Z integration. Heat errors are
17.192%, 5.941% and 2.241% at 16/32/64 X/Z cells; at 64, velocity error is
2.481e-6 m/s. This verifies native evolving viscous shear, not nonlinear buoyant
plume advection. The expanded suite gives 51 native and 51 sanitizer result rows
with exact agreement. The worker/coupling/surface regression suite passes 43
checks; final observation/identity/passive-surface checks pass 15, including one
additional rectangular identity test. A full app rebuild was not repeated:
this slice changes qualification tools/tests only, and solver hashes are intact.

| Direction refined | No box, from 32³ | No box, from 64³ | Insulated box, from 32³ |
| --- | ---: | ---: | ---: |
| X only | 4.389% | 4.050% | 4.394% |
| Y only | 4.389% | 4.050% | 4.394% |
| Z only | 9.283% | 7.389% | 9.244% |

The fine no-box differences on the same 32³ observation partition are 3.494%
(X/Y) and 5.860% (Z). These are measured differences; two pairs with changed
transverse resolution do not establish an asymptotic order.

**The combined-grid gate still fails.** Full twofold refinement in every axis
gives 16.816% local heat for 64×32×32→128×64×64 and its Y counterpart. The
vertically favored 32×32×64→64×64×128 pair gives **15.381%**, or 14.136% on the
fixed 32³ partition. Neither partition clears 10%. Fresh 32³/64³ no-box and
32³ box controls are byte-identical to prior native fields. Rectangular box
runs preserve exactly zero blocked normal velocity and solid heat/smoke.
Actual box heat exchange is still zero; sampled Robin fluxes remain diagnostics.

This rules out a changed source budget or a failure of the tested translation,
viscous-shear, initial-projection and time-refinement ingredients. It does not
identify a single faulty operator in the nonlinear plume. The next repair must
independently isolate localized source/buoyancy/advection interaction and verify
a numerical correction against the same combined-grid local heat gate. Do not
smooth the result by changing physical diffusivity or average away local detail.
Burning faces, reciprocal heat exchange and physical calibration stay disabled
or unqualified. Evidence is retained under
`_private_workspace_artifacts/fire-fluid-directional-20261010/`. About 90 MiB
of native fields and compact proof are retained for the next correction, plus
14 MiB of CFD regression capsules. After consumer checks, 864 MiB of generated
source arrays and four disposable builds were retired. Source hashes, sealed
frames and preparation code remain available for exact regeneration. Fields
omit pressure and are not restart checkpoints.

## Candidate comparison

The candidate used monotonized-centered scalar slopes, Euler time advancement
and unchanged minmod momentum. The MC definition follows the
[Clawpack limiter reference](https://www.clawpack.org/doxygen/amrclaw/2d/inlinelimiter_8f.html).
It was exercised through a compact native probe, not adopted by the JSON worker
or live workbench.

| 32³→64³ observable | Existing minmod | Rejected MC scalar candidate |
| --- | ---: | ---: |
| Local source-region heat, no box | 16.812% | 17.744% |
| Local source-region heat, box | 16.712% | 17.724% |
| Height-averaged profile, no box | 9.620% | 7.897% |
| Height-averaged profile, box | 9.496% | 7.734% |
| Full-field smoke, no box | 9.946% | 9.204% |
| Full-field smoke, box | 9.894% | 9.177% |

All comparisons restrict extensive fields conservatively, use fixed physical
observation regions and retain the original 10% local heat threshold. A larger
grid difference does not prove a larger physical prediction error: the plume has
no exact solution here. It does establish that the proposed repair fails the
chosen readiness criterion.

The candidate improves independent transport fixtures. Smooth translation at
32 cells changes from 1.436% to 0.514% error; at 64 cells from 0.470% to 0.177%.
A compact translated packet changes from 10.468% to 4.927%, remains nonnegative,
conserves heat and has no peak overshoot in the tested fixture. Reconstructed
accepted state with fresh pressure caches recovers bit-exactly, including an
insulated box. Native and sanitizer checks pass. These results do not substitute
for the failed plume gate.

Separate fixed-32³ candidate time refinement gives heat differences
0.2287%→0.1134% and smoke 0.1570%→0.07799% for dt=0.005→0.0025→0.00125 s.
The temporal field gate passes. The weak sampled flux remains sensitive: its
time differences are 5.21%→2.56%. Spatially the box diagnostic peak falls from
0.934 W/m² to 0.0000815 W/m². This is a prescribed coefficient diagnostic,
not actual heat exchange; the native box stays insulated.

## New independent localized projection verification

The active `cfd_open_accuracy_test.c` now verifies a localized 3D temperature
mode, with exact cell-average temperature inputs and an independent continuum
pressure/velocity solution at grid locations. It uses ambient open lateral
pressure, a solid floor and an open top. The mode is
`theta = sin(pi*x) sin(pi*y) sin(pi*z/2)` in a unit cube. From rest, the exact
pressure is `p = -rho*b*kz/(kx²+ky²+kz²) sin(kx*x) sin(ky*y) cos(kz*z)`, where
`b=g*beta`, `kx=ky=pi`, and `kz=pi/2`; projected velocity follows its gradient.
This satisfies the declared pressure boundaries and floor impermeability.

| Grid | Velocity relative L2 error | Pressure relative L2 error |
| --- | ---: | ---: |
| 8³ | 1.9656% | 0.9077% |
| 16³ | 0.4951% | 0.2274% |
| 32³ | 0.1240% | 0.05688% |
| 64³ | 0.03103% | 0.01422% |

Native and ASan/UBSan outputs agree. This isolates the initial localized
buoyancy/projection operation; it does not verify nonlinear momentum advection
or the full time-dependent coupled plume.

The rejected candidate's no-box heat error is concentrated above the injection
layer: the 0.125–0.25 m slab holds 91.879 J on the fine grid and differs locally
by 21.71%; the injection slab holds 59.467 J and differs by 11.77%. This localizes
the next investigation; it is not a proven diagnosis of a single operator bug.

## Evidence and remaining work

`_private_workspace_artifacts/fire-fluid-local-transport-20261010/summary.json`
binds eight probe runs, the unchanged source-frame digest, native invariants and
the candidate decision. The solver, header and normal probe are restored to the
prior accepted hashes. A fresh minmod replay is bit-identical to the retained
32³ baseline. The active change is qualification tooling and the new analytic
projection test, not an adopted solver correction.

The frozen candidate source and `candidate-probe` are retained for reproducing
the rejected experiment. `fire_fluid_accuracy.py --source-input` reuses original
arrays only after matching their producer receipt, frame digest, grid, dimensions
and content hash. The experimental `--scheme muscl_mc_scalar` option requires
that frozen candidate probe; ordinary current probes support minmod. Existing
source arrays, workbench runs and review images were not rewritten.

Next work must isolate spatial transport of localized heated packets and its
interaction with evolving velocity, followed by direction-specific refinement
around the source/plume. Do not infer plume accuracy from smoother averaged
profiles, increase physical diffusivity to make the gate pass, or admit burning
faces from the manufactured tests. Larger full-grid qualification exceeds the
current admitted native/worker grid envelope and needs bounded resource admission
and cost assessment first.

<!-- vertical-reference-current:start -->
### Matched vertical pass and horizontal X refinement — 2026-10-12

The full 128x128x256 run at 2.5 ms flow and 1.25 ms scalar timing completed
all 32 windows (3.2 seconds) with native/strict/independent verification.
Against 128 cubed, local heat differs by 4.705451% and velocity by 3.163460%:
the unchanged 10% directional heat gate passes. The matched 64/128 comparison
still fails at 13.100227%; 64 to the vertical reference differs by 16.646270%.
Heat/smoke accounting, momentum/pressure checks and allocation closure pass.
The reference is retained under
`_private_workspace_artifacts/fire-fluid-matched-timing-20261012`.

The next contained run refines X only to 256x128x256, retaining 128 Y and 256 Z,
original physical source and region, 2.5 ms flow and 1.25 ms scalar timing.
Native and sanitizer source checks recover the vertical source exactly after
X restriction; strict reader restriction/admission checks and prior-profile
preprocessing parity pass. The Mac has 16 GiB RAM, so this bounded first X
comparison avoids the roughly 11 GiB tracked solver footprint of 256 cubed.
Its separate profile caps tracked memory at 8 GiB and scalar work at 22 billion.
A single full-from-zero run has a 24-hour cap and terminal-only strict and
independent analysis; see
`_private_workspace_artifacts/fire-fluid-horizontal-20261012-attempt2/RESULT.md`.
Initial attempt refused the old Cartesian capacity before stepping; the separate
horizontal Cartesian/open capacity profile is repaired and native/sanitizer
large-grid admission tests pass. Failed attempt is preserved; corrected
attempt2 is the sole full run. Horizontal verdict remains pending. Y refinement and full three-axis convergence
remain unqualified. Heat gate stays 10%, momentum CFL stays 0.4.
No surface heating, burning faces, measured calibration, canonical adoption,
version/package/Registry/remote/Desktop refresh or review rebind is included.
<!-- vertical-reference-current:end -->
