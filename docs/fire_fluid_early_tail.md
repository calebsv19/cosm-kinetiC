# Bounded early heat-tail diffusion candidate

## 2026-10-22 — Explicit on/off age intervals repair bounded shutoff/restart histories

New opt-in source_age_intervals module retains source on/off intervals on the
existing scalar clock, including nonzero youngest age after shutoff and positive
interval sums after restart. Existing FCT budgets, incoming-first staging,
actual bounds, grid, source delivery/clocks and10% gate unchanged. Frozen pulse
and restart requested/admitted flux pass56 comparisons. Same four evolving
histories pass48 selected gates;173continuous-control checks and36heater-neighbor
checks also pass. Pulse first-cell at0.00625s improves-17.659% to-0.0001824%;
restart endpoint first-cell-0.0002236%. Tiny discrepancies are numerical
comparisons within unchanged0.5% reference qualification, not precision claims.
14tests strict/ASan-UBSan, full new-history sanitizer parity, delayed-state parity
and five old-route trace comparisons pass. Source transitions/capacity/invalid
ages reject atomically. New route2 is explicit; no default or app adoption.
Qualified fixed-footprint stationary1-D unit on/off heating through0.025s;
next evolving matched transport test before shear selection. No2-D/feedback.
Existing Atlas experiment remains active; no new topology/adoption claim.
Evidence: `_private_workspace_artifacts/fire-fluid-age-interval-repair-20261022/RESULT.md`.

---


## 2026-10-22 — Source-age closure fails shutoff; frozen transport tests pass locally

Bounded same-grid/history comparison: delayed activation reproduces the
continuous case exactly after time shift. Safe generic dispatch after shutoff
has first-cell-17.659% at0.00625s despite passing late endpoint. Restart inherits
that pre-restart failure; no separate new post-restart failure proven. On exact
frozen pulse means, continuous-age closure requested flux is up to99.970% low,
with14/14sampled diffusion faces failing10%; near-heater flux is62.513% high.
Ages are0.003125..0.0046875s but closure infers0..0.000534905s. Thus history
information is needed, not a grid refinement. Frozen continuous transported
shapes at+/-0.025m/s pass local10%: diffusion max4.919%, advection face max2.582%.
This is not an evolving-flow qualification. No general reconstruction selected.
Five histories/eight frozen cases match strict/ASan-UBSan outputs; three affected
fixtures pass both builds, numerical headers unchanged. Source-off metadata
refusal is atomic; forced unsupported closure is labeled diagnostic only.
Next finite-age interval/multi-interval reconstruction, then same pulse/restart
histories before full transport selection. No2-D/feedback/default adoption.
Evidence: `_private_workspace_artifacts/fire-fluid-history-transport-20261022/RESULT.md`.

---


## 2026-10-22 — Source-age reconstruction and incoming/outgoing staging pass the bounded 1-D history

Opt-in source-age profile4 repairs finite-duration heater shape and processes
incoming face transfers before outgoing ones, refreshing unchanged FCT budgets
and actual-cell guards. Grid, clocks, source, bounds and gates unchanged.
All202 reference-qualified measurements across the retained1-D history pass
10%; worst error0.5134%. Band errors at0.0015625/0.0046875/0.025s are
+0.08589%/+0.11966%/+0.00822%; endpoint first-cell+0.05538%. First-interval
requested/admitted flux, initially cold-cell heat, perturbation transport and
invalid-input refusal pass.13 tests strict/ASan-UBSan, full history sanitizer
parity and old profiles0..3 exact trace parity pass. Qualified only stationary
continuously powered unit rectangular-source1-D diffusion through0.025s;
no source-shutoff/advection/2-D/default/feedback claim. Next qualify closure
under changed source histories and transport before prescribed-shear use.
Existing Atlas experiment retained. Minor release recommended; no version edit.
Evidence: `_private_workspace_artifacts/fire-fluid-source-age-staging-20261022/RESULT.md`.

---


## 2026-10-22 — Whole-interval front-flux candidate rejected; two residual mechanisms isolated

