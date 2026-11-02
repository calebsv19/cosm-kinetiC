## 2026-10-14 — Scalar transport diagnosis and bounded improvement

Exact-reference thin-column tests identify excessive scalar spatial spreading
before buoyancy or obstacle effects: temporal refinement barely changes the
error. An explicit scalar-only MC reconstruction improves all 14 controlled
comparisons; the 128-cell physical-diffusion case passes the unchanged 10%
profile/history gates (8.55%/7.46%), while 32/64 remain inadequate. A fitted
delay does not reconcile the original boxed traces (44.60% residual).
Default minmod and momentum remain unchanged; MC requires separate coupled
qualification. Soft-stopped probes now preserve accepted fields and numerical
closure while retaining an explicit incomplete verdict.
[Methods, evidence and limits](fire_fluid_local_transport.md).

## 2026-10-13 — Bounded forcing verdict: not qualified

**Matched physical sampling/transfer support passes; current boxed-plume thermal
forcing is not qualified.** The 64³ replay hit its 1,200 s wall cap, retaining
observations through 3.175 s but no 3.2 s final field or numerical-closure record.
The strict full report refuses this incomplete run. No automatic rerun or larger
grid was launched.

The post-cap matched-prefix diagnostic compares both grids only through 3.175 s.
Mean support temperature rises there are 0.001145575662 K (32³) and
0.000003948032 K (64³), a ratio of about 290. The relative L1 differences are
45,309.97% over the face history and 28,916.37% at the last common observation;
held-response heating demand differs 57,969.89%. These are plainly outside the
unchanged 10% gate, but remain **prefix diagnostics**, not a completed 3.2 s
spatial qualification or a substitute for the missing final local-heat norm.

Independent reciprocal frozen-field checks pass for uniform 32³/64³ and the
completed current 32³ plume field. Matched uniform-bath transfer at 25 ms differs
by only 4.9519e-9 J between grids; the 64³ actual plume response remains untested
because its final field is unavailable. The 32³ dense cadence result below
passes 10→5 ms at 3.263%; no 64³ cadence admission is inferred.

Next boundary: isolate source-to-face transport and arrival-time sensitivity
with bounded diagnostics before choosing a more expensive spatial reference.
This result does not identify the cause of the forcing discrepancy, admit live
coupling, or justify relaxing the gate. Selected proof:
`_private_workspace_artifacts/fire-fluid-fixed-support-20261013/bounded-summary.json`;
`dense-cadence.json` and `RESULT.md` in the same root.

## 2026-10-13 — Matched physical sampling and reciprocal transfer support

The development face module now accepts an explicit physical support depth.
The selected lower face spans x/y `[0.375, 0.625]` m; its gas slab spans
z `[0.34375, 0.375]` m on both 32³ and 64³ grids. Area is 0.0625 m²,
depth is 1/32 m, volume is 0.001953125 m³, and gas capacity is
2.35546875 J/K. The synthetic whole-face conductance is 0.625 W/K and
surface capacity is 100 J/K. These values were fixed before the run results.

Sampling uses gas energy divided by that common physical capacity. Transfer
uses the same volume weights to distribute conductance; subdivision preserves
total conductance, capacity and footprint. Unaligned or out-of-domain supports
refuse atomically. The original adjacent-cell wrapper remains available but
does not imply matched physical support across grids. This is a synthetic
uniform-volume Robin model, not a calibrated wall-convection or wall-gradient
model.

All six face orientations at 32³/64³ pass affine sampling, an independent
aggregate two-capacity thermal solution, partition invariance and energy
closure, natively and with ASan/UBSan. Maximum analytic temperature error is
4.43e-11 K. Existing single-layer heating/cooling, refusal, weak-signal and
subsequent gas-step checks also pass. Common 8×8 observation tiles use
conservative volume restriction; temperature rise and transferred energy are
compared, never absolute temperature near 300 K.

The old retained boxed fields predate the current conservative momentum-flux
repair, so they cannot qualify the current forcing. A bounded current-operator
32³/64³ replay uses the original source frames (matching under restriction to
1.30e-16 relative), 2.5 ms flow, 1.25 ms scalar steps and a 3.2 s horizon.
The CLI admits only these two grids and has a 1 GiB tracked allocation cap.
The 64³ run has a 1,200 s wall limit. No 128³ or larger replay is part of this
qualification.

