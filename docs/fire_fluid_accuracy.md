## 2026-10-20 — Implemented wall-refined normal solver

The named `wall-refined` headless profile now uses 512 wall-normal cells,
1/6400 s exchange, original 1/51200 s scalar time, centered normal/wall
composition and conservative Crank–Nicolson normal diffusion. Constant-mode
centering and compensated increments remove a reproducible uniform-field
roundoff drift without changing the 21 K fixture bound. Native wall/receiver,
physical parameters, original FCT/0.4 K cap and 10% gates remain unchanged.

Short reversal gas point error falls 25.32% -> 3.67% on unchanged supports,
3.83% on actual fine-cell supports. All declared five-second warm/off/reversal
history/final/point/arrival/source-off/control/energy checks pass independent
reference qualification. Warm transfer 14.3608 J; solid front/back rises
0.257101/0.0301152 K; gas mean cools 2.00548 K after source-off. Exchange-off
is exact; reversal transfers 13.0754 J from solid to gas. Extra signed-change
screens near 320 K zero crossings retain point failures; no blanket faint-tail
qualification follows. The authored two-cell solid model remains unchanged.

Build the contained warm-solid tool with explicit frozen Thermal 0.3.3 inputs,
then run `probe <case:0|1|2> <fresh-packets> wall-refined`. Original three-argument
256-cell behavior is retained for comparison. The new normal theta-method
requires its matching implicit matrix timestep; the selected CN profile refuses
outside its explicit positivity condition. No app/worker default or live Thermal
integration. Next implementation is joint headless adoption and restart/rollback
proof, not another large-grid qualification run.

## 2026-10-20 — Fixed-grid20ms reversal timing isolation

A bounded3x3native/sanitizer matrix holds256normal cells and all physical inputs
fixed, changing scalar dt1/51200→1/102400→1/204800s independently of exchange
5→2→1receiversteps (receiver dt1/6400s unchanged). Worst startup point error:
25.3205%baseline,25.0948%scalar-only4x,16.4977%exchange-only,16.1927%both.
Allnine physical point gates still fail10%; fewer or smaller failures are not
acceptance. Old observations/signed-Q prefix reproduce exactly through20ms.

Time-refined same-grid CNitself has14.2548%worst spatial error. Native versus
same-grid CN falls10.4945%→2.08287%, so exchange cadence/splitting dominates the
extra timing error; finer scalar time alone is weak. Max location shifts onecell;
originalcell6combined physical error15.7802%. Independent short reference
normal/time checks qualify all assessed samples within0.044%; exact supplied-Q
solid audit max1.42e-12J. Local gas distribution remains the failing output.

The existing warm-solid probe optionally accepts
`2 <fresh-packet-path> <scalar-factor:1|2|4> <exchange-receiver-steps:1|2|5>`
and stops at0.02s; its original three-argument five-second defaults remain.
No normal refinement, numerical method change, gate relaxation, shared/live/
default adoption or old faint-arrival waiver. Broadapp/package/5s reruns skipped
for these short diagnostic controls. Next candidate is a short normal-spatial
comparison at1/6400s exchange and original scalar time, before live integration.

## 2026-10-20 — Warm-gas / two-cell solid demonstration

A separate non-burning5s uniform-transverse fixture uses the native local wall
law and frozen repaired Thermal0.3.3 receiver. Warm gas320K/cool solid300K,
1K/s heater0–1s, prescribed shear,256normal cells. Delivered14.3582J;
front/back rises0.257057/0.0301075K; gas mean cools2.00538Kfrom1to5s.
Independent normal/time-refined history/final/arrival and conservation/control
criteria pass unchanged10%/0.5%gates. Exchange-off gives exact zeroQ/solid rise;
reversal returns13.0731Jto gas. Given-Qsolid audit passes to1.77e-11J.

This is not an overall all-point accuracy pass. Seven reversal gas samples at
10–20ms fail10%, worst25.3205%at1.587mm. Short refined reference qualifies the
failure; same256normal CN retains13.4179%spatial error and native differs from
that same-grid result10.4945%, leaving exchange/splitting versus scalar timing
to isolate. Reference-only512control lowers that spatial error to3.4824%; no
native512adoption. Old faint-arrival failures and0.4Kcap remain unchanged.