Whole-interval diffusion of a fitted positive profile passes24 analytic flux
cases (worst relative error5.15e-11), but unchanged256-cell history still fails
10%: normal-band errors+41.0754% at0.0015625s,+14.1843% at0.0046875s,
-12.6360% at0.025s; endpoint first-cell-15.8200%. First-interval face63/64
requests already overestimate finite-source heat by40.94%/33.51%. Farther cold
face60 requests within0.035%, but combined budget/application blocks nearly
all of it; incoming heat is unavailable to outgoing whole-interval transfer.
Face58 additionally misses representable arrival from initially zero means.
Candidate remains private and rejected; solver source/defaults/limiter,
physical clocks and gates unchanged. Strict/sanitizer frozen/history parity,
bounds and six affected regression executions pass. No2-D or feedback run.
Next finite-duration source-age reconstruction plus consistently staged
incoming/outgoing flux, retaining budgets. Supporting existing Atlas lane.
Evidence: `_private_workspace_artifacts/fire-fluid-spacetime-front-20261022/RESULT.md`.

---


The opt-in `--tail-diffusion` prescribed-shear profile implements a bounded
CN-derived diffusion update with higher-order finite-volume face gradients.
It targets the original 128-strip/256-normal heated-wall case through **0.1 s**.
The source rectangle and strength, prescribed shear, material coefficients,
physical supports, receiver capacities, 6400 Hz native clock, five-tick exchange,
1/51200 s fine scalar clock and 10% physical gate are unchanged.

## 2026-10-22 — Startup arithmetic/source defects isolated; physical repair remains gated

Second-step low/correction cancellation erases positive cold heat; explicit
net-transfer mode fixes that mechanism with unchanged FCT budgets/bound guards.
Four-cell source redistribution discards representable fifth-cell heat; new
opt-in complete-support helper retains it to natural underflow. Legacy paths
remain bitwise unchanged. Both mechanisms pass targeted strict/ASan-UBSan tests.
The physical10% gate still fails: precision-only.025 s band/cell errors
-15.8466%/+149.464%; complete-source band-10.0169%; combination also fails.
Internal time refinement and one-sided front extrapolation do not qualify the
repair; rejected front code is not adopted.12 test programs strict/sanitizer,
four full1-D profile parity checks and512 precision-mode bounded steps pass.
No2-D prefix/full rerun while1-D prerequisites fail; no default/coupled adoption,
shared Thermal change or feedback. Next space/time-consistent positive front
flux candidate on frozen analytic means, then the same short1-D histories.
Supporting existing experiment; no new Atlas lane.
Evidence: `_private_workspace_artifacts/fire-fluid-startup-flux-repair-20261021/RESULT.md`.

---

## 2026-10-21 — Spatial-shear gas-only endpoint passes; startup history fails

Full2112x256 scalar field, prescribedu=.05z/.0625, original grid/clocks/source,
wall exchange off,160 native ticks/.025 s. Explicit source-aware scalar profile
uses positive advection/same-stage diffusion and a joint-age2-D source increment.
Endpoint errors: downstream+7.3700%, transport-0.004309%, normal band-9.7077%,
first normal cell-5.5374%; all pass unchanged10%. Normal sub-bands also pass.
Startup history does not: first reference-qualified normal-band failure at
.0015625 s; downstream+22,258.6% at.01875 s then-66.7398% at.021875 s. Gas-side
normal startup propagation is already wrong before wall/solid; endpoint closure
and passing endpoint are insufficient. Remaining stage/limiter attribution is
unproven. Next short1-D startup face ledger before another full shear trajectory.
Eleven numerical test programs strict/ASan-UBSan and serial clean five-tick
prefix pass; parallel ASan runtime-interceptor leak independently reproduced,
not suppressed. No full-prefix, wall/solid, default/native adoption or feedback
qualification. Existing experiment support; no new Atlas lane.
Evidence: `_private_workspace_artifacts/fire-fluid-shear-gas-20261021/RESULT.md`.