A separate cheap 32³ observation replay at 5 ms has a bit-identical final field
and identical common 25 ms observations. Prescribed-response demand changes
16.185% at 50→25 ms, 12.615% at 25→5 ms and 3.263% at 10→5 ms. Only the
last pair passes the unchanged 10% cadence gate. This diagnostic integrates
a held prescribed gas history; it does not debit the evolving gas trajectory.
Its cadence result applies to 32³ only. Reciprocal frozen-field exchange is
checked separately, and neither check alone qualifies a live coupled host.

Reproduce the contained tools and tests:

```sh
make BUILD_DIR=build/<fresh-proof> box-forcing-probe heated-face-probe test-heated-face-support-native test-heated-face-support-sanitize test-heated-face-native test-heated-face-sanitize
PYTHONPATH=scripts:tests <numpy-python> -B -m unittest test_boxed_face_forcing test_heated_face_reference test_passive_surface test_horizontal_reference_report test_matched_vertical_report test_vertical_reference_report -v
<numpy-python> -B scripts/fire_fluid_box_forcing_report.py --root <fixed-support-evidence> --thermal-probe <frozen-thermal-probe> --output <fresh-report-output>
```

Evidence root: `_private_workspace_artifacts/fire-fluid-fixed-support-20261013/`.
The older one-cell and historical-field experiments below remain separate.

## 2026-10-13 — Conservative non-burning heated-face prototype

A separate development-only native module, `src/tools/physics_sim_heated_face.c`,
now exchanges heat between one isothermal box face and its directly adjacent gas
cells. The box remains impermeable; other faces remain insulated. Synthetic
constant `h=10 W/m²/K` and whole-face `C=100 J/K` are authored inputs, not measured
material properties. No radiation, ignition, pyrolysis, inter-face spread or
multiple burning objects are implemented.

Each cell/face pair uses the exact two-capacity exponential solution. Forward
and reverse half sweeps form a symmetric split for multiple cells. Gas debit
and surface gain are equal; a separate signed cumulative ledger preserves the
meaning of native `input_j` as source injection. A retained low temperature part
prevents sub-ulp weak-plume heat increments disappearing when added to 300 K.
Both participants remain unchanged on refusal, including allocation and late
contrast failures. The caller owns exchange cadence and must retain the complete
face state, including its correction and ledger, when replaying. This module
is not linked into the ordinary desktop, worker, serialized scene or joint host.

Prescribed uniform 310 K initial gas around a 0.25 m box heats its lower face
from 300 K to **300.133626568 K** in 3.2 s, transferring **13.362656821 J**.
This is a finite gas bath that cools, not a permanently clamped 310 K reservoir.
The existing clamped prescribed-input model remains separately qualified.
An independent capacity-weighted matrix exponential checks every gas cell and
the face. At exchange dt=0.1/0.05/0.025 s, surface errors are approximately
5.70e-8/1.43e-8/3.57e-9 K and gas maximum errors 2.54e-6/6.34e-7/1.59e-7 K.
The surface stored-energy/transfer discrepancy at the finest cadence is 2.67e-14 J.
Twelve one-cell heating/cooling cases across all six orientations agree with the
analytic temperature to displayed double precision; gas balance is within
3.56e-15 J. Weak-signal, exact replay, locality, zero transfer, refusal and ordinary
subsequent gas-step checks pass natively and under ASan/UBSan.

The second experiment initializes gas from the hash-verified retained boxed-plume
32³/64³ final fields and evolves **thermal exchange only**, with transport frozen.
At dt=0.025 s, sampled adjacent-layer mean temperatures are 300.000911785 K and
300.000000140 K. Transfers are 0.00121838739 J and 1.33870419e-7 J. Both numerical
responses agree with the independent oracle and close their energy ledgers.
They are not a matched spatial-convergence pair: a one-cell gas layer has a
different physical thickness and capacity on the two grids. Their very weak,
disparate signals cannot qualify real evolving box heating.

**Stop boundary reached:** the missing prerequisite is qualified boxed-face
thermal forcing with matched physical observation/transfer support and cadence.
The earlier fixed-volume box diagnostic already failed spatial convergence;
the new no-box directional passes do not repair it. The 10% local heat gate
remains unchanged. A live two-way plume/face host, joint transactional checkpoint,
physical heat-transfer law/calibration and reliable face flux remain unqualified.
This is a repeatable numerical heated-face prototype, not a qualified evolving
heated-box or burning-object demonstration.

Reproduce in a fresh checkout-owned build root:

```sh
make BUILD_DIR=build/<fresh-proof> heated-face-probe test-heated-face-native test-heated-face-sanitize test-open-xy-parity
PYTHONPATH=scripts:tests <numpy-python> -B -m unittest test_heated_face_reference test_passive_surface test_horizontal_reference_report test_matched_vertical_report test_vertical_reference_report -v
<numpy-python> -B scripts/fire_fluid_heated_face_qualification.py --probe build/<fresh-proof>/heated-face/probe --accuracy-root <retained-accuracy-root> --surface-evidence <retained-surface-root>/summary.json --output <fresh-output>
```