Separate new fixture bound21K is initial/source-derived. No source/coefficient/
limiter/gate changes, live Thermal session, default solver adoption, resolved
convection/buoyancy, material calibration, hot-fire or continuum-solid claim.
Build using `scripts/build_thermal_transport.py --warm-solid --columns` with
explicit frozen0.3.3inputs and a fresh output directory. Audit retained runs with
`scripts/fire_fluid_warm_solid.py <evidence-root>` using NumPy; exit1 means the
reported physical point gate failed. Source-only diagnostic; no package release.

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

## 2026-10-13 — Matched boxed-face forcing gate

Final bounded verdict: support/exchange passes; forcing is not qualified.
64³ hit its 1,200 s cap after the 3.175 s observation, without final fields or
closure. Matched-prefix history and heating-demand differences are 45,309.97%
and 57,969.89%, outside 10%; these do not replace a full 3.2 s qualification.
No larger grid or automatic rerun. See the linked detailed verdict.

Box-face qualification now fixes the full lower-face footprint and a 1/32 m
physical gas depth across 32³/64³. The paired source inputs agree under
conservative restriction to 1.30e-16 relative; timing is 2.5 ms flow and
1.25 ms scalar. Old boxed fields predate the current momentum-flux repair and
are historical evidence only. New current-operator runs are bounded to these
two grids, with a 20-minute cap on 64³; no larger grid was admitted.

Support/exchange analytic tests pass, but that does not qualify the plume
forcing. The 32³ held-response cadence changes 16.185% at 50→25 ms and 3.263%
at 10→5 ms. Only the latter passes 10%; a denser-observation rerun preserves
the gas field bit-for-bit. Neither absolute 300 K normalization nor a negligible
signal waiver is permitted. [Detailed forcing verdict and evidence](fire_fluid_surface.md).

## 2026-10-13 — Directional fluid proof and bounded heated-face numerics

The completed matched Z and X references pass the unchanged 10% local heat gate
at 4.705451% and 2.679662%; matched 64³→128³ still fails at13.100227%.
Fresh source/hash/report readback reuses those results after the UI checkpoint.
X/Y symmetry and a bounded 90-degree native rotation test pass; full three-axis
convergence remains unqualified. See [reference decision](fire_fluid_local_transport.md).

A separate native non-burning face prototype now conserves gas/surface exchange
and matches independent analytic/matrix solutions under prescribed and frozen
sampled-plume conditions. Live evolving boxed-face forcing is the concrete next
prerequisite; ordinary workers/UI/joint checkpoints remain unchanged.
See [surface proof and limits](fire_fluid_surface.md). No release, Desktop refresh
or review-service change is included.

# Fire–Fluid source and transport accuracy checkpoint

2026-10-10, persistent Main Edit development. The original **common 8³
observation gate passes at 64³** for both the prescribed-source no-box and inert
box controls. This is a bounded low-temperature Boussinesq numerical baseline.
**Local surface heat transfer and burning-object admission remain unqualified.**

## Independent numerical ingredients

Native tests separate each ingredient from the plume:

- A fixed 0.125 m heating layer, with transport disabled, produces the exact
  SI heat and smoke amount and a 1 K temperature rise. Across 8/16/32/64 vertical
  cells, maximum heat error is below 3.2e-14 J.
- The same horizontally uniform layer with buoyancy stays hydrostatic. Pressure
  agrees with independent integration of the piecewise constant buoyancy force;
  velocity stays below 1e-10 m/s. Uniform heating with open vertical boundaries
  also reproduces the analytic acceleration, including the expected Euler lag.
- Uniform-heating velocity error halves for dt 0.01/0.005/0.0025 s. This verifies
  first-order time integration; it does not make the method second order.
- Smooth periodic cell-average translation, at fixed dt 0.0005 s, improves from
  3.20% to 1.44% to 0.470% L1 error across 16/32/64 cells. Heat and smoke agree.