---

## Previous prescribed-flow reduction: advection face repair passes selected gates

The actual unequal2112-cell periodic source-pulse fixture, prescribed u=.025 m/s,
fixed scalar dt1/51200 and original supports exposes a new early-tail failure:
heater-edge method9 alone gives-99.9902% at0.025 s despite passing zero-flow
controls. On analytic cell means, the fine-to-coarse polynomial reconstructs a
negative temperature rise at x=3/32 (face error-406.47%).

Opt-in advection method3 uses the existing positive finite-volume profile fit
for steep/unequal faces, retaining the paired conservative limiter and actual
bounds. Flow-probe method10 combines it with heater-edge method9. Tail errors
become+7.7798% at0.025 s and-1.5395% at0.1 s, passing the fixed10% gate; transport
band and heater-edge cell errors stay below0.04%. Independent reference gate0.5%
is unchanged. No source/time/grid change or state replacement.

Ten test programs pass strict/ASan-UBSan, including196 positive-tail checks;
selected moving/zero-flow histories match across builds and legacy controls
remain exact. This is1-D translation only. Full spatially varying shear,
wall/solid response, earlier faint-history and feedback remain unqualified.
Next use a bounded gas-only shear prefix before a longer trajectory.
Evidence:`_private_workspace_artifacts/fire-fluid-prescribed-gas-20261021/RESULT.md`.

## Earlier heater-edge repair: selected 1-D gates pass

Explicit probe method 9 fits a finite-duration heated-step gradient at declared
source jumps, including post-heating spreading within diffusion stages. It uses
local means, source power and diffusivity, then passes the requested flux through
the unchanged conservative limiter. Source energy, grid, clocks, actual bounds,
previous source/time/quadrature repairs and 10% gate remain fixed.

52 independent analytic cases pass; the same0.025 s history gives normal first
cell -5.6004%, normal band -9.7642%, uniform/unequal horizontal +1.7623%/+1.6921%.
All selected0.1 s controls pass. The normal-band margin is only0.2358 percentage
points. A continuously heated profile that ignored stage spreading was rejected.

Earliest startup improves: first cold normal-cell error after four ticks falls
+31.25% to +0.67%. Later profiles often leave the admissible model family and use
the existing fallback; earlier faint remote tails and the whole history remain
unqualified. No wall/solid result follows from these gas-only diffusion probes.

Fresh strict/ASan-UBSan nine test programs,52 analytic and five history parity
checks pass; all five old method8 controls remain exact. Method9 is1-D only;
no native prescribed-flow/coupled integration or feedback. Next test its
applicability under prescribed advection before wall/solid attribution.
Evidence:`_private_workspace_artifacts/fire-fluid-heater-edge-20261021/RESULT.md`.

## Earlier normal-band flux investigation and fit quadrature repair

The same source-aware 1-D grid/clocks isolate the normal-band deficit to gas
heat distribution. Requested/admitted boundary transfer is 1.0 with no limiting;
its heat ledger closes to 1.34e-15 relative. On analytic cell means the late band
face gradient is accurate, but the heater-edge fit is 26–42% low during early
startup. A smooth reconstruction across the discontinuous source remains the
next bounded repair target; no new wall/solid error is established.

A separate demonstrated arithmetic defect is fixed in `physics_sim_tail_fit.h`:
bounded composite Gauss integration resolves steep exponential variation. The
existing limiter, bounds, source interval, time operator, grid and 10% gate stay
fixed. At 0.025 s the normal band moves -10.9626% to **-10.9465%, still failing**;
normal first cell -7.6085%, uniform/unequal horizontal -2.2105%/-2.2779% pass.
The 0.1 s gas-only controls pass. No entire early-history qualification.

Fresh strict/ASan-UBSan builds, 436 independent moment/fit checks, seven affected
source/operator tests and exact five-history parity pass. No new coupled run,
default adoption, shared Thermal change or feedback. Evidence:
`_private_workspace_artifacts/fire-fluid-normal-band-20261021/RESULT.md`.