Selected result: `_private_workspace_artifacts/fire-fluid-heated-face-20261013/qualified/summary.json`.
The root also retains the source-bound probe, compact source snapshot, tests,
reconciliation and fresh directional verdicts for independent reproduction.
The older dated prescribed-only checkpoint below remains historical.

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

## 2026-10-14: completed matched-flow boxed transport isolation

A full same-MC 32³/64³ scalar comparison driven by one native 32³ velocity
solution completes 3.2 s in 191.30 s, using 87.13 MB tracked numerical memory.
Coarse replay matches native heat bitwise at every step; source restriction,
face flux, wall impermeability, divergence and heat closure pass. The original
local-heat metric passes at 8.2459%, but face-history error is 110,514,775% and
forcing remains unqualified at the unchanged 10% gate. Final face mean rises
are 2.2132e-7 / 3.0131e-13 K: an unresolved weak leading tail, not runaway
heat creation. Fine 0.01/0.005 s response cadence also fails at 11.3836%.

Error begins at the sharp source top, before wall interaction: a zero-velocity
two-step test reproduces the early factor-two diffusive leakage difference,
with identical minmod/MC results and zero leakage when conductivity is zero.
Over the full history, band disagreement first exceeds 10% at z=0.21875–0.25 m
(12.644%), then grows sharply toward the box. Matched flow does not eliminate
scalar transport error; it does not qualify fine momentum or physical convection.
No further numerical formula/default change was made. The next bounded test is
a sharp-source thin-column transient reference with separate spatial and
scalar/source-time refinement. Evidence: workspace
`_private_workspace_artifacts/fire-fluid-matched-box-20261014/RESULT.md`.

## 2026-10-14: sharp-source diffusion and scalar time integration

Independent continuum heat-kernel and exact grid-time references separate source
time error from spatial diffusion error in columns of at most4096 cells. At
2.5 ms with two scalar steps, Euler time-only error is about50%; opt-in
`scalar_rk2` reduces this to0.00184%/0.00736% on32/64 cells. Spatial error remains
99.01%/98.03%: the sharp initial gradient is unresolved. At3.2 s, spatial errors
are67.66%,40.82%,15.49%,4.20% for32/64/128/256 axial cells; only256 passes the
unchanged10% outside-source profile gate. A bounded local0.122 mm zoom passes
the early-time gate at5.225%, without a large cubic grid.

SSPRK2 applies two native scalar/source stages with matched half-weight boundary
receipts and doubled work accounting, reusing projection scratch. Default Euler
is preserved; momentum and time-varying coupled forcing are not made second
order. Native/sanitizer inflow/outflow, heat/smoke, bounds, box replay and budget
tests pass, as do native accuracy/lateral/atmosphere regressions and independent
reference tests. No boxed thermal-forcing qualification or ordinary app/worker
activation is claimed. The remaining leading-tail error needs a bounded sharp-
source advection–diffusion reference next. Evidence:
`_private_workspace_artifacts/fire-fluid-sharp-diffusion-20261014/RESULT.md`.

## 2026-10-14: sharp-source advection–diffusion leading-tail diagnosis

Twenty bounded prescribed-flow trajectories use corrected scalar SSPRK2 timing
and a translated heat-kernel reference. Bulk accuracy does not qualify the
leading tail: at3.906 mm, MC main-profile error is0.1713% but far-history error
is99.996%. Halved dt and a matched-spacing outer-domain check exclude those
causes. Native exact-profile flux checks locate spatial bias before accumulated
trajectory error: MC underestimates tail-plane flux16.73%, minmod overestimates
14.40% at that spacing. No new flux formula or physical parameter was tuned.

Targeted thin-column refinement (maximum16,384 cells, no cubic run) reduces
far-history error to29.73% at0.977 mm and6.955% at0.488 mm. The finest early
prefront history still fails the unchanged10% gate at10.268%; overall transport
and boxed forcing remain unqualified. Finest-column time refinement is a next
bounded check. Extended capacity exists only under the thin-column diagnostic
compile flag; ordinary grid limits are unchanged. Native/sanitizer, reference,
metadata-refusal and narrow-capacity tests pass. Evidence:
`_private_workspace_artifacts/fire-fluid-sharp-transport-20261014/RESULT.md`.