- Fourier diffusion improves from 0.0524% to 0.0128% to 0.00291%. Its separate
  fixed-grid time study exposes temporal/spatial cancellation; a smaller time
  step alone need not monotonically reduce total error against the continuum.

Native and ASan/UBSan runs pass. The earlier 16³→32³ failed plume is preserved.

## Separate time and grid comparisons

All plume probes replay the exact same 32 accepted Fire frames. Source patch,
per-node heat/smoke amounts, 0.125 m support, 10% synthetic transfer, domain,
properties, 3.2 s duration and the 330 K applicability ceiling are unchanged.
No Fire worker evolves during these prescribed-source refinement probes.
The accepted source frames originally came from the live 8³ wind-feedback
control; this is **not a live 64³ coupled-feedback qualification**.

At fixed 32³, the full native scalar-field differences are:

| Fluid dt pair | Heat L1 | Smoke L1 |
| --- | --- | --- |
| 0.005→0.0025 s | 0.190% | 0.131% |
| 0.0025→0.00125 s | 0.0942% | 0.0651% |

Both ratios are approximately 0.497. The declared 1% temporal threshold passes.
Temporal error is much smaller than the previous spatial discrepancy.

Grid refinement keeps dt 0.005 s and restricts extensive fields conservatively
to the **same original 8³ observation partition**. The unchanged 10% gate is:

| Case | 16³→32³ heat / smoke | 32³→64³ heat / smoke | Coarse observation gate |
| --- | --- | --- | --- |
| No box | 22.11% / 16.64% | **9.14% / 7.44%** | Passed |
| Inert box | 22.30% / 16.66% | **9.19% / 7.47%** | Passed |

Fixed-world wind-probe differences also decrease. No-box L2 differences are
0.006020→0.002493 m/s; box differences are 0.005621→0.002247 m/s. Blocked
normal faces and solid heat/smoke remain exactly zero. Fine final accounting
residuals are below 9e-12 J and 4e-17 kg; final peak temperature is below
311.14 K. Native pressure/divergence criteria remain unchanged.

## Deliberate limit of the passing gate

On the **finer 32³ partition**, 32³→64³ differences are 16.81% heat / 9.95%
smoke without the box and 16.71% / 9.89% with it. The coarse gate passing does
not establish accurate fine thermal structure, point temperatures or surface
heat flux. The live 8³ authoring controls and existing 32³ previews retain their
previous accuracy-unqualified labels. No new flame/particle geometry was added.

Burning surfaces still require local heat-profile/flux convergence and an
explicit qualified surface-response boundary. No physical calibration,
reciprocal thermal feedback, material ignition or burning-object enablement
is claimed by this checkpoint.

## Implementation and verification

The unboxed lateral case now reuses the existing cached Cartesian multigrid
pressure solver. Masked box pressure uses a cached Galerkin preconditioner and
CSR iteration matrix. Acceptance still recomputes the independent matrix-free
true residual. Cached versus matrix-free 32³ box results agree within
4.5e-16 J, 2.7e-22 kg and 1.7e-16 m/s. No equation or physical tolerance changed.

The native probe avoids duplicating large JSON source requests for every
refinement batch. It executes the production solver with bounded local binary
SI arrays; it is not a package/worker protocol. The ordinary source-built
worker independently admits and returns a two-step 64³ box state.
Numerical allocation peaks at 162,542,448 bytes (155.01 MiB). Fine profiles
therefore explicitly select the existing 512 MiB numerical and 256 MiB request/
output envelopes; these are resource limits, not relaxed physics checks.

Verification order: fresh native/worker and full application compile; targeted
native and sanitizer tests plus 30 Python tests; broad `test-stable` passes.
Canonical remains unchanged. No commit, VERSION edit, package or publication
occurred. Patch release recommended for this solver/cache slice; approval is
required before a version bump. The broader unadopted coupling work has its own
release assessment.

## Reproduction