## Earlier source-interval repair: experimental 1-D only

Probe method 8 integrates conduction of newly generated source heat during each
original scalar tick, rather than endpoint half-kicks. Existing-field SSPRK(10,4),
reconstruction, paired limiter, bounds, physical supports/clocks and 10% gate stay
fixed. No reference-state replacement or heater-footprint expansion occurs.

At 0.025 s normal wall-cell error improves -70.3916% to -7.6798%; uniform and
unequal horizontal errors improve to -2.2105%/-2.3479%. The normal band improves
-30.9033% to **-10.9626% and still fails**. All 0.1 s gas-only controls pass.
The old first cold normal-cell deficit starts on source tick one (-97.8408%);
source integration removes that initial deficit, while later normal transport
still develops error. This does not qualify an entire early history or wall/solid
response. Thin-front reconstruction controls were rejected.

Fresh strict/ASan-UBSan source, bounds, conservation and six affected operator
regressions pass; the three 0.025 s exports match exactly across builds. Method 8
is 1-D only; no new native receiver run or feedback. Source edges require four
locally uniform cells per side and a <=1e-12 omitted-tail guard. Next localize the
remaining normal-band transport deficit on the same source-aware field.
Evidence: `_private_workspace_artifacts/fire-fluid-startup-deficit-20261021`.

## Earlier gas time update: 1-D only

The explicit experimental`--tail-time`option requires`--tail-diffusion`and uses
SSPRK(10,4), with the same reconstruction and bounded paired FCT update at every
stage. It removes the two-point implicit intermediate prediction. Outer scalar,
native, source and exchange clocks, physical grid and10%gate stay fixed.

At0.025s the normal first-cell/band errors are-70.39%/-30.90%, and unequal x tail
-26.54%: all fail. Increasing internal stage evaluations10→40 changes signals by
<0.45%, so further temporal work does not justify a new coupled trajectory.
At0.1s the gas-only normal cell and unequal x controls pass(-8.42%/-6.32%); this
is not inherited coupled qualification. The remaining gas-only deficit requires
spatial reconstruction/limiter/sharp-source startup localization at the same grid.