## 2026-10-16 coupled prescribed-flow receiver qualification

The repaired core_thermal0.3.2 receiver is linked into a separate PhysicsSim
headless probe with native gas transport and reciprocal fixed-slab heat transfer.
Both25/12.5mswindows pass six unchanged10%response gates against an independent
central-FV/CN coupled reference. At12.5ms:near0.16936%,far6.76419%,early9.81341%,
solid cells1.62343%/1.97491%,Qhistory1.62757%. Final reference refinement changes
early0.05656%; source-only zero-exchange controls agree. Native gas/solid ledgers
close,velocity remains prescribed,and staged failures do not publish.64cell small
ASan/UBSan and12+12legacyface/support regressions pass. Native16,384cell runs take
94.25/108.30s with16.12MBtracked memory. No hugegrid or solver-tolerance change.

This is an authored nonlocal exchange slab,not an impermeable wall,boxed-flow or
resolved-convection qualification. Smaller windows improve solid response but do
not cure upstream tail transport; early error is still close to the gate. Optional
source-linked diagnostic only; no app/worker adoption,pins,Desktop/shared/Thermal
mutation or release. Evidence:
`_private_workspace_artifacts/fire-fluid-thermal-transport-20261016/RESULT.md`.
Next: bounded matched stationary-wall/box geometry and prescribed-flow qualification.

## 2026-10-16 stationary planar-wall checkpoint

A new optional qualifier reuses native scalar transport,conservative face exchange,
Thermal receiver/core_thermal0.3.2 and exact clocks. Nativeground haszero normalflow;
linear tangentialshear is prescribed. Gas/solidhalf-cell plus explicitcontact
resistances replace deep-slab exchange for thiswalltest. Uniform along-wall source
isolates NORMALdiffusion/contact response,not thermal advection around an obstacle.

All128/256normal-grid,6.25/3.125msexchange cases pass six unchanged10%gates. Best
selected256cell/3.125mscase:earlywall0.016218%,nearwall0.082443%,outerband0.027994%,
solid0.599483%/0.601852%,Q0.599531%. Fixedscalarstep1/3200s atbothgrids separates
scalar timing from spatial refinement. Refined normal cells require exchange-cadence
qualification:atfixedwindow soliderror rises to1.88%,then falls to0.60%onwindowhalving.
The earlier advective-tail9.813%result is not superseded by thisdifferentexperiment.

IndependentFV/CN and insulatedcosine references,energy/atomicity checks,smallASan,
legacy slab/face/support and8Pythonregressions pass. Maximum4096nativecells,1.05MB
trackedmemory;three fullruns total28.52s. No shared/Thermal/ordinaryruntime edits,
Desktop,release or broadgridrun. Evidence:
`_private_workspace_artifacts/fire-fluid-stationary-wall-20261016/RESULT.md`.
Next bounded boundary:an along-wall advected thermal patch with matched prescribed
flow before corners/box/plume qualification.

## 2026-10-17 — Along-wall advected patch isolates the leading-tail error

Completed five bounded native pulse trajectories (maximum 32,768 gas cells).
With wall exchange off, doubling along-wall resolution from 32 to 64 cells reduces
early downstream-tail error from 36.516% to 7.401%. The matched coupled case has
the same 7.401% error. The selected 64 × 4 × 128 / 3.125 ms case passes all nine
unchanged 10% gates: early tail 7.395%, near-wall history 0.343%, Q 0.187%, solid
0.188% / 0.131%. At fixed grid, shortening exchange from 6.25 to 3.125 ms reduces
Q error from 0.524% to 0.187% without changing early tail. Error begins in gas
transport; interface timing adds a smaller error. Exact-native-Q analytic solid
replay differs by only 0.00116% / 0.04798%. No new solver formula fix justified.

Independent Fourier/FV/CN reference refinement (maximum 0.02887%), pulse
superposition, strict/sanitizer checks, bit-exact previous wall smoke and eight
sharp-source regressions pass. The coarse failure is retained, not waived.
This wall has one uniform face temperature and two through-thickness solid cells;
local solid hotspots, lateral conduction, box corners and feedback remain unproven.
No shared/Thermal/ordinary-runtime edits or release. Release heuristic: minor for
optional diagnostic workflow; no version change. Existing Fire–Fluid parent only;
Atlas considered, no new topology/plan. No memory write.
Evidence: `_private_workspace_artifacts/fire-fluid-thermal-patch-20261017/RESULT.md`.
Next: matched local wall patches/solid columns before box or buoyancy coupling.

## 2026-10-17 — Local wall strips and independent solid columns