```sh
make BUILD_DIR=build/<fresh-accuracy> open-accuracy-probe open-atmosphere-worker \
  test-open-accuracy-native test-open-accuracy-sanitize \
  test-open-atmosphere-native test-open-atmosphere-sanitize \
  test-open-lateral-native test-open-lateral-sanitize
python3 -B scripts/fire_fluid_accuracy.py --frames <accepted-32-frames.json> \
  --probe build/<fresh-accuracy>/open-atmosphere/accuracy-probe \
  --grid 64 --dt .005 --output <fresh-no-box-case>
# Add --box for the matched inert box. Time refinement uses fixed --grid 32.
```

Use `fire_fluid_accuracy_report.py` against the completed case layout and prior
isolated checkpoint. Read the explicit `coarse_observation_gate_passed` status
and finer-partition limits; successful process exit alone is not qualification.

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

## 2026-10-16 finest-column timing closure

The selected sharp-transport half-step case passes all four unchanged 10% gates.
At fixed 0.48828125 mm spacing, reducing dt from 0.00125 to 0.000625 s changes
main-profile error 0.002817% → 0.002764%, near-history 0.009269% → 0.009049%,
far-history 6.955368% → 6.758111%, and early-near history 10.268064% → 9.956535%.
The previous finest-case failure therefore had a small but decisive temporal
component. Coarse-grid spatial bias remains established by the exact-profile
flux tests. This is one narrow prescribed-velocity/source benchmark pass, with
only 0.043465 percentage points of early-history margin; it does not qualify
coarser production grids, original boxed forcing, wall laws or live coupling.

The native retry completes 5120 steps / 3.2 s in 137.22 s, using 16,384 cells
and 10,003,120 tracked bytes. Heat residual 2.467e-11 J; velocity error zero;
no rejected or live allocations. Initial 90 s attempt timed out with buffered
stdout; preserved failure receipt. A diagnostic-only line-buffering change and
one 180 s capped retry produced the complete result. No solver formula or
physical parameter changed; small full-trajectory output remains bit-exact
against the retained probe. Runner supports `--case finest-half-step` (180 s
native cap), preserves default matrix behavior and rejects simultaneous flux
selection. Five runner/reference tests and three diffusion-reference tests pass.

Evidence: `_private_workspace_artifacts/fire-fluid-finest-timing-20261016/RESULT.md`.
Next: use Thermal's qualified receiver contract for a matched inert coupled
benchmark. Do not tune a flux formula solely to the previous marginal failure.

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

## 2026-10-17 — Original sharp-startup 1D diagnostic qualified

A retained numerical startup layer resolves the initial heat release on 32
subcells per reported cell (4096 internal 1D cells), then conservatively restricts
at 0.08875 s to the original 128-cell operator. Inputs are the original sharp
field; no reference-state replacement, coefficient tuning or temperature floor.
The conservative FCT and prior target sources are unchanged. First-step heat
error is 3.8454% (half dt 8.6804%), and 25-ms heat error 0.1724%. Main/far/wrap
early histories are 0.7186/0.9446/0.8226%; all reference-qualified monitored
histories pass the unchanged 10% gate, including half-dt repeats. Bounds and
conservation pass; exact coarse-only/pure-advection replay and strict/sanitizer
verification pass. See the startup diagnostic tools in `src/tools/README.md`.

This closes the bounded original-startup 1D fixture, not the older along-wall,
boxed-forcing or full gas-solid qualification. The periodic constant-coefficient
startup layer is not an application default or a variable-flow/3D/continuous-
heating implementation. Localized along-wall startup/transfer qualification is
the next boundary; global cubic refinement is not justified.


### Localized prescribed-flow startup qualification (2026-10-17)

A separate 128-strip along-wall diagnostic now retains local x-subcell profiles
through continuous source startup, with matched native wall support and Thermal
receiver packets. Against independently refined retained references, early gas
tail error is 1.116%, total wall-heat history .233%, and given-Q solid replay
.001161%/.047832%. All standard monitored histories and 18 reference-qualified
local checks pass the unchanged 10% gate. This is not uniform improvement over
the earlier candidate: patch 4 heat error rises .593% to 4.736%, while wrap patch
heat improves 5.389% to 2.306%. Patches 5/6 remain reference-unqualified.