Strict/sanitizer modal, exact bounds, cold/subnormal and conservation tests pass;
all three .025s1-D exports match exactly. The new native hooks compile, and the
affected regressions pass, but no new receiver run or feedback was performed.
Detailed evidence:`_private_workspace_artifacts/fire-fluid-gas-time-20261021`.
[Method coefficients](https://faculty.washington.edu/rjl/students/ketcheson/thesis.pdf#page=112).

## Earlier coupled revision: 0.025 s steep-face repair

Positive finite-volume fits now also cover steep regular faces, while the
paired limiter, updated-cell bounds and cold-cell heat path remain unchanged.
Analytic normal-face errors improve from345–407% to <=0.327%. The unchanged-grid
live run reaches0.025 s, but gas tail remains1002624x the qualified reference and
wall Q52.606x reference: both fail10%. Mixed two-point prediction/higher-order
flux timing is a demonstrated remaining gas error; wall timing also needs a
separate accurate-gas check. This revision does not inherit the earlier coupled
0.1 s pass below; its repeated1-D0.1 s controls pass.

Actual-Q solid replay passes255/256 histories; one below-binary64 channel and26
subnormal point failures remain. A first tick55 joint refusal was not reproduced
by the diagnostic replay and remains unattributed. Strict/sanitizer operator
checks and affected regression pass; no full sanitized coupled run was added.
No feedback or default adoption. Detailed measured result and exact retained
reproduction are under`_private_workspace_artifacts/fire-fluid-steep-tail-20261021`.

## Earlier revision: what the 0.1 s comparison established

For the earlier revision, at 0.1 s a complete strict live-receiver run on frozen Thermal/core 0.3.3 gives:

| Quantity | Previous error | Candidate error | Fixed gate |
| --- | ---: | ---: | --- |
| Downstream gas-tail temperature | +429.7067% | −1.1662% | 10%, pass |
| Total wall-delivered heat | at least +491.2095% | −2.6187% to −2.6218% | 10%, pass |

The tail reference and wall reference interval are independently qualified at
this time. No new reference grid was run. These are two selected **point** gates,
not a complete history, all-patch or earlier-time qualification. The 0.025 s
point still fails, despite a large improvement. The prior 0.5 s aggregate gates
belong to the previous selected profile and have not been inherited by this
changed numerical method.

The positive two-point backward-Euler diffusion solve conserved energy but
produced heavier leading tails than the continuous heat kernel. One-dimensional
controls separated that spatial error from time integration and source sampling:
quartering the timestep left approximately +376% wall-cell error at the original
normal spacing, while a CN-derived bounded higher-order update reduced it to
−3.79%. On the actual unequal x mesh, the candidate's local positive-profile
reconstruction reduced diffusion-only tail error to +1.85%. These are operator
reductions, distinct from the coupled point gates above.

## Implementation and limits

`src/tools/physics_sim_tail_diffusion.h` supplies the candidate diffusion step.
It reconstructs a sixth-order gradient on regular cells; steep regular and unequal-cell faces use
finite-volume cubic weights and local log-quadratic/exponential profiles where
needed for steep positive tails. Those profiles are fitted to current cell
means and widths, never to a reference solution. A reflected normal stencil
preserves the closed normal boundary. Wrong-sign/zero fits retain two-cell
transport so cold cells can still receive heat.

The two-point implicit half-step matrix supplies a predicted intermediate state.
Higher-order gradients use the old and predicted fields. This mixes numerical
operators and leaves a temporal tail error at steep profiles; it is not a
fully matched higher-order CN solve. Low and corrected
fluxes are applied as paired extensive transfers with the existing FCT
headroom factors. Actual updated donor and receiver temperatures are checked;
transfer is reduced to available headroom/representable values rather than
clipping a cell temperature. This repairs a one-quantum upper-bound refusal on
a subnormal plateau. Widths are owned by the candidate workspace, avoiding a
temporary-array lifetime error found in the first coupled attempt.

Uniform source heat is split symmetrically around each scalar step. The
independently isolated source-sampling bias at 0.1 s was only +0.30% normal and
+1.14% horizontal, so source timing alone did not explain the old excess tail.
The advection candidate, wall-exchange law and Thermal solver are unchanged.

The profile is deliberately limited to 0.025/0.05/0.1 s runs and refuses longer runs.
It is not combined with the old durable checkpoint build: old checkpoints and
tools retain their exact engine identity. Coarse handoff, source-off, longer
histories, all local physical patches, evolving-flow feedback and ignition
require separate qualification. The installed app/default solver is unchanged.

## Reproduction

From the PhysicsSim Main Edit root, select fresh output `FRESH_BUILD` and pinned
frozen input paths; no clean target is used:

```sh
python3 scripts/build_thermal_transport.py --tail-diffusion --live-receiver \
  --columns --localized-startup --local-micro-factor 2 --solid-trace \
  --repaired-scalars --shear-normal256 --core-version 0.3.3 \
  --thermal FROZEN_THERMAL --shared FROZEN_SHARED --output FRESH_BUILD
FRESH_BUILD/probe 128 256 5 640 10 NEW_PACKETS ABSOLUTE_RECEIVER_WORKER
```

Capture stdout observations and stderr execution receipts into fresh files.
`tests/cfd_tail_diffusion_test.c` checks polynomial and steep exponential cell-mean gradients,
conservation, exact-zero/cold/subnormal transfer, owned width lifetime and
non-mutating invalid-step refusal. The dedicated diagnostic
`src/tools/physics_sim_early_tail_probe.c` isolates temporal/spatial reductions.
`scripts/fire_fluid_early_tail.py EVIDENCE_ROOT` audits completed coupled exports
against retained qualified references and the independent supplied-Q solid
recurrence. Exact commands, outcomes and retained identities are in the private
result `_private_workspace_artifacts/fire-fluid-early-tail-20261021/RESULT.md`.