Completed the bounded local-mapping step; qualification is PARTIAL, with the 10%
gate unchanged. Each gas x-strip exchanges only with its own area-scaled native
Thermal two-cell column. Whole-batch rollback and wrong-recipient binding checks
pass. Existing whole-face behavior remains bit-exact in its previous patch smoke.
No shared/Thermal numerical changes or ordinary worker/UI linkage changes.

All 25 hotspot/combined-profile/selected-patch gates pass in four trajectories.
The selected 64 × 4 × 128 / 3.125 ms case gives local-Q spatial/history error
0.505%, solid 0.506% / 0.568%, hottest-patch history 0.378%, and early gas tail
7.395%. The final hottest receiving-layer patch is x[0.0625,0.09375] m; the deeper
layer remains hottest over x[0.03125,0.0625] m, both matching the reference.
The hottest patch rise is 4.341 times the wall average; 94.86% of heat
is in the source-footprint and first downstream patches. Uniform-wall temperature
therefore concealed a meaningful hotspot despite correct total heat.

Individual weak-patch checks prevent an aggregate-pass overclaim. An added
128 × 4 × 64 directional check reduces early gas tail to 5.045% and upstream
local-Q error to 2.497%, but downstream patch x[0.125,0.15625] m remains 13.554%
(outside 10%). The very faint periodic-wrap patch also fails; two extremely faint
patches lack a sufficiently converged individual reference and are unqualified.
No ratio waiver or huge-grid escalation. Remaining error is already present in
local heat delivery before solid response. Exact-packet solid replay remains
about 0.00116% / 0.048% in combined histories; x-resolution sensitivity points
toward weak gas/source-edge transport or local sampling, not a new solid defect.

Four native runs used at most 32,768 gas cells, 260.91 s total, and
19.27 MB tracked allocation. Strict/small ASan/UBSan,
local mapping/rollback, 12+12 legacy face/support, slab, prior-patch parity, and
eight sharp-source regressions pass. Reference hotspot/local-primary refinement
max 0.235%; per-patch reference limitations are explicit in report.json.
Evidence: `_private_workspace_artifacts/fire-fluid-thermal-columns-20261017/RESULT.md`.
No commit, version bump, release or Desktop refresh. Release heuristic recommends
minor for the optional diagnostic API/workflow. Existing Fire–Fluid parent only;
Atlas considered, no new plan/topology/adoption. No memory write.
Next bounded boundary: exact-profile weak-edge local flux/sampling controls before
expanding geometry or introducing lateral solid conduction/feedback.

## 2026-10-17 — Frozen reference-field weak-edge isolation

Completed 32 one-shot probes on independently generated reference snapshots at
1.5 and 3.2 s; no new coupled trajectories. Native scalar fluxes are measured on
the same reference cell averages, separating spatial operator error from evolved
state error. Flux extraction agrees with the actual native Euler energy change
within 1.77e-11 relative. The column exchange kernel agrees with the independent
frozen two-capacity solution within 0.0000041%. At 3.2 s, weak-downstream wall
sampling error is 0.637% on the 128-normal-cell grid.

The gas operator still fails the unchanged 10% gate on reference input. At fixed
128 normal cells, doubling x resolution 64→128 reduces source-height downstream
flux error at 1.5 s from 30.077% to 10.319%, near-wall periodic-wrap flux error
from 92.104% to 19.279%, and source-height wrap flux at 3.2 s from 65.541% to
11.006%. These references pass the separate 0.5% refinement check. Individual
unconverged faint reference quantities retain null gates, not fabricated passes.
Minmod does not generally fix the failures and worsens several. Advection and
central-diffusion component errors can cancel or amplify in the small net flux.
No physical coefficient, solver formula/default, timestep or acceptance change.

Strict/sanitizer largest-state parity, prior reference-history bit parity,
column mapping, legacy 12 face + 12 support checks and eight Python reference
regressions pass. Maximum one-shot state 65,536 cells, 8.47 MB tracked; all 32
native probes total 0.67 s, four reference exports total 57.33 s. No negative gas
profile clipping was needed. Prior whole-trajectory weak-tail failures remain.
Evidence: `_private_workspace_artifacts/fire-fluid-thermal-edge-20261017/RESULT.md`.
Next: bounded consistent advection/diffusion flux reconstruction candidate on
these frozen fields, requiring positivity/conservation before a trajectory test.
Existing Fire–Fluid parent; Atlas considered, no new topology/adoption. No memory
write, shared/Thermal edit, commit, version bump, release or Desktop refresh.
Release-impact heuristic recommends minor for optional diagnostic workflow only.