Short faint-arrival references through .5 s also fail their own .5% refinement
criterion; their enormous native/reference relative differences cannot establish
a 10% accuracy failure. Same-normal reference comparisons identify a gas normal
spatial contribution; no result justifies changing wall or solid laws. Further
work must qualify those short/weak reference signals before numerical tuning.
This is source/test evidence for prescribed y-invariant flow, not application-
default adoption, full 3D feedback, combustion, package or release qualification.


### Qualified faint-arrival references expose short/local failures (2026-10-17)

Independent positive Robin arrival and cancellation-controlled Fourier/source-
image calculations now qualify the previously uncertain short gas-tail/wall-heat
history and patches5/6. The retained native run has 32.74% gas-tail and
53.84–53.91% wall-heat history error through .5s; patch5/6 full heat histories
have15.11/36.53% error. These excessive signals fail10%; broader history passes
remain limited to their original metrics. Same-normal reference errors drop to
2.36/5.94%, supporting a bounded wall-normal resolution comparison next.

A separate small-signal solid limitation is also exposed: patch6 back rise is
zero despite positive given-Q replay. A direct shared core_thermal0.3.2 probe
reproduces zero back energy versus independent BE1.88418e-17J for a two-cell
50J/K,2W/K pair heated by1e-16W. Shared/Thermal repair is separate; no shared or
native numerical change/adoption is made by this reference qualification.


### Short normal/time attribution (2026-10-17)

The bounded64-to128normal comparison through.5s improves wall-heat history
error53.84–53.91% to10.45–10.50%;halving scalar time then gives10.15–10.20%.
All fail the unchanged10% gate. Source/near-wall gas histories pass,but the
separate downstream gas tail remains30.00% after time refinement. An independent
same-normal reference shows128-normal spatial wall-arrival error13.55–13.60%,
partly offset by other numerical effects in native coupling. This does not
establish a converged solver or justify tuning time merely to cross the gate.

Thermal separately repaired the tiny-signal back-cell defect as shared development
core_thermal0.3.3;its supplied probe is independently read back at6.97e-14 relative
energy error. These gas measurements intentionally use pinned0.3.2 inputs for
attribution;patch6/full coupled replay with0.3.3 remains a distinct qualification.
No application-default,package,release or full3D feedback adoption is claimed.


### Corrected solid replay verified independently (2026-10-17)

Exact original patch6heat packets have been captured at3.125ms windows,including
actual1/1600s allocation,to avoid interpolation from25ms observations. Replaying
these through core_thermal0.3.3passes all32per-strip receiving/back HISTORY gates
against an80-digit BE oracle,worst1.978e-15relative. Local accounting and native
refusal/regression checks pass;strict/sanitizer output is exact. Six earliest
point samples still fail10% at<=2.213e-323J;no signal cutoff or blanket pointwise
qualification is introduced. Final back rise1.082516e-19K is restored.

The producer rerun exactly matches the old0.3.2trajectory. This is given-Q replay,
not a corrected full gas run. Physical-reference receiving/back errors remain
36.535/33.945% under those inaccurate packets;the latest short gas wall/tail
failures10.15–10.20%/30.00%remain. Stop before feedback or broader adoption.

The bounded0–.5s interface comparison at128normal cells and half scalar time
finds two errors with opposing effects. Moving the fine-region end from3/32 to
1/8 increases tail-history error30.0017% to47.8024%;wall history remains
10.1493–10.1991%,still outside10%. A manufactured smooth-tail check at the same
time step measures62.9777% of the exact diffusive face flux at the32:1 jump,
versus100.0208% across equal fine cells. The old interface suppresses heat entry
and partly masks another gas-transport error. At.475s,before restriction at
.48875s,the extended gas tail is already52.3256% high. Which retained gas term
(upwind spatial transport,BE timing/splitting,source integration) dominates still
requires a uniform-fine one-dimensional control. Main and near-wall histories
pass at.2304/.8539%;conservation/positivity do not establish tail accuracy.
Ordinary local Fourier profiles fail mode qualification and are not treated as
truth. No new mesh/solver default,10%gate change,receiver adoption or feedback.

The subsequent uniform-fine1D source-pulse control isolates retained x transport:
4096periodic cells,dx1/16384m,dt1/25600s,original source/supports to0.5s,without
normal transport,restriction or wall/solid coupling. Diffusion-only tail-history
4.3817%passes10%;u0.025m/s tail48.7814%fails. At0.5s,the exact-in-time spatial
operator is43.7983%high;BE time adds1.8787%relative to that biased spatial
solution. Exact spectral BE/source recurrence matches native within4e-12relative
at late sampled points,clearing a source-window bookkeeping error in this control.
First-order upwind numerical diffusion is the identified dominant spatial
mechanism;smaller time alone cannot remove its exact-in-time error. Constant
velocity1D percentages are not a quantitative decomposition of the coupled shear
case. Diffusion history acceptance does not qualify14early point failures;
advection fails all20nonzero tail points. Conservative limiter/source clocks/
10%gate unchanged;no coupled solver adoption or feedback. Positive-kernel and
60/90-digit late spectral qualification are retained in private uniform-pulse
20261018evidence. Next boundary is a bounded conservative advection candidate
in this same harness,before returning to the coupled test.

A bounded higher-order advection candidate now passes the SAME uniform-fine1D
history gates:main0.00980578%,tail0.765727%,versus original advection main
0.120067%,tail48.7814%. A same-split/time donor control still has46.4991%tail
error,so reconstruction provides most of the improvement. Advection uses the
existing fourth-order face interpolation,unchanged paired FCT limiter and
SSPRK2;physical diffusion remains the prior BE solve. Diffusion-only history
is exactly unchanged(4.38173%tail). Original limiter source is byte-identical;
a diagnostic-only256→4096capacity specialization changes no limiting algebra.
Bounds,source inventory and cold-cell transfer checks pass;strict/sanitizer
histories match. Remaining10tail samples0.025–0.25s fail10%;no point waiver,
heat floor or reference-state replacement. Uniform periodic constant-u acceptance
does not establish a nonuniform-interface or coupled gas/wall/solid result.
Next boundary is the known3/32nonuniform interface before returning to the wall
benchmark. No default solver adoption,gate change or feedback. Evidence:private
uniform-advection-20261019/RESULT.md,runs/report.json.

Nonuniform3/32qualification now separates a working advection candidate from a
remaining diffusion failure. On the original32:1mesh,tail-history errors are
41.0351%original,38.8278%same-split donor,and5.72366%higher-order candidate.
Main candidate error0.009806%;uniform recovery retains0.765727%tail. The mixed
candidate passes10%history,but diffusion-only26.7071%still fails and12tail
point samples remain outside10%. At0.5s,candidate tail is5.46636%low;
diffusion-only26.3519%low. Manufactured limited advection at the jump99.9887%
of physical face flux;unchanged diffusion62.9777%. This identifies the remaining
missing late heat at the diffusive interface rather than wall/solid response.
The early time/spatial decomposition remains unqualified.

The geometry adapter preserves the paired FCT algorithm/16epsilon safeguard,
using cell capacities and exact-width polynomial face moments. A strict-bound
negative-subnormal refusal exposed underflow in physical-width heat budgets;
normalizing to finest-cell heat units repairs the arithmetic without changing
cell values,limiter,cap,10%gate or introducing a heat floor.36uniform parity,
256mixed profile and32subnormal steps,cold transfer,full strict/sanitizer and
limiter/matrix regressions pass. No global/default/coupled adoption or feedback.
Next bounded boundary:nonuniform diffusion reconstruction at the same interface.
Evidence:private nonuniform-advection-20261019/RESULT.md,runs/report.json.

A local nonuniform diffusion correction now clears the prior diffusion-history
failure while leaving the advection candidate and limiter source unchanged.
The original positive BE solve is followed by a conservative,limited derivative
correction on stencils touching the32:1width changes. Face derivatives use
actual cell-average moments;sign/cold-front guards retain physical two-cell
transport when a high-order fit is unsafe. Manufactured target flux100.618%
of exact and accepted one-step flux95.6019%replace the old62.9777%face flux.
Same2112-cell0–.5s diffusion-only tail26.7071%→1.51884%;fixed-advection tail
5.72366%→0.765039%. At.5s,respective tail biases+1.68114%/+0.658849%.
Uniform control is byte-identical,4.38173%diffusion-tail history.10%gate,
source/timestep,limiter,bounds and cold-cell transfer remain fixed;no floor,
clipping or reference-state replacement. Early point failures remain9diffusion/
8advection samples,so earliest arrival is not fully qualified. Full strict/
sanitizer,bounds/conservation/cold/subnormal and unchanged regressions pass.
Standalone1Dqualification only;next short prescribed-shear128normal benchmark
before any feedback or default adoption. Evidence:private nonuniform-diffusion-
20261019/RESULT.md,runs/report.json.

The next short prescribed-shear qualification is complete:128normal cells,
repaired scalar pair,0–0.5s,unchanged10%gate. Main/near gas histories pass
0.057247%/0.667888%;tail30.001674%→1.220113%. Total wall heat remains
10.149297–10.199060%fail,unchanged by xrepair to2.596e-12relative. Prior
independent normal control isolates13.55–13.60%128-normal spatial error with
partial transport/exchange cancellation. All256solid-cell histories respond
correctly to actual delivered heat (independent80-digit BE,worst1.413e-15).
That is not physical local-solid acceptance:short far-patch Fourier prefixes
cancel,and earliest gas points still fail.21solid point errors remain near
1.24e-323Jand below. No heat cutoff or point waiver.

Integration exposed a -min-subnormal gas update at tick69 near the periodic
coarse-to-fine companion7/32. A narrow adapter guard rounds the paired accepted
face transfer toward zero only when its actual two-cell update would be negative;
no clipping,limiter-factor,bounds,source or10%gate change.16cell/full-row
old-refusal/new-pass strict/sanitizer reproducers pass;prior1Dhistories remain
byte-identical. Fresh strict/sanitizer shear80tick parity,full800tick runs,
all-column given-Qaudit and scalar/transfer regressions pass. No feedback,
shared change or default adoption. Next bounded boundary:1Dx-mean wall-normal
128/256and separate scalar/exchange-time refinement,not another large grid.
Evidence:private fire-fluid-shear-pair-20261020/RESULT.md,runs/report.json.

The bounded periodic-x mean now reproduces short shear wallQto3.12e-12relative
and isolates normal spatial error from scalar,receiver and exchange timing.
Same physical source/contact,actual native four-y-cell pair exchange and frozen
Thermal0.3.3receiver.128/half-scalar remains10.000594–10.050290%fail;
256normal/base scalar0.508084–0.553492%pass. Independent time-refined CN
isolates13.55–13.60%128spatial error versus3.30660–3.35328%at256. Receiver
step halving changes~2e-9relative;scalar half~0.135%;exchange half/quarter
changes2.11%/0.95%and removes fortuitous spatial-error cancellation. A preferred
mean candidate256/half scalar/quarter exchange passes2.554766–2.601098%and
is0.727774%from same-normal CN.10%gate,bounds,physical parameters and existing
operators unchanged. No full shear/default adoption or feedback.

Earliest physical wall points remain unqualified:256CNat25ms gives1.26e-43J
versus~1.06e-59Jwith only0.0758%time-refinement change. This demonstrates a
normal spatial early-tail error despite history acceptance. All given-Qsolid
histories pass~1e-15. Eight full strict/sanitizer mean histories/packet traces
match;transfer/kernel regressions pass. Evidence:private normal-mean-20261020/
RESULT.md,runs/report.json. Next:separate audit,then matched-clock256normal
shear integration;existing normal limits/hardcoded times require explicit work.

The selected 256-normal, half-scalar, quarter-exchange profile is now integrated
into the repaired-scalar prescribed-shear diagnostic through 0.5 s. Named physical
clocks preserve source-off 0.4 s, restriction 0.48875 s and observations 0.025 s.
The old 128 control reproduces all observations and packets exactly. The repaired
scalar pair, limiter, normal matrix, wall kernel and Thermal 0.3.3 remain fixed.
Main/near gas history errors are 0.015698%/0.190373%; tail 1.160550%; total wall
heat 2.554766–2.601098%; interval-average wall rate 1.945417–2.113974%. These pass
the unchanged 10% gates. Independently refined local references qualify three
patches; their heat/front/back histories pass at 2.37–3.47%, versus about 13–14%
old back-solid error with the same reference. Five weak patches remain unresolved
by short-prefix reference cancellation. All 256 given-Q solid histories agree
to 3.792e-15, with 101 subnormal point discrepancies explicitly retained.

Earliest gas and wall arrivals still fail: 25 ms tail 1.70e-160 K versus 8.86e-200 K;
wall heat 1.35e-43 J versus about 1.06e-59 J. At 100 ms respective relative errors
are 429.7% and 491.2%. No point exclusion, heat floor, coefficient tuning, extended
horizon or new scheme was introduced. Full selected run, strict/sanitizer smoke,
exact source/transition clocks, eight refusal/recovery controls and affected
regressions pass. Evidence: private shear-integrated-20261020/RESULT.md,report.json.
This is a diagnostic reciprocal gas/solid integration, not a new live Thermal
return channel, evolving-flow qualification or default adoption. Stop for manual
advancement to the proposed appreciable-response non-burning demonstrator.


## 2026-10-20 — Joint headless wall-refined consumer

The selected warm/cool-solid numerical step is now a reusable private module
shared by the original demonstrator and a new local joint runtime. Complete
accepted gas/solid states have exact-clock admission, versioned checkpoints,
retained cell corrections and compare-and-swap publication/recovery. Frozen
Thermal 0.3.3 receiver numerical ownership stays separate. No live channel,
ordinary app/worker default, evolving-flow or ignition adoption is implied.
See [runtime contract and reproducible commands](fire_fluid_joint_runtime.md).
The original physical inputs, bounds, limiter and 10% gates remain unchanged;
old faint arrivals and extra near-zero signed-change errors remain open.


## 2026-10-20 — Online headless Thermal receiver process

The fixed wall-refined joint host can now route each actual wall packet through
a separate process executing Thermal's receiver/solid solve. The worker returns
a staged candidate; PhysicsSim alone accepts and checkpoints both participants.
Loss of a response or worker death therefore leaves no separately committed
solid ahead of gas. Protocol replies check the actual receipt interval, payload,
source checkpoint, allocated heat, state and request identity. Explicit fields
cross bounded inherited pipes; no ABI structure or solver pointer crosses.

Frozen Thermal/core0.3.3 remains the numerical control. Current Thermal owner
sources with core0.3.4 quantity wrappers are additionally built read-only and
compared against that exact baseline; no shared source/import/minimum change.
This is a source-built benchmark process link, not an installed Thermal GUI or
session connection. Inputs, conservative limiter, bounds and10%gates stay fixed;
old faint-arrival and near-zero net-change limitations remain open. See the
[joint runtime contract](fire_fluid_joint_runtime.md). Stop before new fluid
momentum/buoyancy or combustion feedback. Next functional boundary is the existing
spatially varying heated-wall benchmark through this same receiver seam.


## 2026-10-20 — Spatial live receiver benchmark

The selected128-strip/256-normal prescribed-shear benchmark now accepts an
explicit separate receiver executable. Full0.5s current-Thermal execution,
including receiver death/restart controls, reproduces retained gas, wall and
all-column solid output exactly. Physical supports, clocks and10%gates remain.
The adapter's five-second idle exit was repaired with a separate60swait for
producer work; frame/reply waits remain5s. No physical/numerical coefficient
change. Old faint-point/reference-limited results remain open. This is a
source-built benchmark connection; durable spatial-host restart, buoyancy,
ignition and installed-session adoption remain separate. See the
[headless process contract](fire_fluid_joint_runtime.md).
