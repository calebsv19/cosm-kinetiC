# kinetiC Current Truth

Last updated: 2026-10-08

## Reconciled source checkpoint (2026-10-08)

Main Edit contains canonical `2114163835ae` (application 0.4.0), the committed
lifecycle repairs and atmosphere tools, and managed shared snapshot
`1bf262dc8acac5354d95eb0c57bdc9ce0af39197` with Core Sim 0.8.1. The shared
snapshot preserves the shape JSON text APIs needed by the persistence repairs.
Fresh contained application/headless/session-worker builds pass. The framework
is available in the subtree; real bidirectional Fire–Fluid API adoption remains
separate. Canonical source, application/worker VERSION, installed packages and
remote state are unchanged by this checkpoint. Review the checkpoint evidence
before canonical fast-forward and Registry release preparation.

## Shared UI preparation and historical evidence gap (2026-10-06)

The [shared UI rollout ledger](shared_ui_rollout.md) records preserved source
checkpoints, canonical reconciliation and immutable shared import. At the October 6 checkpoint, UI adoption
and a clean consumer build were unverified. The October 8 source checkpoint now
passes a fresh contained build; visual UI adoption remains a separate acceptance. A broad clean command mistakenly
removed retained ignored CFD evidence before being stopped. Some historical
field/receipt paths below are now unavailable; full recovery is unresolved.
Source checkpoints survive, but source docs alone do not restore those proofs.

## Local corner Fire / 32³ atmosphere source qualification (2026-10-05)

The separate [open-reservoir model](open_atmosphere.md#atomic-source-batches-and-32³-corner-receiving-qualification-2026-10-05)
adds bounded atomic Python source-batch admission without changing the native
solver. GrowthSim runs matched 32³/16³/disabled-feedback sources through 5 s:
conservative J/kg, exact continuation and causal thermal flow pass. The original
refinement's cp100000 applicability rejection is preserved; qualified runs declare
an explicitly synthetic cp800000 material with the same source bytes. This is
committed Main Edit local CLI evidence, separate from releases, CFD obstacle
qualification, physical ground closure, large-temperature fire-air behavior,
64³ scaling and full-lit synchronized movie acceptance.

## Usable stationary-box source checkpoint (2026-10-04)

`cfd_box_3d` / `steady_box_duct` now supports explicit aligned physical bounds
through actual local scene authoring, compiled revision, CLI/MCP and native
session execution. It retains steady creeping Stokes, a 2×2 m duct, no-slip body/
Y/Z walls, natural end tractions and bulk body Re≤0.1. Snapshot bounds, six face
areas, projected area, Reynolds/Cd and XYZ momentum assessment use actual geometry.
Absolute force accuracy remains unestablished; no transient obstacle, arbitrary
mesh, moving body, turbulence or validated wake capability is added.

A reproducible [48,24,24]→[96,48,48] long-box scene passes complete exported U/P
SI equation readback, original residual/divergence/flux/discrete budgets and
physical energy/momentum screens. Fine physical imbalances are 0.5684%/0.4944%.
Its pressure/viscous force changes are 6.7218%/2.9313%, explicitly provisional.
Nineteen developer fields measure geometry/refinement/domain sensitivity; short
box n64→n80 passes the four-metric 1% screen, while long pressure force and the
narrow-gap pair still fail it. L4→L6 force changes are below 0.032%. Original
600 s stops remain failures; separately declared longer repeats pass unchanged
equations and memory/cell/residual gates.

Backend, nine material-scaling cases and native sessions pass ordinary/sanitized.
Actual box MCP/scene controls, masks, export and low-memory rejection pass; existing
cube/Cartesian agent regressions pass. A separately named source worker is built;
the protected worker, package/install state and unrelated Main Edit work remain
unchanged. The finite delivery stops at this usable checkpoint.

Transient obstacle implementation is incomplete. The concrete next slice is the
masked mass/history operator and one pressure-driven Stokes startup from rest,
with an independent time oracle, transient budgets and atomic failure publication.
The current cube reference investigation is closed for this batch: raw 1.13–1.20%
mismatch keeps certified cube-force claims open, without blocking this workflow.
See [runnable box checkpoint](cfd_3d_box_checkpoint.md),
[finite delivery checklist](cfd_3d_box_delivery.md) and
[exact startup implementation specification](cfd_3d_obstacle_startup_spec.md).

## Unforced physical flow and independent cube trials (2026-10-04)

An optional unforced periodic3D Navier-Stokes backend now accepts a finite,
divergence-free initial face field, reusing the existing conservative transport,
BE/BDF2 momentum and pressure operators. Independent decaying Beltrami tests
show approximately second-order spatial and temporal convergence. Finest32 cubed
velocity/pressure errors are0.03169%/0.89800%; energy error0.06339% and physical
dissipation error1.21594%. Zero forcing, decreasing energy, global momentum,
advective work, original full residual/divergence and resource controls pass.
Existing manufactured complete fields are bitwise identical; unforced and
existing session tests pass ordinary/sanitized. This is a source backend API;
wall/open/obstacle transient and agent integration remain separate.

Nine steady-Stokes material/flow/density controls pass complete-field and force/
power scaling, maximum2.36556e-12 relative error. Surface-edge031 cube refinement
worsens raw force mismatch1.1951→1.4114% and is rejected. First side-normal023
improves to1.1343%, missing both raw1% and prospective10% improvement forL8;
no paired run is admitted. The retained047 pair remains the paired baseline.
Native absolute force certification and broad CFD remain incomplete.

Next: unforced material/anisotropic and energy-transfer cases, cube stress accuracy,
qualified stationary boxes, boundary-aware transient inertia and curved-object
wake references. Physical accuracy remains first. See the
[unforced accuracy and cube refinement checkpoint](cfd_3d_unforced_accuracy_checkpoint.md).

## Native cube convergence and improved paired reference (2026-10-04)

Four complete native pressure-driven cube fields pass fresh checks of the original
SI equations, strict flux and discrete conservation. L4 n64-to-n80 pressure,
viscous, total force, inlet pressure and dissipation changes are all below 1%;
matched n64 tunnel-length body-force changes are below 0.0037%. Fine cases pass
5% separate-force / 3% scalar comparison screens against the retained reference.
The original n32 case still fails these screens.

A four-interval pressure trace is integrated as an explicit optional backend API,
with original default forces and solver equations retained. It passes 900 analytic
physical-face controls and 33 failure controls across centered/fallback cases and
a displaced rectangular body on anisotropic cells, ordinary and sanitized. All
four solved-field API readbacks preserve every original numerical/physical control.
The helper is source integration; installed-worker/package state is separate.

A quality-screened directional reference pair reduces raw surface/reaction gaps
1.4037/1.4066% to 1.1951/1.1996%, with original full equations, pressure modes,
flux, divergence, energy and resource controls passing. Refinement/domain force
changes remain below 1%. This is useful progress, but the reference raw 1% gate
still fails. Native physical certification remains open. The narrower 0.03125 m
edge interval passed geometry but its completed field worsens raw mismatch to
1.4114% and is rejected. The thinner side-normal trial improves only to1.1343%;
its prospective paired-run gate fails. Tighter and two-pass geometries that fail
quality checks remain excluded.

Next: complete reference edge accuracy, then qualified native force comparisons,
stationary-box cases and truthful scene/agent diagnostics, followed by transient
and inertial qualification. Performance remains secondary. See the
[native cube and paired reference checkpoint](cfd_3d_cube_pressure_checkpoint.md).

## Nonzero cube-wall accuracy passes; pressure candidate retained (2026-10-04)

Five independently forced native wall-shear resolutions measure exact face-average
velocity, cell-average pressure and known physical traction. Supported160x80x80
meets velocity1%/pressure0.3% targets with errors0.6340%/0.2434%, total force
error0.63481%, full momentum2.301e-14 and max divergence1.440e-14. Existing native
1048576-cell admission and1GiB/600s case limits remain; larger grid rejects before
solving. The finer supported case and its read-only physical receipt assessor are
reproducible developer commands.

A test-only second-order viscous trace improves prescribed fields but worsens
solved traction and is rejected. Native first-wall quadratic consistency defect
is measured separately; full fields still converge about second order. Actual C
four-interval pressure trace passes940 polynomial/fallback/offset patch controls
and retains half-order on a square-root counterexample. It halves solved pressure
force error at n64; at n80 it reduces total known-force error0.63481→0.37470% with
the current viscous observer. It is retained for further diagnostics, with native
operators/defaults unchanged. Original pressure-driven cube reference raw1% and
broader native physical qualification remain open. Next matched native cube force
checks and directional reference edge convergence. See the
[wall accuracy checkpoint and next physical work](cfd_3d_wall_accuracy_checkpoint.md).

## Reproducible native accuracy; local cube refinement rejected (2026-10-04)

The new `scripts/run_cfd_native_accuracy_regression.py` freezes native inputs,
compiles and repeats four independently forced smooth steady-Stokes resolutions
plus84 forcing-integral checks. All pass; fine128x64x64 velocity/pressure errors
repeat0.4221%/0.1351%, with final orders about2.015/1.998. Unique named runs retain
bounded supervision, logs and success/failure receipts. This source-checkout
developer command measures interior field accuracy, not cube-wall shear,
singular cube forces, arbitrary objects or transient/inertial flow.

The genuine radius0.04m local L4 cube trial completes on88320tet. Original full FE
residual8.8801e-12, flux, divergence and energy pass, but raw surface/reaction
mismatch worsens1.4037→1.8312%. The candidate is rejected under its prospective
accuracy rule; no L8 counterpart runs. The retained graded pair remains the best
physical baseline, with original raw1% still open.

Independent quartic-velocity/cubic-pressure traction calibration on both actual
L4 meshes passes six face loads, two pressure offsets and two quadrature orders;
maximum face-load error3.775e-15N. This supports the observer on prescribed smooth
fields without qualifying solved edge stresses. Four-interval reconstruction
remains a useful diagnostic. Next: a known-answer native case with nonzero cube
wall shear, then systematic normal/tangential edge convergence. Accuracy remains
first; native equations, worker and defaults are unchanged. See the
[current checkpoint and physical development sequence](cfd_3d_local_accuracy_checkpoint.md).

## Graded force tests improve accuracy; native pressure targets measured (2026-10-04)

Two new graded filled-cube fields (81792/100608tet) pass original full FE residuals
9.046e-12/8.995e-12, flux, divergence and physical energy checks. Raw surface/reaction
mismatch improves about18.5%, from1.7215/1.7265% to1.4037/1.4066%; original1% remains
open. Separate force refinement changes stay below0.665%, and paired-tunnel force
changes below0.007%. This pair is selected for further accuracy diagnostics, without
physical certification or native/default adoption. A separately declared local
reference allowance120000tet/8192MiB/1800s preserves strict physical gates.

Calibrated cubic pressure projection plus matched archived native fields isolates
two pressure-force contributions. At64x32x32, native reconstruction gives-3.5620%
and the remaining field-functional contribution-4.7673%, summing to-8.3293% against
raw reference pressure force. This is an archived force-functional diagnosis, not
a full pressure-field norm or a new native solve; reference raw equilibrium is still
unqualified. Twelve support controls pass, including a structured resource-stop
repair in a separate guarded entrypoint with unchanged successful behavior.

Next near-edge stress convergence and independent native pressure/operator/traction
controls; broader objects and inertial/transient physics follow explicit physical
qualification. Efficiency remains secondary. See the
[current graded accuracy assessment and next six steps](cfd_3d_graded_accuracy_assessment.md),
[sealed pair evidence](cfd_3d_accuracy_graded_checkpoint.md) and
[cubic projection calibration](cfd_3d_cubic_projection_checkpoint.md).

## Accuracy-first physical testing resumed (2026-10-04)

Latest continuation: two support controls and four targeted side-layer geometries
preserve the cube, but both redistribution/bisection double worst Jacobian
conditioning. No new flow field or factorization from these controls. Develop a
graded transition next; speed remains secondary. Use the
[current accuracy assessment and next six steps](cfd_3d_accuracy_next_steps.md),
including the corrected geometric shape values from authoritative survey JSON.

The new explicitly declared 3072 MiB/600 s local reference lane completes both
quality-screened finer filled-cube fields: L4 43008tet/84iterations/154.383s/fullFE
6.035e-12; L8 49920tet/130iterations/231.207s/fullFE8.893e-12. Original physical
coefficients, pressure modes, strict residuals, conservation/energy and atomic
publication remain checked. Historical performance/resource failures remain sealed;
speed and factor-savings thresholds no longer gate this accuracy-first lane.

Separate pressure/viscous/reaction refinement changes stay below0.3%, and fine-pair
body-force domain changes below0.2%. Raw surface/reaction agreement still fails:
1.7215/1.7265% against1%. Independent signed stress observation localizes major
contributions near cube edges; next target first side-face fluid-layer resolution.

Correction to later historical summaries below: original domain criteria treat
inlet pressure and total dissipation as length-dependent responses, not quantities
required to stay flat in a longer no-slip tunnel. Fixed-length scalar refinement
and per-run energy checks remain. Steady Stokes reference evidence does not qualify
native general-object or transient/inertial CFD. See the
[accuracy-first checkpoint and wider direction](cfd_3d_accuracy_first_checkpoint.md).

## Packed velocity improvement: useful small field, matched cost still fails (2026-10-04)

Twenty-four support controls and six sealed checkpoints preserve original filled-
cube equations, residual/pressure modes, physical inputs and resource gates. Packed
local sweeps give9.07%complete-action speedup and one useful small full field:
113iterations/22.1806s/fullFE8.9834e-12. Force/scalar equivalence passes. Native,
default, complete-factor recipe and physical qualification remain unchanged.

Exact28416tet matched L4 hits180.163s during independent full FE verification;
no accepted field is published. Complete velocity factor/work savings280.930MiB
miss300MiB, although numeric memory admission and actual peaks pass. Retained
momentum dominates continuity. A separate unchanged-action timing control assigns
61.2%cost to local inner8,28.7%coarse corrections,9.1%outer physical actions.
Next split local action/sweep costs and improve that measured target, proving full
scratch bounds. The original small/matched usefulness gates still precede finer
force testing. Raw force and domain-energy failures remain open.
See [measured batch and next gates](cfd_3d_packed_velocity_batch_assessment.md).

## Pressure and velocity investigation: finer force testing still withheld (2026-10-04)

Twenty new support controls and five sealed checkpoints preserve the original
filled-cube equations, Float inputs, residual targets and resource gates. Complete
Double diagnostics quantify fixed pressure response error and sampled global
coverage. Stronger pressure columns produce a strict full small field at
114 iterations / 24.909 s; sixteen pressure directions produce a strict field at
135 / 25.961 s. Both fail the unchanged 23.502 s usefulness gate. Adaptive inner
stopping saves work but fails accuracy; a tighter safe fill bound has negligible
benefit. Native/default/qualified complete-factor recipe stays unchanged.

The next candidate should improve retained local fill at the same storage cap,
then pass small and matched L4 whole-cost/memory gates before a finer field.
Earlier momentum/continuity traces and current pressure/mesh diagnostics guide
that investigation; sampled conditioning is not a full spectral certificate.
The quality-passed finer L4 still exceeds complete-factor admission by 287.527 MiB.
Existing force-sensitivity fields remain valid, with raw force/energy failures
retained. See [batch evidence and next gates](cfd_3d_pressure_velocity_batch_assessment.md).

## Cubic pressure correction rejected by exact-cube time cap (2026-10-04)

Four support tests and small strict field pass, but exact33216tet L8 candidate
stops at180.035s, sampled1535.313MiB, no field or complete physical proof. Retained
continuity remains slower than momentum; twenty global directions are unadopted.
Original equations/modes/caps/fields/failures stay intact. Next one fixed physical
Double residual correction inside Float velocity PC with measured accuracy and
extra workspace reserve. See [cubic pressure result](cfd_3d_pressure_cubic_checkpoint.md).

## Exact cube full numerical field passes; performance threshold still open (2026-10-04)

One target-only33216tet L8 experiment completes390iterations/156.923s/1520.375MiB
with full FE1.258e-11, original coefficient/Float/mesh/load identity, restoration,
conservation and atomic publication. Forces/scalars agree<3.2e-10 relative to the
accepted baseline. Iteration/setup+solve improvements8.45percent miss10percent
adoption threshold; experimental recipe/general size policy unadopted. New field
is available for diagnosis; original1.757percent force discrepancy still fails.
Next20-column cubic GLOBAL PRESSURE candidate with unchanged physical/full gates.
See [complete exact-target result and limits](cfd_3d_exact_target_checkpoint.md).

## Size selection strict fields pass; general cost adoption withheld (2026-10-04)

Two policy tests and small/normal legacy/normal selected full controls preserve
physical/Float identities and strict FE/resource/publication. Small historical
cost and larger prospective whole-peak criteria fail; general size policy stays
unadopted. Exact target stage1678.332MiB and full numerical controls permit a new
target-only bounded numerical investigation without changing those failures. See
[size policy costs and next exact-cube proof](cfd_3d_size_selected_checkpoint.md).

## Completed workspace retirement reduces memory; historical timing fails (2026-10-04)

Three lifecycle tests and both full controls preserve FE/load/solution bits and
release all named completed buffers without GC. Small358.828MiB and normal976.766MiB
improve peak, but whole13.072s/81.447s fail their unchanged historical <=10percent
increase gates. No matched numeric/adoption. Next size-selected exact encoding with
prospective contemporary complete controls; failures remain sealed. See
[whole costs and remaining boundary](cfd_3d_workspace_retirement_checkpoint.md).

## Exact encoded action passes; small complete cost gate rejects adoption (2026-10-03)

Six independent C/action/factor/FE/lifecycle tests pass. Exact matched symbolic
stage releases Double owner, saves85.494MiB known backing and freshly admits at
1678.332MiB. Complete normal control owns16.94percent less peak memory with strict
FE/equivalence, but original small control owns11.23percent more peak, failing its
unchanged cost gate. No matched numeric/adoption; next distinct post-solve workspace
retirement preserves original FE/full residual checks. See
[complete costs and retained failure](cfd_3d_encoded_operator_checkpoint.md).

## Shared predictor exact-bit storage potential passes (2026-10-03)

Four codec tests and complete actual22.412million-coefficient representation pass.
Float32 predictor matches prior PC input; Int32 word corrections restore every
original Float64 bit. Joint physical/PC stored-byte potential85.494MiB passes;
whole diagnostic26.20s/owned1143.5MiB with exact archive readback. Original Double
remains live, no factor/field/RSS-benefit/adoption inferred. Next encoded C action
and shared Float factor adapter, then strict full-run proof. See
[bit preservation, joint bytes and limits](cfd_3d_exact_prediction_checkpoint.md).

## Full-word catalogue rejected by memory and negative storage bound (2026-10-03)

Four bit-preservation codec tests pass, but actual matched representation probe stops
at1821.840MiB projected construction memory. Already>=9.401million distinct words
bound maximum possible saving13.769MiB<32MiB, so no catalogue/archive/factor/field
adopted. Next distinct shared Float-predictor plus exact integer bit correction,
with complete Float-input/physical-bit/resource proof. See
[resource evidence and storage bound](cfd_3d_coefficient_catalogue_checkpoint.md).

## Cycle collection rejected as sufficient memory recovery (2026-10-03)

Four lifecycle tests and both symbolic controls preserve complete original inputs/
action. Explicit full GC collects29objects, but paired fresh RSS improvement4.922MiB
fails32MiB useful-recovery gate. No numeric continuation/adoption; old failures remain.
Next measure exact velocity coefficient bit-pattern catalogue potential, then only
proven lossless representation can justify action/factor changes. See
[cycle evidence and limits](cfd_3d_cycle_recovery_checkpoint.md).

## Exact residency measured; unreachable-cycle control next (2026-10-03)

Corrected symbolic-only census covers complete physical operator and309.127MiB
known numeric backing. Post-relief/validation/fresh RSS629.469MiB gives1820.879MiB
required>1800MiB; timing does not explain this gap. Initial callable-operator census
is retained as incomplete. Four original/five corrected support tests pass; no GC,
numeric factor or field in large diagnostics. Next separate live-action-protected
cycle collection control; completed numeric benefit remains unproven. See
[ownership, measurement timing and limits](cfd_3d_residency_checkpoint.md).

## Coarse pressure controls useful; matched memory admission withheld (2026-10-03)

Four independent algebra controls and four strict paired fields pass with exact
physical/Float-input identities. Quadratic pressure correction reduces iterations
16.9%/25.5%; normal whole86.44→73.07s with owned peak+4.98%. Matched L8
admission1831.051MiB>1800MiB withholds numeric factor/field; correction remains
experimental. Prior accepted field/force tests remain available; physical force
qualification fails. Next exact residency attribution and fresh admission timing.
See [cost, preserved gates and resource gap](cfd_3d_pressure_coarse_checkpoint.md).

## Balanced body grids rejected by transition quality (2026-10-03)

Four independent controls pass; all eight predeclared L4 body remeshes preserve
domain/symmetry and refine surface triangles, but violate local quality gates.
Several edge mean shapes improve13–18%; surrounding transition condition worsens.
No factor/field launch; all original physical failures remain. Next bounded global
pressure correction on accepted meshes. See [screened candidates](cfd_3d_balanced_body_checkpoint.md).

## Calibrated empty ducts separate wall-length resistance (2026-10-03)

Five independent tests and all four strict P4/DG-P3 empty fields pass. Both n6
baselines improve all analytical errors and meet the separate.1% amplitude,
dissipation and full pressure-field mass gates. Diagnostic obstacle-excess
pressure/dissipation domain changes.196%/.050%; original raw total25%failures and
raw/reaction1.757%force mismatch remain. Complete force qualification still fails.
Next fresh quality-controlled fixed-domain body-resolution geometry, then resource
admission and measured force checks. See [calibration and limits](cfd_3d_empty_baseline_checkpoint.md).

## Signed-edge refinement rejected by actual local geometry; empty baseline next (2026-10-03)

Four independent geometry controls pass. Accepted L8 mesh rebuild is bitwise exact;
16actual signed-score-guided edge stars preserve domain/volume/symmetries but all
fail local quality. Longest affected worst shape+40–50%, max condition+30–52%;
no factor or field launch. Existing accepted force testing remains available.
Next independently calibrated empty-duct L4/L8 baselines; preserve raw force/scalar
failures and all caps. See [survey evidence](cfd_3d_edge_star_checkpoint.md).

## Exact matched field accepted; force convergence testing resumed (2026-10-03)

Explicit retained1e-11 stopping plus exact bounded observation reuse keeps full
requested1e-10 and every physical/resource gate. Original/normal controls pass;
exact matched33216tet L8 completes160.38s/owned1619.55MiB/full1.179e-11 with maxdiv
2.455e-9/s. Saved cube triangles and23616held inner cells match the L4 anchor.
Body pressure/viscous/reaction domain changes.298/.058/.197% pass component gates,
but raw/reaction1.757% and total pressure/dissipation~25%still fail complete physical
qualification. Independent signed identities close4.26e-14N and account for.700mN
gap. Next body-adjacent quality/force refinement and explicit empty-duct baselines.
See [accepted field, comparisons and limits](cfd_3d_retained_margin_checkpoint.md).
Default/native/desktop unchanged; broad Stage1/general qualification stays active.

## Exact bounded observation reuse recovers whole time; divergence still gates L8 (2026-10-03)

Independent analytical and accepted saved-field outputs match exactly; combined
volume observations31.77→21.79s. Separate complete original/normal controls pass;
normal whole93.66→76.23s with higher measured owned peak in this pair. Exact matched
L8 finishes155.71s/full8.431e-11 under caps, but maxdiv1.978e-8/s rejects publication.
Next declare stricter retained1e-11 stopping while full1e-10 and all physical
checks/caps stay fixed. See [cost, exactness and limits](cfd_3d_observation_reuse_checkpoint.md).

## Restart6 improves solve time; complete observations still reach180s (2026-10-03)

Two calibration fields and three independent controls pass. Exact matched L8
restart6 live admission1793.03MiB and sampled1600.59MiB recover memory, but complete
observations hit180.17s; no field/force acceptance. Next bounded observation reuse
must preserve every residual/physical check and prove actual equality/cost before
a new attempt. See [evidence and next scope](cfd_3d_restart_six_checkpoint.md).

## Small restart closes memory admission; completion time still blocks matched force (2026-10-03)

Three original/normal full fields pass requested1e-10 and all numerical gates;
normal8 fails maximum divergence and publishes no field. Normal4 passes at250
iterations/99.865s/1103.75MiB. Exact matched33216tet L8 stage now admits1763.71MiB
with complete both-basis accounting, but the bounded numerical run reaches the
180s cap after full-residual verification and remains unaccepted/unpublished.
No force comparison follows from its progress phases. Next distinct restart6
calibration must recover whole completion under the same caps. Default60 preserved.
See [complete evidence and limits](cfd_3d_restart_small_checkpoint.md).

## Short restarts verified; exact matched force field still budget-withheld (2026-10-03)

Four mixed/basis/stop/parameter controls pass; six original/normal full fields meet
requested1e-10 and complete gates. Optional restart24/12 retain the full Float
factor/all physical coefficients and reduce normal owned peak~1244→1076MiB with
lower measured whole cost; default60 is preserved. Exact matched L8 restart24/12
estimates1873/1825MiB remain above1800, so no numeric factor/field. Import-only AMG
exclusion saves~5MiB, insufficient; no adapter/scientific placeholder is used.
Preserve prior1.770%raw/reaction physical gap. Next declare8/4calibration with full
basis accounting and explicit requested-target publication gate.
See [complete calibration, limits and evidence](cfd_3d_restart_checkpoint.md).
Stage1/native certification and the broad goal remain active.

## Compensated graph rejected by measured convergence/cost (2026-10-03)

Six coupled-SPD/Float/lifecycle controls pass. Normal strength distribution has no
completely zero off-diagonal blocks. Threshold1e-2 misses original1e-10 at3000
iterations with momentum stall;1e-3 normal passes but costs21%more time/4.5%more
peak for2.15%factor saving. Fresh complete versuszero-pruning control has essentially
identical cost/memory; reject apparent historical saving. Preserve pressure
coefficient/mass/global-mean readbacks and every original equation/mode/target/cap.
No pruning adoption or larger matched L8 launch. Next bound restart storage with
complete Float factor, then consider separately measured global pressure correction.
See [complete controls and limitations](cfd_3d_pruned_graph_checkpoint.md).
Stage1/native certification and the broader goal remain active.

## Same-surface normal refinement reduces force gap; matched L8 capped (2026-10-03)

Thirteen complete-FE/geometry/Float/lifecycle/domain controls pass. Original-macro
cut and scalar graph remain resource-rejected. Separate non-nested28416-tet tensor
remesh holds actual cosine cube triangles and solves120iterations /87.75s /
1621MiB /full8.32e-11. Pressure/viscous/reaction shifts0.153/0.334/0.126% and raw/
reaction2.108→1.770% are useful but do not pass separate1% physical gate. Signed
identities close; global stress norms rise. Matched L8 holds23616inner cells/body
but estimates1981MiB, so no larger field. Preserve every target/cap/mode and initial
conditioning failures. See [complete fields, limits and tests](cfd_3d_second_normal_checkpoint.md)
and [next measured factor-storage gate](cfd_3d_normal_force_next_goal.md).
Stage1/native physical certification and the broad goal remain active.

## Uniform cube solves; physical force gate remains open (2026-10-03)

Eight constrained transition and six balanced-cosine candidates fail unchanged
shape gates. Seven original-FE/geometry controls pass. Uniform count6 normal
solves80iterations /62.52s /1339MiB /full5.51e-11; count8 numeric estimate1916MiB
remains withheld. Global shape/stress improve, but viscous change1.190% versus
cosine normal and raw/reaction2.121% keep physical gates open. Retain the diagnostic
field without physical adoption. Next isolate X-normal resolution with actual
cosine body surface held. See [complete geometry, forces and limits](cfd_3d_force_transition_checkpoint.md).
Stage1, native physical certification and broad goal remain active.

## End plateau capped; signed force-local geometry screened (2026-10-03)

Three end layers preserve the inner/body control but estimate1968MiB exceeds
1800; no numerical allocation/field. Nine mesh/signed/input controls pass and
the accepted end2 signed observation preserves original stress rows exactly.
Both lifts recover0.839mN traction discrepancy with substantial component/cell
cancellation. All16ranked paired local candidates worsen affected intrinsic
worst shape, so no mesh is adopted or solved. Next optimize transition quality
before resource/fullFE/force tests. See [resource, attribution and geometry evidence](cfd_3d_end_plateau_checkpoint.md).
Stage1, native physical qualification and broad goal remain active.

## Optional precision preconditioner resumes blocked end-slab force test (2026-10-03)

Eleven support controls pass for separate Float coupled preconditioning and
float64 flexible iteration, preserving every original physical coefficient/mode/
fullFE check. Original/base/normal pass requested1e-10; matched base chunk512 is
~6.9% faster/~8.4% less memory than exact. Retain initial chunk128180s rejection.
Previously blocked28416-tet L8 end refinement now passes180iterations /102.18s /
1445MiB /full6.16e-11 under unchanged caps. End pressure/reaction shifts1.635%/
1.203% and raw/reaction2.104% keep physical gates open; smaller global stress
indicators are not certification. Optional measured reference path adopted,
exact/default paths retained. See [complete tests/costs/forces/limits](cfd_3d_mixed_precision_checkpoint.md).
See the [matched-control cost readback correction](cfd_3d_mixed_precision_readback_correction.md).
Stage1, native/desktop certification and broad goal remain open.

## Nodal complement passes equations but misses target/useful cost (2026-10-03)

Six bijection/nodal/congruence/SPD/FE/lifecycle controls pass. Original cube field
matches exact reference, but2861iterations /81.41s /604MiB is~5x slower,~57% more
memory and full residual7.94e-10 misses requested1e-10. Retain accepted numerical
field/target miss; reject adoption/larger extension. Next test precision only in
coupled preconditioner with unchanged float64 physical operator/fullFE residual,
flexible outer iteration and unchanged admission/resource/force gates.
See [measured rejection](cfd_3d_nodal_hierarchy_checkpoint.md).

## End-slab shape improves; exact allocation rejected (2026-10-03)

Four controls pass for28416-tet held-normal L8 end-slab subdivision with inner
mesh/body/domain held. Maximum Jacobian condition halves543→272, but exact
symbolic/live budget2254MiB exceeds unchanged1800. Numerical launch is withheld;
no field or force result. Next test a bijective nodal-vanishing polynomial split
with fixed SPD blocks, original equations/full residual and all caps retained.
See [shape and allocation evidence](cfd_3d_end_slab_checkpoint.md).

## Matched longer normal tunnel accepted; physical force gate remains open (2026-10-03)

The 23616-tet L8 held-normal control passes in 400 iterations /126.34s /1560MiB,
full original FE residual 8.74e-11, complete numerical/publication gates and unchanged
resource caps. Pressure/reaction L4-to-L8 changes 1.953%/1.418%, raw/reaction 2.073%
keep the physical 1% gates open. Signed traction gap stays about 0.837mN; end-slab
Jacobian condition rises to543 while held inner conditioning is unchanged. Next
separate end-slab quality from domain sensitivity and test signed force-specific
refinement. See [matched field, attribution and limits](cfd_3d_normal_domain_checkpoint.md).
Stage 1, native/desktop certification and broad goal remain open.

## Exact normal cube accepted; force convergence testing resumed (2026-10-03)

The original stopped 23616-tet count6 normal now passes at 170 iterations,
97.8s / 1555MiB, independent full residual 4.81e-11 and complete numerical/
publication gates. Complete vector/caller-owned exact factor plus bitwise load/
FE metadata/catalog restoration preserves all equations, pressure modes, target
and caps. Explicit GC is rejected; smaller-grid vector/load paths remain preferable
for cost. Base-to-normal pressure/viscous changes 1.117%/1.010% and raw/reaction
2.108% keep the 1% physical gate open. Stress defects decrease but are not force
certificates. See [accepted field, costs and limits](cfd_3d_factor_catalog_checkpoint.md)
and [next matched domain/force/refinement sequence](cfd_3d_force_restart_goal.md).
Native/desktop certification, Stage 1 and the broad goal remain open.

## Exact FE metadata restoration passes; normal live guard still rejects (2026-10-03)

Five controls pass; original/base fields match after bitwise metadata restoration.
Whole-process base cost worsens. Normal diagnostic estimate 1774 MiB admits an
attempt, but its live guard measures 1806 MiB and stops before factor allocation.
No normal field. Next include deterministic full FE catalogs unused during factor
lifetime, restore all original numbering/tables before reconstruction and preserve
all gates. See [measured control and gap](cfd_3d_factor_metadata_checkpoint.md).

## Explicit Python collection rejected for normal admission (2026-10-03)

Five lifetime/FE/GC-policy controls pass, but base measured cost worsens and normal
stage still predicts 1810 MiB above 1800. Collection reports 26 objects with no
immediate RSS drop; no numerical normal launch or field. Next suspend/restore
bitwise only deterministic FE metadata unused during the exact factor lifetime.
See [costs and preserved rejection](cfd_3d_collect_pressure_checkpoint.md).

## Lossless load passes; normal live budget still stops allocation (2026-10-03)

Five load/admission controls pass. Original/base full RHS is restored bitwise;
accepted fields/forces match, base peak is 1119 MiB. Normal diagnostic estimate
1719 MiB admits an attempt, but its numerical live guard measures 1817 MiB and
stops before factor allocation. No normal field exists. Next explicit collection
of unreachable Python cycles is measured with all live input/action and high-water
proof. See [retained stage and guard difference](cfd_3d_sparse_load_checkpoint.md).

## Complete exact vector storage accepted; normal still over budget (2026-10-03)

Eight support controls pass. Original/base numerical fields match prior exact
results at 381/1136 MiB and requested 1e-10 full residual. Matched base memory
improves 16.1%, time rises 4.9%. Normal symbolic/live-RSS estimate improves to
1821 MiB but remains above unchanged 1800 cap; numeric launch withheld. Next
lossless sparse load residency removes redundant dense inlet-load copies and
restores exact full RHS after factor cleanup. See [measured evidence](cfd_3d_vector_storage_checkpoint.md).

## Exact caller-owned factor accepted on base; normal budget still exceeds cap (2026-10-03)

Original/base pass full numerical gates in 150/160 iterations, 16.2/116.2 s,
443/1353 MiB, and meet requested 1e-10 residual target. Base memory improves about
8.72% against shared-factor control with matched chunk size. Exact normal symbolic/
pressure-stage budget is about 1912 MiB versus 1800 cap; numeric launch is withheld
and no normal field exists. Next complete vector-block operator/factor storage
preserves exact coupled inverse and all equations. See [evidence and limits](cfd_3d_workspace_cholesky_checkpoint.md).

## Exact caller-owned sparse-factor API declared (2026-10-03)

Installed SDK documents separate symbolic/numeric Cholesky with caller-owned
factor and scratch buffers. The next bounded control uses those stages and
free-page pressure between them while preserving the exact coupled inverse and
all original equations/acceptance/resource authority. Nodal-vanishing approximate
hierarchy is deferred. See [declared API/lifetime proof](cfd_3d_workspace_cholesky_goal.md).

## Unit polynomial split rejected on original cube (2026-10-03)

The bijective exact cubic/quartic unit-carrier split is algebraically SPD but
fails original numerical gates: 3000 iterations, full residual 3.55e-4. Quartic
coordinate momentum remains much larger than coarse projection. No accepted
field or larger extension. The next nodal-vanishing complement retains all
physical DOFs/equations. See [failed control and diagnostic scope](cfd_3d_hierarchical_checkpoint.md).

## Additive cubic control: original passes, refined publication incomplete (2026-10-03)

Original passes in 38.3 s / 994 iterations / 624 MiB. Exact count6 base stops
at 180 s with retained momentum 2.18e-9 at iteration 1300 and 1465 MiB, before
full FE/physical/publication verification. It remains rejected. The next declared
invertible cubic/quartic coordinate block preconditioner keeps all velocity and
pressure equations. See [retained evidence](cfd_3d_additive_coarse_checkpoint.md).

## Cubic coarse convergence improves; refined wall-time still fails (2026-10-03)

Balanced cubic/two-block original passes in 696 iterations / 40.7 s / 652 MiB
and meets 1e-10. Exact count6 base stops at 180 s, momentum 1.97e-8 at iteration
800, 1466 MiB. Symbolic coarse/principal factor bytes exactly match numerical
setup. No accepted refined field or normal admission. The next additive SPD
cost control keeps the same factors and original equations. See [proof and limits](cfd_3d_cubic_coarse_checkpoint.md).

## Quadratic coarse modes improve convergence, refined time gate open (2026-10-03)

Original L4 passes at 1046 iterations / 48.2 s / 474 MiB and meets 1e-10 full
residual. Exact count6 base still stops at 180 s with momentum 1.04e-7 and about
1225 MiB. No refined field is published. The next declared macro-cubic space
adds edge/face modes with cheaper fixed component smoothing; physical and resource
gates remain unchanged. See [proof and two controls](cfd_3d_quadratic_coarse_checkpoint.md).

## Macro-linear correction: numerical pass, cost rejection (2026-10-03)

The exact balanced macro-linear coarse correction passes original gates:
1852 iterations, 65.3 s, 496 MiB, full residual 4.13e-10. Against the two-block
sweep it saves only 8.63% iterations and raises time 24.06%; larger use is rejected.
The next declared macro-quadratic space adds edge modes while preserving the
balanced SPD formula and full equations. See [proof and cost](cfd_3d_coarse_velocity_checkpoint.md).

## Two coupled blocks: original passes, refined time cap (2026-10-03)

The u / (v,w) exact block sweep passes original numerical gates in 52.6 s at
509 MiB, but the admitted count6 base stops at 180 s, 1197 MiB, retained momentum
2.43e-5. This path is rejected for larger runs. The next declared global coarse
velocity correction preserves equations and all acceptance/resource gates.
See [four frozen controls and limits](cfd_3d_block_cholesky_checkpoint.md).

## Fixed component sweeps accepted numerically, cost rejected (2026-10-03)

Four fixed Cholesky component sweeps pass full numerical gates on the original
cube: 2112 iterations, 175.8 s, 336.1 MiB, full residual 3.45e-10. Same-mesh fields
match the accepted coupled factor, but wall time grows 13.06 times. Momentum
limits convergence. Larger use is rejected; the next declared comparison uses
two exact coupled principal blocks. Physical qualification and the full goal
remain open. See [measured evidence](cfd_3d_component_cholesky_checkpoint.md).

## Exact symbolic cost and rejected ordering controls (2026-10-03)

Seven coefficient/action/graph/lifetime/serialization proofs and six bounded
symbolic diagnostics pass without publishing numerical fields. The exact normal
velocity factor requires 1264 MiB storage with only 26 MiB numeric workspace;
scalar interleaving saves 2.65% factor storage but increases conversion/analysis
peak, while the complete vector graph increases requirements. Both controls are
rejected as implemented. See [exact costs, input identity and diagnostic limits](cfd_3d_symbolic_checkpoint.md).
The next declared slice revisits fixed SPD coupled component sweeps using faster
exact principal-component Cholesky factors. Original caps, equations, residual/
physical gates, native sources and old fields/workers stay intact; physical
qualification and the full object/wind-tunnel goal remain open.

## Allocator pressure measured and rejected (2026-10-03)

Five live-input/action/RHS/FE/factor-owner proofs pass for an explicit optional
allocator pressure call. The exact body6 base matches all fields/forces and
numerical gates but saves only 1.83% owned peak RSS with 12.5% more measured total
time; a large current-RSS drop and zero API release report do not establish robust
normal-mesh headroom. The control is retained and rejected as the next adopted
path; no normal retry is made. See [measured total cost and unchanged inputs](cfd_3d_factor_peak_checkpoint.md).
The next declared slice measures exact symbolic factor storage/numeric workspace
and bounded vector-graph ordering. Original caps, native sources, prior fields/
workers and earlier runner defaults remain intact; physical qualification and the
full object/wind-tunnel goal remain open.

## Shared exact-factor input and remaining setup peak (2026-10-03)

Eight independent mixed-action/FE-reconstruction/zero-copy/input/lifecycle proofs
pass. Complete mixed-block storage lets the optional exact factor share row/value
arrays: L4 count6 base completes at 1482.4 MiB / 112.70 s, saving another 7.66%
memory and 14.42% measured time; matched L8 completes at 1685.9 MiB / 130.52 s.
Fields, force diagnostics, original equations and all acceptance gates match the
triangle predecessors. The exact 23616-tet normal still stops during factor setup
at 1827.8 MiB sampled RSS after 15.11 s, with no accepted field. Physical force/
domain/raw-reaction qualification remains open. See [shared ownership, measured
costs and retained failure](cfd_3d_shared_factor_checkpoint.md). The next declared
slice measures allocator/symbolic-factor peak while preserving live inputs and
all original caps. Native sources, prior fields/workers and earlier runners stay
intact; the full object/wind-tunnel goal is active.

## Explicit triangle storage and finer force controls (2026-10-03)

Seven independent action/reconstruction/factor/input/lifecycle proofs pass. The
new optional reference triangle path reduces owned memory 8.04% on the exact
18816-tet count6 base, with matching fields/forces and a 5.86% time cost; the
smaller count4 normal uses 2.58% more memory. The 23616-tet count6 normal still
stops at the unchanged memory cap during factor setup and publishes no field.
Matched near-body L8 base completes at 1784.3 MiB / 144.18 s but pressure/reaction
domain changes are 2.115%/1.440% and raw/reaction gap remains 2.063%. Physical
qualification stays open. See [measured costs, exact failure and domain limits](cfd_3d_triangle_checkpoint.md).
The next declared slice shares the exact velocity-triangle factor input while
retaining all mixed couplings, pressure diagonals, full equations and original
gates. Native sources, prior fields/workers and earlier runner defaults remain
intact; the broader object/wind-tunnel goal is active.

## Bounded assembly admits finer-body force testing (2026-10-03)

The new optional large-mesh reference path assembles exact macro contributions
in bounded COO batches/CSR merges directly on free variables. Five independent
operator/RHS/reconstruction/quadrature/pressure-diagonal proofs pass. A fresh
matched count4 normal control uses 18.7% less owned peak RSS with total time
within 0.6%, and fields/forces match. The exact 18816-tet count6 base that stopped
at 2001.9 MiB now completes in 124.39 s / 1745.8 MiB with original full residual
3.15e-11 and all numerical/resource/publication gates passing. Raw/reaction gap
improves 3.007% to 2.171%, but pressure sensitivity is 1.056% and global stress
defects worsen; physical mesh qualification remains open. The small original
control uses more memory in the new method, so earlier reference defaults remain
available and unchanged. See [measured costs, equivalence and physical limits](cfd_3d_bounded_checkpoint.md).
The next bounded slice is exact symmetric triangle storage for finer normal/domain
controls. Native sources, prior fields/workers and all original thresholds remain
intact; the full object/wind-tunnel goal is still active.

## Signed cube force gap and finer-body resource target (2026-10-03)

Signed stress attribution locates the largest uncancelled contribution near cube
edges: the held L8 nearest centroid bucket contributes -0.000987618 N to a net
-0.000848945-N raw/reaction gap. Six fixed-cost redistributions and both 63-pair
local refinement families fail prospective shape gates; no rejected geometry is
solved or adopted. Five independent shape/conformity/original-action/attribution
proofs pass. The existing better-shaped 18816-tet count6 base is attempted with the
stronger solver, but stops during factor setup at the unchanged memory cap after
11.64 s / 2001.9 MiB observed RSS, with no accepted field or force result.
See [signed targeting, rejected controls and the exact memory failure](cfd_3d_corner_checkpoint.md).
The next bounded implementation is reduced sparse assembly/operator storage with
matched original-equation proof before retrying that body mesh. Native sources,
old fields/workers and all original gates remain intact; physical qualification
and the broader object/wind-tunnel goal remain open.

## Selective outer refinement measured and rejected for accuracy (2026-10-03)

A 14208-tet paired outer refinement completes in 75.85 s with owned peak
1546.5 MiB and original full residual 9.18e-11. Four independent geometry/action
and input-integrity tests pass. Despite small raw-force changes, raw/reaction
mismatch rises 2.102% to 2.118%, volume/jump stress defects increase 44.2%/28.5%,
and worst Jacobian conditioning worsens. The independent observer closes force
identities to 2.18e-14 N. The field is retained as numerical evidence; the mesh
is rejected as an accuracy improvement and larger outer candidates are not run.
See [measurements, rejected control and body/corner next gate](cfd_3d_selective_checkpoint.md).
Force testing remains operational under the stronger optional solver. Physical
qualification and the broader goal remain open; native sources, old fields/workers
and all original equations and acceptance/resource thresholds are preserved.

## Controlled L8 domain and mesh sensitivity (2026-10-03)

Matched L8 controls now separate tunnel length from near-body spacing. Holding L4
near planes gives normal-refinement pressure/viscous/reaction changes of
0.158%/0.918%/0.345%, but raw/reaction mismatch remains 2.102% and matched L4-to-L8
pressure/reaction changes remain 1.954%/1.341%. Global outer subdivision fails the
unchanged memory cap and publishes no accepted field; its brief peak was missed
by periodic sampling. The domain probe adds owned phase resource checks, with
unit rejection and accepted-field equivalence proofs; the actual oversized retry
was stopped by the supervisor. Six focused tests and two independent stress
observers pass. See [measurements, retained failures and selective refinement gate](cfd_3d_domain_checkpoint.md).
Original geometry remains the default. Native sources, prior fields/workers,
original equations and residual/force/resource gates are preserved. Physical
qualification and the broader object/wind-tunnel goal remain open.

## Sparse symmetric factor resumes force testing (2026-10-03)

The optional macOS reference path now completes the exact previously stopped
13824-tet L4 body4 normal refinement in 58.28 seconds and 1577.4 MiB, with original
full residual 9.17e-11 and all numerical gates passing. Five independent factor
support tests, frozen binary/source provenance, tighter uncondensed field controls
and independent stress identities pass. Pressure/viscous/reaction changes are
0.318%/0.887%/0.325%; raw/reaction mismatch improves 3.007% to 2.182%, still failing
its separate 1% gate. Exact factor, METIS ordering, direct triangle copying,
zero reconstruction cache and 128-cell batches are adopted only in this optional
reference runner. Its final original-cube default control is 2.38 times faster
with 12.7% less observed peak memory than the prior uncondensed control. See [admission evidence, limits and next domain/stress gate](cfd_3d_cholesky_checkpoint.md).
Force-convergence testing has resumed; physical qualification and the broader
object/wind-tunnel goal remain open. Native sources and prior fields/workers are
preserved.

## Exact condensation and coupled solver checkpoint (2026-10-03)

Exact macro elimination reconstructs all original P4/DG-P3 cube coefficients and
passes nine focused tests including 24 manufactured solved controls. Coupled
velocity factoring reduces the original cube solve from 32.02 to 12.74 seconds
with original full residual 6.49e-12 and matched raw loads, but peak memory rises
832.4 to 1308.4 MiB. Positive incomplete factoring lowers that peak to 1024.9 MiB,
still above the prior method. Cheaper component, incomplete and multigrid controls
are retained failures; no force/residual/resource gate is relaxed. Raw/reaction
mismatch remains 3.6436%, above 1%. See [exact equations, measured costs and
coupled solver next gate](cfd_3d_condensed_checkpoint.md). Finer force qualification
and the persistent object/wind-tunnel goal remain open; native sources and
predecessor fields/workers remain unchanged.

## Quartic stress, mesh conditioning and numerical acceptance (2026-10-03)

The independent quartic stress observer reproduces component force identities to
1.61e-14 N. All local macro pressure ranks and the global constant restriction are
checked on each tested cube; no extra pressure null direction is detected at the
recorded tolerances. Balanced spacing reduces iterations 610 to 200 but worsens
raw-force mismatch; outer-only subdivision still leaves normal pressure sensitivity
4.78%. These meshes remain rejected accuracy controls. The audit catches one
linear success with divergence just above its unchanged gate; a tighter rerun
passes numerically with essentially identical forces. The new reference runner
now separates linear/full acceptance and atomically publishes fields only after
conservation and post-serialization resource checks. Ten focused tests, the final
matched runtime control and the evidence audit pass. See [measurements, retained
failure and next exact-condensation gate](cfd_3d_graded_checkpoint.md). Native
sources, older reference runners and predecessor evidence remain intact. Physical
cube qualification and the broader object/wind-tunnel goal remain open.

## Higher-order cube reference checkpoint (2026-10-03)

The P4/DG-P3 reference passes seven support tests and 24 manufactured solved
controls, with verified polynomial-exact volume/boundary quadrature and a guard
against singular under-integrated pressure mass. Empty L4/L8 pressure errors are
about .0011%. Original, normal-subdivided and higher-body-resolution cubes
converge; tighter residual changes forces by <2e-9 relative. Raw/reaction mismatch
remains 3.01–6.31%, and two 13,824-tet normal controls hit the original memory cap.
Higher order alone does not qualify cube traction. See [measurements, retained
failures and next mesh/memory gate](cfd_3d_quartic_checkpoint.md). Eight focused
tests and the immutable evidence audit pass. Native sources, prior fields/workers
and the original force/residual/resource gates are preserved. Stage 1 and the
broader object/wind-tunnel goal remain open.

## Cube equilibrium attribution and rejected adaptive refinement (2026-10-03)

An independent elementwise stress observer now reproduces saved raw-surface/weak
force gaps to about 1e-15 N, separating volume defects and interior stress jumps.
Four observer support controls and two adaptive mesh tests pass. A symmetric
score-guided 27,264-tet refinement also converges under the original caps, but
pressure changes 1.63%, raw/reaction mismatch worsens to 2.23%, and its defect score
increases. It is retained as a failed accuracy control. Six focused tests and the
equilibrium audit pass; native sources/headers, saved fields and predecessor
workers/evidence remain intact. See [measurements and next approximation-order
investigation](cfd_3d_equilibrium_checkpoint.md). Physical reference qualification
and the broader object/wind-tunnel goal remain open.

## Continued cube force testing and reference memory improvement (2026-10-03)

A validated implicit saddle operator and bounded-cell reference assembly reduce
matched cube observed RSS by 37.55% and let the previously memory-blocked
23,616-tet control finish. Larger normal and edge controls also converge under
unchanged residual/resource caps. Sixteen focused tests and the spatial audit pass.
Every local macro's mean-zero pressure rank and the global constant restriction
are checked on the exact original cube; no extra pressure null direction is found
at the recorded tolerance. Two new pressure-preconditioner candidates are rejected
on measured convergence/cost; sparse velocity factor plus pressure mass remains
working. Pressure traction still changes 8.97% on the larger normal pair and 2.94%
on targeted edge refinement, so physical force qualification and the broader
object/wind-tunnel goal remain open. See [results and next diagnostic](cfd_3d_spatial_checkpoint.md).
Native source/headers, workers and historical evidence are preserved by this slice.

## Cube preconditioner readiness and resumed force tests (2026-10-01)

The exact failed Alfeld P3/DG-P2 cube now converges with a sparse velocity-block
factor preconditioner, preserving matrix/RHS/mesh identities and all original
resource/residual caps. Momentum residual dominated the AMG diagnostic; the
checked global macro-constant pressure restriction shows no extra near-null mode.
The exact cube passes in 690 iterations/8.63 s; its normal-subdivided control also
converges. Empty Fourier calibration and tighter-residual stability pass. Force
convergence testing has resumed, but pressure-force change 11.06% and raw/reaction
mismatch 3.35–4.02% keep the physical reference unqualified. See
[results, evidence and next pressure/corner gate](cfd_3d_preconditioner_checkpoint.md).
Native sources, workers and historical evidence are unchanged by this slice.

## Stronger CFD reference support gate (2026-09-30)

An independent Alfeld cubic-velocity/discontinuous-quadratic-pressure Stokes
reference now passes element/conformity/divergence support, 24 solved boundary
controls and square-duct Fourier calibration. The first cube hits the unchanged
3000-iteration cap with true residual 1.405e-5; no force reference is accepted.
Frozen success/failure receipts, capped supervision and focused tests are retained.
See [results and next numerical gate](cfd_3d_reference_method_checkpoint.md).
Stage 1 remains incomplete and obstacle physical accuracy remains unqualified.
Native source and existing workers are unchanged by this slice.

## Initial CFD reliability and traction consistency (2026-09-30)

The first independent-audit improvement slice now rejects nonfinite native cube
inputs and validates actual scaled candidates before accepted publication. Agent
snapshots/capabilities disclose the restricted scene and stress reconstruction
semantics. A solved polynomial reference and four frozen cube controls isolate
normal boundary trace sensitivity from a small bulk weak-form divergence correction;
pressure-force uncertainty remains. Focused native/sanitizer/MCP/3D/2D regressions
pass. A separately built new source worker leaves historical workers/audits intact.
See [results and revised stages](cfd_3d_initial_improvements_checkpoint.md).
Obstacle accuracy remains unqualified. No commit, adoption, package or install.

## Bounded 3D CFD Main Edit update (2026-09-29)

The source Main Edit now has `incompressible_cartesian3d_v1`, a fixed-flow
periodic-X rectangular duct and a fully periodic three-component manufactured
Navier-Stokes transient. Three-grid duct pressure, four-wall shear and physical
energy gates pass; separate transient spatial/time refinement is second order.
The models use the existing immutable scene/session controls, slices/probes,
assessment/comparison, numerical budget and digest-bound field artifacts.
See [implementation/evidence](cfd_3d_completion.md) and the
[predeclared contract](cfd_3d_goal.md). Existing 2D numerical and agent regressions
pass. C3D-6 additionally passes the [stationary open straight duct boundary gate](cfd_open3d_completion.md), with solved cell pressure, independent wall/physical-energy references, three grids and outlet-distance tests.
General transient/wake outlets, obstacles, moving bodies, turbulence and local refinement remain future work. This source update does not change public Desktop freshness.

## C3D-7 verified full A/B/C source baseline (2026-09-30)

The predeclared wall-Stokes, conservative transport, physical pressure startup
and nonuniform natural-traction transient contract is complete in Main Edit.
Three grids, separate timestep tests, harmonic amplitude/phase, physical wall/
energy references and evolving 4/6/8 m outlet extensions pass. All four modes
use the existing source scene/session agent interfaces. The final retained 47-case
matrix and independent exported-field/refinement/outlet audit pass; native,
actual MCP, cancellation, sustained inspection, sanitizer and preserved 2D/
periodic/open regressions pass. The macOS source build binds the installed tested
0.18 JSON archive after diagnosing a 0.19 release container leak. See
[completion evidence and limits](cfd_wall3d_completion.md),
[predeclared contract](cfd_wall3d_goal.md) and [historical checkpoint](cfd_wall3d_checkpoint.md).

Startup is temporal profile development, and the nonuniform open test prescribes
continuous natural tractions; no general wake/backflow or body-force certificate
follows. C3D-8 is now implemented but unqualified, as recorded below. Existing 2D, periodic 3D and C3D-6 numerical
paths are preserved. No commit, package or Desktop refresh was performed.

## C3D-8 reference robustness and pressure attribution (2026-09-30)

Native stationary-cube creeping Stokes, scene/session/MCP controls and the prior
body-edge traction/energy correction are unchanged. All native runtime sources
and the exact worker/field evidence are byte-preserved; finest physical momentum
and energy budgets remain passing. No commit, package, adoption or Desktop
refresh occurred.

Independent reference traction now separates normal/tangential stress and edge
bands, verifies exact surface quadrature, and freezes bounded reference sources
and results. Exact tetrahedron/box pressure integration projects reference cell
averages into the native pressure-force trace. Analytic affine/piecewise-affine,
quadratic/gauge and limited-regularity tests plus focused sanitizer pass.

**Physical certification remains false.** Both improved symmetric candidate
references pass their last component/reaction checks, but genuine X-normal
refinement changes raw viscous force 4.88%/5.45% on L4/L8. Consistent grad-div
reduces sensitivity but still fails 1% robustness; stronger stabilization also
fails physical surface/reaction agreement. Tiny residuals and small last-grid
changes do not certify these references. The measured sensitivity is consistent
with approximate divergence/normal-stress and anisotropic coupling error, not
surface quadrature arithmetic.

Relative to unresolved candidates, finest pressure-force deficits split into
trace/solved-field functional contributions: L4 n48 2.582%/2.359% (total 4.942%),
L8 n40 2.429%/3.276% (total 5.704%). These are provisional force-functional
estimates, not full-field norms or physical certificates. Read the
[reference refinement checkpoint and next bounded solver gate](cfd_obstacle3d_reference_refinement_checkpoint.md),
[experiment contract](cfd_obstacle3d_reference_refinement_goal.md) and
[original physical gates](cfd_obstacle3d_goal.md). Stop grading/stabilization sweeps
before a stronger independent incompressibility/traction reference is established.
No native adaptive grids, new geometry, motion or turbulent wakes are qualified.

## Local lifecycle source adoption — 2026-10-07

Uncommitted canonical working-tree changes now use the owned compilation profile
`build/profiles/local-owned`, guarded cleanup, private staged launcher
configuration, retained macOS dependency bundling, coherent optional Linux desktop
entry installation, and retained Linux package transactions/proofs. Existing
build roots, package outputs and evidence are preserved. See
`cleanup_operations.md`, `launcher_configuration_recovery.md` and
`packaging_lifecycle_operations.md` for current commands, proofs and limits.

The canonical headless build passed; disposable build/clean/rebuild, Water,
scene-cache and CLI proofs passed. Packaging adoption passed the focused fixture
suites and a native Mach-O dependency readback. These are source/fixture results,
not installed-app, signing, publication or human GUI acceptance. VERSION and
WORKER_VERSION remain unchanged. The frozen repairs and evidence were independently
archived and retrieved/verified; `focused_cleanup_closeout.md` records the exact
coverage and limits. Main Edit's separate dirty source has not been bulk-adopted
or reset.

## Persistent Main Edit Development Identity

- Canonical source remains `main`; functional development uses the persistent
  `codex/physics-sim-main-edit` lane documented in
  `docs/main_edit_worktree.md`.
- The isolated package is `kinetiC Main Edit.app`, bundle
  `com.cosm.kinetic.main-edit`, with separate `PhysicsSim-Main-Edit` runtime
  and log namespaces.
- `package-desktop-main-edit`, `package-desktop-main-edit-self-test`, and
  `package-desktop-main-edit-refresh` provide the local development package
  surface with exact generic source/binary build identity.
- Packaging fails if source mutates during the build. Main Edit refresh also
  refuses the canonical Desktop destination and a running development app.
- Canonical Desktop refresh now fails closed unless it runs from canonical,
  clean `main` with exactly one registered PhysicsSim worktree and no running
  canonical app. Retained specialist registrations are not removed or ignored
  to satisfy that gate.
- This development identity does not change `VERSION`, the public desktop or
  worker-package line, release history, Registry state, publication, or remote
  runtime authority.
- PhysicsSim source gives the headless worker an independently owned
  `WORKER_VERSION` source surface. The approved preparation targets are
  `VERSION=0.4.0` and `WORKER_VERSION=0.3.4`; Linux worker archive names and worker
  manifests resolve only from `WORKER_VERSION`, while package manifests retain
  `source_program_version` from `VERSION`.
- `config/worker_release_contract.json` records the worker slug, supported
  Linux platforms, job/capability contract, required receipt classes, and the
  two release-authority decisions. This is source-candidate truth only: it
  does not build or authenticate a release artifact, update the canonical
  Release Control PhysicsSim adapter, move Registry current, distribute to a
  host, or submit a job.
- The adopted source adds a distinct
  `package-linux-worker-x86_64-self-test` entrypoint while retaining
  `package-linux-worker-self-test` for the adopted aarch64/Pi route. Both
  package identities still derive from the single independent
  `WORKER_VERSION`. Released desktop and registered worker remain `0.3.2`
  and `0.3.3` until Decision 2. Installed host versions require fresh fixed-helper
  readback; package registration is not activation or workload acceptance.

## Program Identity
- Repository directory: `physics_sim/`
- Public product name: `kinetiC`
- Internal/repo/runtime identifiers still use `physics_sim` and `Physics Sim`
  in launcher, log, binary, and source-level contracts where required
- Primary runtime entry:
  - `src/main.c` (`main()` -> `physics_sim_app_main(...)`)
  - wrapper shell: `include/physics_sim/physics_sim_app_main.h`, `src/app/physics_sim_app_main.c`

## Current Shipped State
- The source-grounded Vulkan lifecycle adoption now tracks canonical shared
  commit `ddc0c2b1420d95132ef089e68e2ce7728fbc53a4`: the managed subtree carries
  `vk_runtime 0.6.0` and `vk_renderer 1.3.2`, and the existing
  `VkRendererDevice` singleton delegates instance/device/queue ownership to
  `VkRuntime`. The app-local `vulkan-rollout-self-test` proves shared-device
  handle identity, validation-clean startup/resize/restart, deterministic BMP
  capture readback, and 2.0x Retina drawable sizing (`1440x900` then
  `1800x1120`). This is committed source truth, but not a version bump,
  release, publication, Registry promotion, Linux package proof, or
  compute-kernel adoption. The simulation and CPU solver paths remain app-owned
  and unchanged.
- Producer-side truthful `3D` export is complete (through `PSBU-11D`).
- The first external-agent source-checkout contract is documented in
  `AGENTS.md`, `docs/AGENT_CONTROL.md`, and `docs/AGENT_DEMO_PACK.md`. It
  starts with `physics_sim_headless`, the deterministic Water smoke, and the
  scene-project cache-output fixture. It does not claim desktop package
  freshness, website download validation, remote worker submission, registry
  mutation, release publication, public worker-package downloads, or a public
  remote submission API. The agent docs show both standalone GitHub clone
  commands and CodeWork workspace-parent `make -C physics_sim ...` forms.
- Public website agent discovery is live through
  `https://ecosystem.calebsv.tech/agents/index.json`,
  `https://ecosystem.calebsv.tech/agents/programs/cosm-kinetic.json`, and
  `https://ecosystem.calebsv.tech/agents/programs/cosm-kinetic.md`. Those
  public files are the supported external discovery layer; private registry and
  release-control evidence is maintainer-only.
- Direct retained-scene headless CLI volume runs are available through
  `physics_sim_headless`.
- Headless scene-project cache output is now available through
  `physics_sim_headless --scene-project <dir> --save-volume-frames`. Project
  mode validates `scene_authoring.json` and `scene_runtime.json`, accepts
  optional `scene_project.json`, derives the runtime scene from the project,
  writes the default run under `physics_sim/runs/<physics-run-id>`, promotes
  VF3D/physics artifacts into `assets/vf3d/active` and
  `assets/physics/active`, and writes project-relative cache manifests without
  mutating `scene_authoring.json`.
- Standalone Water Basin headless runs are available through
  `physics_sim_headless --water-mode`; the current scaffold seeds the interior
  bottom 3D volume from a normalized `water_level`, resolves the default basin
  as a square X/Z footprint, exports standard VF3D/VF3H frames, and writes
  explicit water heightfield sidecars. The optional
  `--water-object-fixture` path now stamps the deterministic
  `water_pool_submerged_solid` fixture into the Water Basin solid mask and
  writes object-coupling/displacement diagnostics into water sidecars.
- Detached submit/status/cancel supervision is now available through
  `physics_sim_job_runner`.
- The first trio detached chain adapter now routes through
  `bin/run_trio_detached_job_chain.sh`, which performs LineDrawing authoring
  synchronously, submits PhysicsSim as the first detached child, and leaves
  RayTracing submission to a later status refresh after `scene_bundle.json`
  becomes available.
- `physics_sim_headless` now writes both `run_summary.json` and
  `run_progress.json`, rejects non-empty output roots by default, and requires
  `--overwrite` for intentional reruns into the same root. `--resume` is
  reserved and currently rejected.
- `run_summary.json` now includes an `atmosphere` diagnostics block for
  retained-scene/headless runs. It records the backend initial-state source,
  sanitized parsed atmosphere settings, procedural seed cell count/max density,
  VF3D warm-start stats when applicable, and final debug/export-facing volume
  density metrics.
- Headless volume export can now select retained frames natively with
  `--volume-export-start-frame`, `--volume-export-stride`, and
  `--volume-export-max-frames`; summary/progress JSON report selected and
  skipped volume frame counts, and detached job-runner requests preserve the
  same fields.
- Headless and detached job paths are trusted-local operator boundaries, not
  public upload or untrusted worker request surfaces. `--runtime-scene` is a
  read-only local input, `--output-root` is the direct run artifact root,
  `--summary`/`--progress` are sidecar writes, `--cancel-flag` is read by
  headless and written by the detached runner under the job root, and
  `--jobs-root` owns detached job state. Direct detached requests may choose
  local scene/output paths; shared job bundles rewrite artifacts under the job
  root.
- `run_progress.json` is now solver-step aware:
  - schema `physics_sim_headless_run_progress_v2`
  - per-frame `sim_steps_completed_in_frame`
  - `sim_steps_total_in_frame`
  - normalized `progress_ratio`
  - progress `stage`
  - `updated_at_utc`
- Detached PhysicsSim jobs now write:
  - `build/agent_runs/jobs/<job_id>/job_request.json`
  - `job_status.json`
  - `run_progress.json`
  - `stdout.log`
  - `stderr.log`
  - `pid.txt`
  - `result_summary.json`
  - output-root artifacts under the requested run directory
- The packaged desktop launcher defaults the shared TimerHUD overlay off for
  normal app use. Developers can still re-enable it with
  `PHYSICS_SIM_TIMER_HUD=1` / `PHYSICS_SIM_TIMER_HUD_OVERLAY=1`.
- A private Linux GUI desktop package proof lane now exists for the windowed
  app. `package-linux-desktop-determinism-test` builds a `desktop_app_linux`
  archive on Linux x86_64, verifies a deterministic `.tar.gz` plus `.sha256`
  sidecar, and runs an unpacked launcher `--self-test`. Linux PC proof
  `pslgui4-20260709a` built
  `kinetiC-0.3.0-linux-x86_64-desktop-stable.tar.gz`, verified checksum
  `ebd581a014abfe69f02df8745ca8bf645fea2b3a7551065ce59aeb47683a572e`,
  launched the unpacked package in the logged-in X11 desktop session
  (`DISPLAY=:0`, `XAUTHORITY=/tmp/xauth_SmfAha`), and captured nonblank
  app-window screenshots. This remains private proof capability only: no
  `VERSION` bump, public release artifact, website metadata, production
  registry state, or worker-package install changed.
- Detached status schema is `physics_sim_detached_job_status_v1` and exposes
  `queued`, `starting`, `running`, `stalled`, `completed`, `failed`, and
  `cancelled` states without requiring a live PTY.
- Authoritative volumetric (`XYZ`) runs now emit:
  - raw `.vf3d`
  - additive `VF3H` `.pack`
  - truthful `manifest.json` / `scene_bundle.json` metadata (`frame_contract=vf3d`, `space_mode=3d`, `axis_authority=xyz`)
- Water mode keeps the existing 3D density/velocity/pressure/solid-mask fields
  for VF3D/VF3H debugging and now additionally emits
  `water_manifest_v1.json` plus per-frame `water_surface_%06d.json`
  heightfield sidecars. The sidecar uses X/Z row-major samples, Y heights,
  finite normals, surface min/max/average diagnostics, wet/dry/solid column
  counts, material defaults for later RayTracing water import, and optional
  `summary.object_coupling` diagnostics for the `water_pool_submerged_solid`
  fixture: object solid cells, footprint columns, wet-overlap cells,
  approximate displaced volume, affected sample bounds, and applied
  displacement delta range. WTR-6.5 adds object-local quality diagnostics to
  the same sidecar contract: displacement sample count, kernel weight
  sum/max, delta sum/absolute-sum/RMS, capped-sample count, object-zone height
  min/max/average/stddev, and object-zone max slope. These are reporting
  metrics over the current export-side response, not solver-authored wake
  coupling yet. `test-physics-sim-headless-water-object-quality-compare` now
  runs a PhysicsSim-only baseline-vs-quality comparison and writes
  `wtr65_quality_compare_summary.json`. The current WTR-6.5 export-side
  response uses broader/lower displacement support plus a bounded
  deterministic ring/shear wake term; wet-stencil object-zone diagnostics now
  show low roughness (`0.003482646 m` quality-profile height stddev and
  `0.019251581` quality-profile max slope in the default comparison), with
  zero capped displacement samples. This remains an export-side approximation,
  not solver-authored two-phase water.
- First-pass parity fixture is locked and deterministic (tiny-domain proof lane).
- Downstream consumer work remains separate:
  - `ray_tracing` now ingests Water Basin `scene_bundle.json.water_source`
    sidecars for backend/headless transparent-water review and remaps
    PhysicsSim Y-up heights into RayTracing's Z-up render frame for horizontal
    basin views; WTR-5.4 moving-light multi-frame review is landed, WTR-5.5
    adds a long-motion sparse full-RayTracing basin review from frames
    `40, 80, 120, 160, 200` of a `201`-frame Water Basin run, and WTR-6 now
    has a first object-water proof with deterministic displacement diagnostics,
    a full-RayTracing basin frame sequence/MP4 review under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_review/`,
    a WTR-6.5 direct-light smoothed-wake preview under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_direct_light_preview/`,
    a matched short WTR-6.5 Disney-v2 temporal-2 comparison under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_wtr65_disney_v2_t2_short_compare/`,
    and a corrected local `100`-frame Disney-v2 slow-light review under
    `ray_tracing/build/agent_runs/physics_trio/water_object_coupling_hq_local_64/ray_tracing_disney_v2_local_t2_100f_slowlight_corrected/`;
    stronger solver-authored object coupling, smoother object-water boundary
    behavior, and editor controls remain follow-up cross-program boundaries.
- Atmospheric preset initialization is now available as an app-local current-state lane:
  - standalone Atmospheric `2D` and `3D` modes seed deterministic density and velocity fields from the atmospheric sampler instead of starting blank
  - normal `3D` fluid/box presets can opt into the same procedural initializer through the compact `Atmo Init` settings control while keeping their normal mode identity
  - custom preset persistence uses v14 for the optional `3D` atmospheric initial-state bit alongside embedded `ATMOS` settings; older v13 files load with that optional layer off
  - exported Atmospheric `3D` `.vf3d` / `VF3H` `.pack` frames can be selected as session-local warm starts for Atmospheric `3D`; warm-start file paths are runtime config, not portable preset-owned data
  - runtime reports and the HUD distinguish blank starts, standalone atmospheric procedural starts, optional atmospheric procedural starts, and loaded warm-start starts
- Runtime mesh assets now have a first actual-geometry `3D` fluid integration:
  - retained runtime-scene mesh instances can carry shared `core_mesh_preview`
    sidecar path/probe/metadata for editor and runtime overlay display
  - preview sidecars are visual and diagnostic payloads only; PhysicsSim solver
    effects use authoritative `mesh_asset_runtime_v1` geometry
  - imported/runtime mesh assets default to solid obstacles when a runtime mesh
    path is available
  - mesh instances can opt out with `visual_only`, `none`, or
    `fluid_obstacle: false`
  - mesh instances can opt into the existing emitter flow with
    `extensions.physics_sim.fluid_behavior` values such as `surface_emitter`,
    `surface_heat_emitter`, and `boundary_flow_emitter`, or the structured
    `extensions.physics_sim.emitter` object
  - mesh emitters clear solid obstacle occupancy and emit density, velocity,
    sink, and heat through voxelized actual runtime mesh footprints
  - closed-volume runtime mesh fills now use a first app-local triangle
    acceleration path over transformed runtime mesh triangles, and high-triangle
    generated contract coverage proves acceleration stats for imported-mesh-like
    assets
  - the `3D` scaffold backend keeps a prepared runtime mesh cache for loaded
    documents, transformed vertices, bounds, and acceleration trees so static
    mesh obstacles and mesh-attached emitters can reuse mesh work across backend
    lifetime
  - prepared runtime mesh entries also cache domain-specific voxel footprints,
    allowing repeated static fixtures to stamp occupied cell indices without
    rerunning triangle shell rasterization and closed-volume point tests on
    every pass
  - prepared runtime mesh cache entries include runtime file size/mtime
    signatures, so edited mesh files trigger an in-place stale refresh on the
    next cache lookup
  - oversized mesh/grid intersections use a conservative actual-mesh
    bounds-fill fallback instead of stalling the solver
  - the scene editor exposes selected runtime mesh role controls for Solid,
    Visual Only, Surface Emitter, Surface Heat Emitter, and Boundary Flow
    Emitter
  - selected runtime mesh objects now show a compact right-inspector readout
    for solver role/effect, runtime file, preview metadata/probe state,
    cached diagnostic footprint, runtime triangle and BVH stats, runtime file
    signature availability, and Wind projected area; this readout is cached on
    editor state and is not drawn through the always-on HUD
  - runtime mesh/import diagnostics are split by owner: import bridges report
    authored/runtime-scene input and path-resolution problems; runtime mesh
    diagnostics report role, preview/runtime paths, voxel footprint, BVH/cache
    fallback, bounds/domain overlap, and Wind projected area; wording-only
    changes should not modify voxelization, prepared-cache behavior, live-watch
    behavior, or Wind inspector UI
  - runtime scene/runtime mesh paths are trusted local authored inputs unless a
    worker-safe bundle stages the scene, runtime mesh documents, preview
    sidecars, and run config together; absolute paths and `$HOME/Desktop/stls`
    recovery are local-authoring conveniences, not portable payload guarantees
  - Apply/Save persists per-mesh role and emitter parameters into each mesh
    object's `extensions.physics_sim` block while preserving non-PhysicsSim
    object extensions
  - the runtime scene emitter diagnostics tool reports each mesh instance's
    runtime path, preview path, transform, bounds, role, voxel footprints,
    acceleration usage/tree stats, budget fallback state, bounds volume, and
    Wind projected area

## Runtime and Editor Snapshot
- Runtime/editor retained-scene lanes are active and structurally separated from legacy compatibility mapping.
- Retained `3D` runtime views use a compact top-left HUD summary for run state,
  domain, volume, slice, Wind metrics, and quality. Detailed backend, mesh,
  cache, and Wind object diagnostics should route through inspector panels,
  headless outputs, or diagnostic CLIs instead of the always-on HUD.
- GUI status and detailed diagnostics remain separate surfaces. Menu status
  text is for short user-facing confirmations or acknowledgement prompts;
  editor status cards summarize Apply/Save state; inspectors own selected
  object/runtime-mesh/Wind details; launcher setup detail belongs in
  `launcher.log`, `--print-config`, and `--self-test`; direct headless detail
  belongs in stderr plus `run_summary.json`, `run_progress.json`, and exported
  artifacts; detached jobs own `job_status.json`, `stdout.log`, `stderr.log`,
  and wrapper stderr diagnostics; renderer/Vulkan stderr remains developer
  diagnostics unless a later selected workflow promotes it into a user-facing
  inspector or artifact lane.
- The retained-scene editor right pane now routes through an app-local
  inspector module with explicit Scene Physics, Object Physics, and
  Source/Emitter grouping. Retained `3D` Source/Jet/Sink controls now live in
  that right Source/Emitter inspector context, while legacy non-retained scenes
  keep the left-pane global source row. Selected runtime mesh objects also show
  cached Object Physics readouts for mesh role/effect, preview metadata,
  diagnostic voxel/BVH stats, file signature state, and Wind projected area.
  Existing button actions and save/apply behavior are unchanged.
- Menu/editor shell buttons now use bounded shared `kit_ui` button
  spec/state/style semantics through app-local `physics_sim_ui_button.*`
  wrappers, while SDL drawing, palette tuning, and button placement remain
  app-local.
- Agent/headless retained-scene runs can now bypass menu interaction:
  - standalone clone build: `make physics_sim_headless`
  - workspace-parent build: `make -C physics_sim physics_sim_headless`
  - standalone clone scene-project run:
    `./physics_sim_headless --scene-project <project_dir> --frames <n> --grid <w>x<h>x<d> --save-volume-frames --overwrite`
  - workspace-parent scene-project run:
    `physics_sim/physics_sim_headless --scene-project <project_dir> --frames <n> --grid <w>x<h>x<d> --save-volume-frames --overwrite`
- Retained `3D` menu selection now recognizes selected scene-project roots
  that contain `scene_authoring.json` and `scene_runtime.json`, reports concise
  project cache state from `physics_sim/active_cache_manifest.json` or the
  compatibility `physics_sim/cache_manifest.json`, and exposes a
  `Copy Cache Cmd` affordance that copies the matching
  `physics_sim/physics_sim_headless --scene-project ... --save-volume-frames --overwrite`
  update command. This is a read-only guidance surface; it does not run the
  cache update from the GUI. The menu now renders contextual Data I/O:
  legacy/direct selections keep editable `Output Root` and `Input Root` rows,
  while scene-project selections show a `Scene Project Cache` panel with
  read-only `Project Root`, artifact-aware `Cache Target`, and active-run
  rows plus `Frames`, `Headless`, and `Copy Cache Cmd`. The cache target
  readout now distinguishes no-cache, ready active VF3D/physics bundle, and
  manifest-present-but-missing-artifact states by checking the manifest's
  active VF3D directory, physics directory, and scene bundle path. In
  scene-project mode, the selected project root is the input/output container;
  legacy roots remain in config but are not shown as cache destinations. The
  bottom action row remains only `Duplicate`, `Edit Preset`, and `Start`.
  - standalone clone runtime-scene run:
    `./physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - workspace-parent runtime-scene run:
    `physics_sim/physics_sim_headless --runtime-scene <scene_runtime.json> --frames <n> --output-root <dir> --progress-interval <n> --save-volume-frames`
  - or run the standalone basin with
    `./physics_sim_headless --water-mode --water-level <0..1> --frames <n> --output-root <dir> --save-volume-frames`
  - pass `--volume-export-start-frame <n> --volume-export-stride <n>
    --volume-export-max-frames <n>` to write only selected retained VF3D/PACK
    and water-surface sidecar frames during long warm-up runs
  - output includes `run_summary.json` under the selected output root
  - `run_progress.json` now advances inside a frame instead of only at frame boundaries
  - volume exports keep the existing `volume_frames/<Preset>/` layout with
    `.vf3d`, `.pack`, `manifest.json`, and `scene_bundle.json`
  - standalone Water mode also writes `water_manifest_v1.json` and
    `water_surface_%06d.json`; `scene_bundle.json.water_source` points to the
    water manifest while `scene_bundle.json.fluid_source` remains the VF3D
    volume contract
  - `--water-object-fixture` / `--water-pool-submerged-solid` enables the
    deterministic object-in-water fixture for backend review; the first
    response is an export-side displacement approximation over the existing
    solid-mask path, not a full two-phase CFD solver
  - skip-present volume-only runs avoid SDL video and renderer initialization
  - presented runs still use the existing window/Vulkan renderer path
  - skip-present Wind `--save-render-frames` runs can write nonblank
    `render_frames/` BMPs through a renderer-free diagnostic fallback. The
    fallback supports `--wind-visual-mode` views for oblique flow/speed,
    speed deficit, vorticity, object mask, mid-depth speed deficit, and
    mid-depth vorticity, plus oblique volume speed-deficit/vorticity views
    with depth-projected inlet dye, particle streaks, and visible solid-mask
    obstacle overlays. These fallback views are diagnostic artifacts; the
    product direction is an in-app Wind Tunnel Inspector sourced from actual
    solver/analyzer fields.
  - Wind long-tunnel visual proof is available with
    `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-visual`;
    it validates the long-box tunnel fixture, nonblank and changing analyzer
    projection BMPs, final Wind metrics, and nonblank headless render-frame
    fallback output without SDL video initialization
  - Wind long-tunnel MP4 proof is available with
    `make -C physics_sim test-physics-sim-headless-wind-long-tunnel-video`;
    it defaults to the `volume_vorticity` diagnostic view in the `high`
    quality profile, encodes fallback frames to
    `tmp/headless_wind_long_tunnel_video/wind_long_tunnel_oblique.mp4` and
    removes transient BMP frames after successful encode. The volume diagnostic
    view is still a renderer-free software visualization, but it now reads as a
    3D tunnel: the obstacle is visible in depth, and the moving tracer/dye cues
    are distributed through the inlet volume. The tracer population now includes
    object-adjacent seeds, and volume streaks integrate backward through the
    exported `velocity_x/y/z` field with modest cross-flow visual gain so the
    wake is easier to inspect in low-resolution smoke videos. The video smoke
    fails if the first and final render BMPs are identical. The Wind backend
    now advects inlet density as a persistent dye/smoke scalar along left/right
    tunnels, so `flow` mode can show a solver-owned plume and obstacle shadow
    rather than only cosmetic inlet bands. The current scalar transport is still
    an axis-aligned first pass, and the solver state still reaches a mostly
    steady field rather than a long, turbulent wake trail, so multidirectional
    scalar advection and true wake relaxation/advection remain the next solver
    boundary. Wake velocity/pressure perturbations now also advect downstream
    with decay in left/right tunnels instead of being erased by each baseline
    corridor write. The obstacle wake injector is now a near-object source, so
    downstream deficit structure is carried by the solver state rather than
    repainted across the full tunnel every frame. `volume_speed_deficit` is the
    clearer visual proof for this behavior; aggregate vorticity metrics are
    still dominated by the near-object source region. The Wind analyzer now
    reports a one-object proof readout derived from the aggregate solid-mask
    bounds: object drag availability, solid-cell count, projected area,
    upstream/downstream stagnation-pressure proxy averages, signed positive
    object pressure delta, and positive object drag-pressure proxy magnitude.
    The left/right wake source now uses an upstream stagnation region, a
    downstream suction core, and time-phased lateral vortex-shedding lobes
    instead of only adding positive downstream pressure. This is intentionally
    still an aggregate one-obstacle proxy, not a per-object-id table or final
    surface-force integration. The `volume_speed_deficit` fallback now also
    boosts only the detected downstream object wake corridor, using
    deficit-driven opacity/mark size and a longer backend source so the wake
    shading is visibly stronger without saturating the whole tunnel.
  - Wind object comparison proof is available with
    `make -C physics_sim test-physics-sim-headless-wind-object-comparison`.
    It runs the long-tunnel box, sphere (`object_type: "circle"`), and slim-box
    fixtures through the same renderer-free `volume_speed_deficit` view, writes
    `tmp/headless_wind_object_comparison/object_comparison_summary.txt`, and
    validates nonblank/changing render frames plus distinct projected-area and
    drag-pressure proxy values. This is still a supported-primitive comparison:
    true arrowhead/wedge behavior requires a native single-object primitive or
    per-object force/readout IDs before it can be reported cleanly.
  - The next `3D` Wind product boundary is not more MP4 tuning. It is a
    user-facing Wind Tunnel Inspector with field slices, streamlines/pathlines,
    inlet/outlet labels, object readout, and metrics sampled from the live
    solver/analyzer state.
  - The Wind backend now applies a bounded obstacle wake pass after the uniform
    corridor throughflow. For left/right tunnels, the pass derives one aggregate
    solid-object bounds volume, then applies a downstream velocity deficit,
    capped cross-flow swirl, and pressure variation in the exported velocity
    field behind that object. The volume particle diagnostic
    samples those cross-flow components, so tracers visibly bend around the
    obstacle instead of only moving straight through the tunnel. This is still
    an approximate wake model, not full CFD turbulence or object-surface force
    integration.
- Detached agent supervision can now bypass a live terminal:
  - build with `make -C physics_sim physics-sim-job-runner`
  - submit `physics_sim/physics_sim_job_runner submit --request <request.json>`
  - inspect `physics_sim/physics_sim_job_runner status --job-id <job_id>`
  - cancel `physics_sim/physics_sim_job_runner cancel --job-id <job_id>`
  - or let `bin/run_trio_detached_job_chain.sh` submit PhysicsSim on behalf of
    a chained LineDrawing -> PhysicsSim -> RayTracing run root
- Dense retained-runtime `3D` domains now enforce a resident-memory budget before backend allocation:
  - oversized volumetric domains are downscaled by increasing voxel size rather than attempting full dense allocation at the requested resolution
- The `3D` scaffold backend no longer requires full dense field residency as its always-live source of truth:
  - persistent fluid state is stored in app-local sparse bricks
  - solver steps materialize bounded dense work regions around active bricks instead of solving over the whole domain every frame
  - obstacle occupancy is tracked at both cell and brick scope so enforcement and solver-region expansion can skip fully empty brick lanes
  - solver-region scheduling is brick-aligned and pulls in nearby obstacle bricks instead of relying on whole-volume obstacle scans
  - compatibility slices and export/debug volume views are derived readouts rather than the authoritative storage path
  - a dense mirror is kept only for bounded test-scale domains so existing small-domain contract lanes remain inspectable without reintroducing high-end dense residency
  - live `3D` stepping is sparse-authoritative even when the bounded dense mirror exists; the step path no longer resyncs sparse truth from dense arrays before solve
  - backend reporting now exposes sparse-runtime region metrics, dense-mirror-live state, and an explicit solver-region cell budget/guard result
  - oversized sparse solver regions now stop at an explicit guardrail instead of silently rematerializing arbitrarily large dense work buffers
  - live `3D` stepping now schedules connected active-brick clusters instead of one global padded region:
    - distant active plumes solve independently when their padded solver regions do not overlap
    - overlapping padded regions are merged before solve so nearby activity still behaves as one safe cluster
    - backend reporting now exposes solver-cluster count, max per-cluster solver cell count, and cluster-limit fallback state
  - obstacle authority beneath the sparse runtime is now brick-local first:
    - live obstacle writes, rebuilds, solver-region solid-mask materialization, and obstacle enforcement use per-brick occupancy masks
    - the dense whole-domain obstacle mask is now a derived compatibility/export/readout cache instead of the live control plane
    - debug/export/test surfaces can explicitly zero the dense obstacle cache and still recover truthful obstacle state from the brick-local masks
  - support-surface cache paths are now materially sparser:
    - full-volume export cache rebuilds now materialize only allocated fluid bricks and occupied obstacle bricks instead of rescanning the full domain cell-by-cell
    - sparse debug-volume stats now derive from active fluid cells and occupied obstacle bricks without forcing an export-cache rebuild first
    - retained runtime overlay peak-density readout now prefers backend-reported sparse stats and only falls back to a view scan when a report value is unavailable
    - sparse reporting no longer treats export-cache materialization as an implicit side effect of ordinary debug/report queries
  - the first runtime control-plane hardening slice is now live:
    - sparse cluster scheduling now reports solved cluster count, skipped cluster count, and skipped solver-cell count for oversized-region guard cases
    - the first-pass `3D` solver now applies a bounded pre-advection velocity safety clamp to prevent per-step displacement spikes from outrunning the sparse work region
    - backend reporting now exposes pre-clamp and post-clamp peak velocity/displacement metrics plus a post-project maximum absolute divergence residual
    - reporting and solver contract lanes now prove the new guard metrics route through the backend without depending on export or dense-cache side effects
  - the guard-threshold seam is now explicit instead of hardcoded-only:
    - `fluid.solver_region_cell_budget` can override the sparse solver-region cell budget
    - `fluid.max_velocity_displacement_cells` can override the pre-advection velocity-clamp limit
    - `3D` backend reporting now exposes both the effective values and whether each guard is running on defaults or config overrides
  - legacy export/parity/emitter contract lanes now read backend-owned `3D` truth views instead of writing directly through scaffold dense-state pointers
- Retained-scene save/reopen workflow exists with scene-library routing and catalog re-entry.
- The `3D` menu catalog now resolves retained scenes from the configured `Input Root` live:
  - the configured input root is treated as the source of truth for retained-scene discovery
  - direct child scene directories are scanned
  - one grouped area layer is also scanned (`<input-root>/<Area>/<Scene>/...`)
  - a directory is listed only when both `scene_authoring.json` and `scene_runtime.json` exist
  - retained-scene labels prefer authoring `scene_name` / `display_name` metadata when present, then fall back to directory names
- Catalog root selection is centralized through `physics_sim_runtime_scene_catalog_roots(...)` in `src/app/data_paths.c`:
  - configured input root
- Menu behavior now treats `Input Root` edits as live catalog changes rather than next-launch-only configuration drift.
- Stale retained-scene selection is cleared when the input root changes or the previous runtime path is no longer present in the refreshed catalog.
- The retained-scene `2D` editor viewport now partially adopts shared `core_viewport2d` math:
  - fit-to-bounds reset, cursor-anchor wheel zoom, drag-pan, and world/screen transforms route through the shared viewport contract
  - canvas rectangle choice, scene-world meaning, and broader editor gesture policy remain app-local
  - retained-scene `3D` orbit/distance/projection behavior still uses the app-local camera path
- The retained-scene `3D` editor viewport now derives orbit-distance and minimum-zoom limits from scene bounds, so large scenes can zoom farther out than the old fixed-distance ceiling allowed.
- Focused contract coverage exists for this lane:
  - `tests/scene_editor_scene_library_contract_test.c`
  - `tests/scene_editor_viewport_contract_test.c`
- Overlay writeback status:
  - `motion_mode` is runtime-consumed
  - `initial_velocity` persists through overlay/storage lanes but remains deferred until a full runtime sink is completed

## Structure
- Required scaffold lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active subsystem lanes:
  - `app`, `command`, `config`, `export`, `geo`, `import`, `input`, `physics`, `render`, `tools`, `ui`

## Verification Contract
- Build:
  - `make -C physics_sim clean && make -C physics_sim`
  - `make -C physics_sim clang-build`
  - `make -C physics_sim fisics-build`
  - `make -C physics_sim dump-sema`
  - `make -C physics_sim dump-sema-runtime-3d-solver-step`
  - `make -C physics_sim dump-sema-fluid2d`
  - `make -C physics_sim dump-sema-rigid2d`
  - `make -C physics_sim dump-sema-rigid2d-collision`
  - `make -C physics_sim dump-sema-structural-runtime`
  - `make -C physics_sim dump-sema-particles2d`
  - `make -C physics_sim dump-sema-structural-solver`
  - `make -C physics_sim dump-sema-atmospheric-field`
  - `make -C physics_sim dump-sema-object-manager`
  - `make -C physics_sim dump-sema-soft-body`
  - `make -C physics_sim dump-sema-runtime-fields-2d`
  - `make -C physics_sim dump-sema-runtime-backend-2d`
  - `make -C physics_sim dump-sema-runtime-backend-3d-emitters`
  - `make -C physics_sim dump-sema-runtime-backend-3d-runtime`
  - `make -C physics_sim dump-sema-runtime-backend-3d-obstacles`
  - `make -C physics_sim dump-sema-runtime-emitter`
  - `make -C physics_sim dump-sema-runtime-obstacle`
  - `make -C physics_sim test-rigid2d-collision-contract`
  - `make -C physics_sim test-sim-runtime-backend-2d-runtime-fields-contract`
  - `make -C physics_sim test-sim-runtime-emitter-contract`
  - `make -C physics_sim test-sim-runtime-obstacle-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-attached-emitter-contract`
  - `make -C physics_sim test-sim-runtime-backend-3d-obstacle-contract`
  - `make -C physics_sim toolchain-contract`
- Stable validation:
  - `make -C physics_sim test-fast`
  - small deterministic non-GUI lane for common source changes; excludes
    headless integration, package, local-system/private-path, long
    visual/video, and remote-worker probes
  - includes `test-scene-menu-layout-contract`, which validates menu settings
    toggles and both legacy `Data I/O + Batch` and scene-project
    `Scene Project Cache` rectangles for normal and minimum window sizes so
    cache/headless/bottom actions do not overlap and mode-specific controls do
    not leak into the wrong panel
  - `make -C physics_sim test-stable`
  - includes 3D export contract/parity, retained-scene bridge coverage, and the shared-viewport-backed scene-editor viewport contract
- Smoke wording note:
  - `make -C physics_sim run-headless-smoke`
  - currently aliases `test-stable` rather than a separate long-lived runtime shell
- Direct headless CLI:
  - `make -C physics_sim physics_sim_headless`
  - `make -C physics_sim test-physics-sim-headless-cli`
  - `make -C physics_sim test-physics-sim-headless-scene-project-cache-output`
  - `make -C physics_sim test-physics-sim-headless-water-mode`
- Wind orientation probes:
  - `make -C physics_sim test-physics-sim-headless-wind-orientation-probe`
    is portable and uses a checked-in mesh-wedge fixture
  - `make -C physics_sim test-physics-sim-headless-dragonwind-orientation-probe`
    is a local-system probe because it defaults to a user-machine DragonWind
    runtime scene unless `DRAGONWIND_ORIENTATION_PROBE_SCENE` is supplied
- Detached runner:
  - `make -C physics_sim physics-sim-job-runner`
  - `make -C physics_sim test-physics-sim-job-runner-smoke`
  - `make -C physics_sim test-physics-sim-job-runner-policy`
- Build-only readiness:
  - `make -C physics_sim visual-harness`
- Source-run visual proof:
  - `make -C physics_sim visual-artifact`
  - writes a validated Wind projection BMP under ignored
    `visual_artifacts/source_first_frame/` and prints the final artifact path
  - uses `physics_sim_headless` with a checked-in fixture; does not require
    package launch, desktop capture, remote workers, or private scene paths
- Packaging verification:
  - `make -C physics_sim package-linux-worker-self-test`
  - `make -C physics_sim package-linux-worker-dry-run`
  - `make -C physics_sim package-linux-desktop-contract`
  - `make -C physics_sim package-linux-desktop-determinism-test`
  - `make -C physics_sim package-desktop`
  - `make -C physics_sim PACKAGE_TOOLCHAIN=fisics package-desktop`
  - `make -C physics_sim package-desktop-smoke`
  - `make -C physics_sim package-desktop-self-test`
  - `make -C physics_sim package-desktop-refresh`
- Legacy lane (known stale/failing tests can exist here by design):
  - `make -C physics_sim test-legacy`

## Release and Packaging Snapshot
- Release-readiness phases are complete through artifact flow (`RL0`-`RL3`).
- Signed/notarized/stapled distribution flow is established for production release operations.
- Package/release helper security boundary is local-operator only:
  package/release targets quote configured paths and may destructively clean
  their configured package/release/Desktop roots; app packaging stages only the
  binary, launcher, `Info.plist`, bundled dylibs, config, shader resources,
  optional icon, and optional shared fonts; Linux worker packaging stages only
  headless/job-runner binaries, config, selected public docs, and manifests;
  private/generated run lanes such as `build/agent_runs/`, `tmp/`, detached
  outputs, launcher logs, maintainer-private artifact roots, and local icon
  source copies are not package inputs unless a future target explicitly stages
  them.
- Linux worker packaging now emits truthful host-architecture metadata for
  either `linux-x86_64` or `linux-aarch64` by default:
  - `make -C physics_sim package-linux-worker`
  - `make -C physics_sim package-linux-worker-dry-run`
  - the package manifest platform follows the Linux build host architecture
  - `LINUX_WORKER_PLATFORM=<value>` remains available for explicit override
  - the archive is accompanied by `.tar.gz.sha256` and deterministic
    `.tar.gz.manifest.txt` sidecars so generic portable executors can discover
    one exact archive/checksum/manifest set
- `make -C physics_sim test-linux-worker-artifact-manifest` exercises the real
  Make producer with local stub executables and verifies the exact sidecar
  naming, content, checksum binding, and deterministic regeneration.
- `package-linux-worker-dry-run` is local-only validation. It validates the
  staged manifests, entrypoints, selected docs/config payload, archive contents,
  package-manifest self-test command, and each executable's required GLIBC
  symbol ceiling without upload, install, registration, raw SSH/SCP, or remote
  worker execution. The default fleet ceiling is GLIBC 2.39 and is recorded as
  `max_glibc_version` in both worker manifests.
- Worker package/cross-host handoff boundary is explicit: the local Linux
  worker archive does not install, upload, run, or register itself; worker-safe
  payloads must stage runtime scene, run config, runtime mesh documents,
  preview sidecars, and output/report roots together; Linux PC package
  upload/install/status/fetch routes through the Linux PC handoff lane, while
  VPS worker-fleet visibility, worker-exchange requests, and VPS-side runtime
  proofs route through the VPS handoff lane. Raw SSH/SCP or ad hoc remote shell
  is outside the package boundary.
- Private Linux GUI desktop packaging now has first scaffold targets:
  - `make -C physics_sim package-linux-desktop-contract`
  - `make -C physics_sim package-linux-desktop`
  - `make -C physics_sim package-linux-desktop-self-test`
  - `make -C physics_sim package-linux-desktop-determinism-test`
  - package class: `desktop_app_linux`
  - artifact role: `desktop_app`
  - runtime: `linux_gui`
  - private artifact name:
    `kinetiC-<version>-linux-x86_64-desktop-stable.tar.gz`
  - checksum sidecar:
    `kinetiC-<version>-linux-x86_64-desktop-stable.tar.gz.sha256`
  - this target is Linux-only and refuses real packaging on macOS; the local
    Mac can verify the contract shape but not the GUI package build
  - the Linux launcher runs from an unpacked archive, copies package resources
    into XDG/proof runtime roots, writes launcher logs under XDG state or an
    override, and supports display-free `--self-test`
  - real release-grade GUI proof still requires the Mac/Linux PC handoff lane:
    deterministic package target, sidecar verification, clean unpack
    self-test, real logged-in desktop launch, app-window screenshots, and
    launcher log/runtime markers
  - no `VERSION` bump, public artifact, website metadata, registry mutation,
    worker install, remote job submission, or RayTracing artifact change is
    implied by this scaffold
- The first compiler-overlay dual-toolchain contract is now active for app-local validation:
  - `clang-build` writes the default app binary to `build/clang/physics_sim`
  - `make` still copies the Clang binary to the repo-root `physics_sim` path for compatibility
  - `fisics-build` writes the overlay-enabled binary to `build/fisics/physics_sim`
  - `dump-sema` targets the retained-scene projection-domain seam under `src/import/`
  - `dump-sema-runtime-3d-solver-step` targets the first `3D` solver-step math seam under `src/app/`
  - `dump-sema-fluid2d` targets the legacy `2D` fluid solver seam under `src/physics/`
  - `dump-sema-rigid2d` targets the rigid-body solver seam under `src/physics/`
  - `dump-sema-rigid2d-collision` targets the rigid-body collision/impulse seam under `src/physics/`
  - `dump-sema-structural-runtime` targets the structural dynamic-runtime integrator seam under `src/app/structural/`
  - `dump-sema-particles2d` targets the `2D` particle integrator seam under `src/physics/particles/`
  - `dump-sema-structural-solver` targets the structural static solver seam under `src/physics/structural/`
  - `dump-sema-atmospheric-field` targets the atmospheric density/wind field generator seam under `src/app/atmospheric/`
  - `dump-sema-object-manager` targets the rigid object-manager seam under `src/physics/objects/`
  - `dump-sema-soft-body` targets the soft-body integrator seam under `src/physics/soft/`
  - `dump-sema-runtime-fields-2d` targets the `2D` backend runtime-fields/emitter seam under `src/app/`
  - `dump-sema-runtime-backend-2d` targets the adjacent `2D` backend host/runtime seam under `src/app/`
  - `dump-sema-runtime-backend-3d-emitters` targets the `3D` scaffold emitter application seam under `src/app/`
  - `dump-sema-runtime-backend-3d-runtime` targets the `3D` scaffold runtime region-step seam under `src/app/`
  - `dump-sema-runtime-backend-3d-obstacles` targets the `3D` scaffold obstacle enforcement/materialization seam under `src/app/`
  - `dump-sema-runtime-emitter` targets the emitter support seam under `src/app/`, covering resolved `position_z`, resolved `radius`, and the `3D` world-space-to-grid placement bridge
  - `dump-sema-runtime-obstacle` targets the obstacle support seam under `src/app/`, covering storage/compatibility policy, source-footprint routing, and domain-face slab bounds
  - `test-rigid2d-collision-contract` directly validates the deeper rigid-body collision manifold seam under `src/physics/rigid/`
  - `test-sim-runtime-backend-2d-runtime-fields-contract` directly validates the first bounded `2D` runtime-fields/emitter seam under `src/app/`
  - `test-sim-runtime-emitter-contract` directly validates the bounded emitter support seam under `src/app/`
  - `test-runtime-mesh-preview-bridge-contract` validates runtime-scene mesh
    preview metadata, sidecar probing, and PhysicsSim fluid-role metadata
  - `test-runtime-mesh-obstacle-proxy-contract` validates default-solid runtime
    mesh obstacle behavior, actual runtime mesh voxelization, and emitter
    exclusion from solid occupancy
  - `test-runtime-scene-bridge-contract` validates projection from runtime
    scene mesh metadata into attached runtime-mesh emitters
  - `test-sim-runtime-backend-3d-runtime-mesh-emitter-contract` validates that
    mesh-attached emitters use actual runtime mesh footprints in the 3D backend
  - `test-sim-runtime-obstacle-contract` directly validates the bounded obstacle support seam under `src/app/`
  - `test-sim-runtime-backend-3d-emitter-contract` directly validates the bounded free-emitter `3D` scaffold seam under `src/app/`
  - `test-sim-runtime-backend-3d-attached-emitter-contract` directly validates the bounded attached-emitter `3D` scaffold seam under `src/app/`
  - `test-sim-runtime-backend-3d-obstacle-contract` directly validates the bounded `3D` scaffold obstacle seam under `src/app/`
  - `package-desktop` still packages the Clang build by default
  - `PACKAGE_TOOLCHAIN=fisics` is required to package the overlay-enabled binary intentionally
- Core release utilities:
  - `make -C physics_sim release-contract`
  - `make -C physics_sim release-verify-signed ...`
  - `make -C physics_sim release-notarize ...`
  - `make -C physics_sim release-staple`
  - `make -C physics_sim release-verify-notarized`
  - `make -C physics_sim release-artifact`
  - `make -C physics_sim release-distribute ...`

## Runtime Config and Data Policy
- Tracked defaults: `config/`
- Runtime mutable state: `data/runtime/`
- Generated/temp lanes are intentionally kept out of tracked defaults policy.

## Current Boundary
- `physics_sim` producer export lane is intentionally stable.
- The current local `3D` fluid-runtime boundary remains inside runtime control-plane hardening:
  - the first solver-safety/observability slice and its config seam are landed
  - the next refinement step is to widen region/materialization and solver-health diagnostics, then decide whether any of the new control seams should graduate into authored menu/runtime UI before deeper solver-quality changes
- Current local worktree drift is in retained-scene quality/usability, not in the producer export contract:
  - input-root scene-library refresh behavior
  - paired `scene_authoring.json` + `scene_runtime.json` discovery
  - shared `core_viewport2d` adoption in retained-scene `2D` editor mode
  - large-scene `3D` viewport zoom/orbit range
- Current atmospheric initializer follow-up is user runtime validation, then a Phase 5 planning boundary for volumetric rendering/cross-app handoff.
- The next major cross-program boundary is still downstream in `ray_tracing`:
  - ingest `vf3d` / `VF3H`
  - consume truthful scene metadata
  - land first-pass density-driven volume rendering
- The current local compiler-units boundary now includes the structural
  dynamic-runtime lane and the `2D` particle integrator lane in addition to
  the rigid-body lane:
  - all first-party runtime declarations in `src/` and `include/` now use the
    public `FISICS_DIM(...)` / `FISICS_UNIT(...)` portability surface from
    `<fisics/extensions.h>`; ordinary Clang builds erase the annotations while
    fisiCs semantic-dump lanes retain their physical meaning
  - direct `[[fisics::dim(...)]]` and `[[fisics::unit(...)]]` spellings are no
    longer present in first-party runtime source
  - rigid-body lane remains active:
    - `src/physics/rigid/rigid2d.c` now has an explicit semantic-dump target
      and time-typed `dt` at the solver entry
    - `src/physics/rigid/rigid2d_collision.c` now carries the first deeper
      collision-time seam through typed `dt` and explicit inverse-step fallback
  - structural dynamic-runtime lane is now also a semantic-dump customer:
    - `src/app/structural/structural_controller_runtime.c`
    - `structural_controller_runtime_step_dynamic(...)` now accepts time-typed
      `dt`
    - the gravity-ramp time bookkeeping is now explicit physical time instead
      of plain scalar seconds
    - the translational runtime clamp path now makes displacement length and
      derived translational speed explicit before writing back to the runtime
      state arrays
    - the translational Newmark predictor/corrector and explicit-integrator
      update equations now also carry explicit physical meaning for:
      - position/displacement length
      - translational velocity
      - translational acceleration
    - the remaining translational dynamic seam inside the file is now also
      explicit:
      - mass-proportional translational damping now routes through a typed
        helper
      - explicit translational net-force-to-acceleration now routes through a
        typed helper before updating runtime acceleration/velocity/position
  - this is a cleaner next customer than the static structural stiffness core
    because the dynamic runtime already exposes honest time, displacement, and
    speed boundaries without first needing derived-unit cleanup for area,
    inertia, or stiffness storage
  - `2D` particle integrator lane is now also a semantic-dump customer:
    - `src/physics/particles/particles2d.c`
    - `particles2d_step(...)` now accepts time-typed `dt`
    - particle lifetime decrement is now explicit physical time
    - gravity application is now explicit translational acceleration into
      translational velocity
    - position integration is now explicit translational velocity into length
  - the particle/fluid compatibility seam is now isolated into a named bridge:
    - fluid-grid velocity sampling intentionally remains scalar
    - the particle lane now reuses the shared `fluid2d_sample_velocity(...)`
      API instead of owning a second inline bilinear sampler
    - particle positions are still sampled directly in legacy grid coordinates
    - the compatibility blend now lives in one helper instead of being mixed
      inline with the rest of the particle integrator
  - the structural static solver lane is now also a semantic-dump customer:
    - `src/physics/structural/structural_solver.c`
    - member-axis geometry now routes through one typed helper
    - nodal planar load application now routes through a typed force helper
    - cross-sectional area and second moment of area now route through typed
      section-property helpers, and axial stress now resolves through an
      explicit force-over-area pressure helper
    - repeated material/stiffness formulas now also route through localized
      typed helper seams for Young's modulus, axial stiffness, and bending
      stiffness
    - the remaining limit is still runtime storage and assembly vocabulary:
      the solver does not yet carry full modulus/stiffness families through
      scene storage or matrix assembly outputs
  - the atmospheric field generator is now also a semantic-dump customer:
    - `src/app/atmospheric/atmospheric_field.c`
    - atmospheric sample velocity fields now carry explicit velocity semantics
    - localized base-wind and turbulence-strength locals now route the `2D`
      and `3D` curl-noise wind expressions through one typed helper seam
    - the remaining limit is still vocabulary coverage:
      density remains scalar, and normalized sample/noise coordinates remain
      intentionally untyped until a real density-family contract exists
  - the rigid object-manager is now also a semantic-dump customer:
    - `src/physics/objects/object_manager.c`
    - `object_manager_step(...)` now carries explicit time semantics at the
      rigid-world handoff into `rigid2d_step(...)`
    - circle radius, box half-extents, and object position now route through a
      localized length helper seam before writing into body storage
    - the remaining limit is still polygon/local-vertex cleanup:
      `add_poly(...)` still accepts raw local vertex arrays without a typed
      per-vertex length contract
  - the deeper rigid collision manifold lane is now also directly validated:
    - `src/physics/rigid/rigid2d_collision.c`
    - typed `dt` and explicit inverse-step fallback remain in place
    - restitution thresholding now routes through a typed
      relative-normal-velocity helper
    - penetration bias velocity now routes through a typed
      penetration-length / inverse-time helper
    - tangent-speed extraction and the first friction threshold checks now
      route through typed velocity helpers instead of raw scalar compares
    - `make -C physics_sim test-rigid2d-collision-contract` now directly
      exercises the bounded manifold lane with isolated restitution and
      friction checks
  - the soft-body reference solver lane is now also a semantic-dump customer:
    - `src/physics/soft/soft_body.c`
    - `soft_body2d_step(...)` now carries explicit time semantics
    - the file now owns a bounded real solver reference:
      node storage, spring storage, pinned-node semantics via `mass <= 0`,
      gravity force accumulation, spring pull/damping, explicit
      velocity -> displacement -> position integration, and iterative
      spring-constraint relaxation with velocity rebuild from corrected
      positions, plus triangle area constraints for basic shape preservation
    - `make -C physics_sim test-soft-body-contract` now exercises the lane
      directly
    - the remaining limit is still solver depth:
      this is a bounded spring/area reference, not a full collision-aware or
      volume-preserving production soft-body system
  - the `2D` backend runtime-fields lane is now also a semantic-dump customer:
    - `src/app/sim_runtime_backend_2d_runtime_fields.c`
    - `backend_2d_apply_emitters(...)` now treats `dt` as explicit physical
      time with a typed zero-time no-op guard
    - the first honest emitter-physics slice is now explicit:
      - scalar total-strength accumulation still stays untyped because the
        file mixes density-source and velocity-source magnitudes
      - emitted velocity deltas for density-source, jet, and sink paths now
        route through typed velocity helpers before writing into `fluid2d`
      - the free-emitter radial injection path, shared-mask path, and
        attached-object/import footprint application path now all route
        through named helpers instead of inline mask-building branches
      - the moving-obstacle mask + obstacle-velocity writeback path now also
        routes through localized helpers instead of an inline import/circle/poly
        splat branch
    - `make -C physics_sim test-sim-runtime-backend-2d-runtime-fields-contract`
      now exercises the bounded lane directly:
      - zero-`dt` emitter application is a no-op
      - free velocity-jet emission injects positive density and x-velocity
        into the `2D` field
      - attached-object velocity-jet emission injects density and x-velocity
        through the extracted footprint helper lane
      - a moving circle obstacle writes both obstacle mask coverage and
        obstacle x-velocity through the extracted runtime-field bridge
    - the remaining limit is no longer the obstacle bridge:
      scalar grid-cell geometry and mixed density/velocity strength semantics
      remain compatibility carriers rather than honest world-unit lanes

- The next compiler-units boundary after the localized `2D` runtime-fields
  bridge slice is now also landed in the adjacent backend host file:
  - `src/app/sim_runtime_backend_2d.c`
  - it is now the thirteenth semantic-dump customer in the rollout
  - the first bounded host/runtime seam now covers:
    - `backend_2d_apply_boundary_flows(...)` with typed `dt` and typed zero-time no-op handling
    - `backend_2d_step(...)` with the same typed zero-time guard at the host-step boundary
    - `backend_2d_inject_object_motion(...)` with typed object-body velocity locals and a localized velocity injection helper into `fluid2d`
    - `backend_2d_capture_atmospheric_seed_stats(...)` with typed wind-velocity locals and typed speed magnitude recovery
  - `make -C physics_sim test-sim-runtime-backend-2d-contract` now directly exercises the bounded host seam:
    - zero-`dt` boundary-flow application is a no-op
    - positive-`dt` boundary emission injects density and velocity into the field
    - object-motion velocity injection writes velocity into the `2D` fluid field
  - the remaining limit in this file is now host/control compatibility geometry rather than the first physical runtime seam
  - the adjacent `3D` scaffold lane now also has its first explicit sema coverage:
    - `src/app/sim_runtime_backend_3d_emitters.c`
      - emitter total-strength over `dt`, footprint world-volume / voxel-volume normalization, per-cell density and velocity deltas, and thermal buoyancy routing are now explicit semantic-dump customers
      - direct proof already existed and remains green:
        - `make -C physics_sim test-sim-runtime-backend-3d-emitter-contract`
        - `make -C physics_sim test-sim-runtime-backend-3d-attached-emitter-contract`
    - `src/app/sim_runtime_backend_3d_runtime.c`
      - the region-limited runtime step with typed `dt`, solver-region budgeting, and displacement-clamp metric recovery is now also an explicit semantic-dump customer
    - `src/app/sim_runtime_backend_3d_obstacles.c`
      - obstacle-volume rebuild, compatibility-slice sync, and live obstacle enforcement zeroing are now also explicit semantic-dump customers
      - direct proof already existed and remains green:
        - `make -C physics_sim test-sim-runtime-backend-3d-obstacle-contract`
    - together these three files move the remaining meaningful uncovered `3D` physics surface down into narrower support lanes rather than core solver/runtime math

## History and Deep Lane References
- Detailed execution slices, archived plans, and deep phase logs are kept in
  maintainer-private docs outside the public repository.
- Use this public file as the compressed current-state contract.

## Local agent session workspace (S1)

The source/Main Edit lane now includes a background Wind session worker, bounded
inspection, eight MCP tools, and a native workspace sharing the same run controls.
See [agent_session.md](agent_session.md) for build, connection and acceptance
contracts. This does not change the public desktop version or remote-worker API.

S2 adds three-plane live diagnostic slices, scalar/vector field inspection,
world-coordinate MCP probes, bounded health history, fixed-range comparison, and
native-density shared-font rendering in the agent workspace. See the S2 section
of [agent_session.md](agent_session.md) for sampling limits and model caveats.


## Main Edit S3 numerical qualification

The Main Edit session runtime now supports deterministic closed STL sphere/cube/cone
fixtures and matching runtime geometry, hash-bound mesh dependencies, editable
constant SI carrier density/dynamic viscosity, and a qualification mode that disables
synthetic Wind wake/corridor/carrier corrections. A tested velocity diffusion
operator uses nu*dt/h² with bounded stable diffusion substeps. Legacy viscosity is
unchanged when no physical fluid is supplied. Solver snapshots expose the discrete
Poisson residual and pre/post-projection divergence; source desktop inspection shows
residual and SI viscosity. Repeatable shape/fluid/grid/timestep sweeps produce
source-worker-bound JSON reports, sampled wake sections, PNGs, and before/after
comparisons. S3B also adds a matched divergence/transpose CG projection, opens
qualification Wind inlet/outlet faces that were incorrectly solid, and exposes
bounded final-snapshot volume/mass-flux, divergence, and kinetic-energy diagnostics.
Local projection residual is distinct from final-state conservation after boundary
writes. This is numerical qualification infrastructure and solver repair,
not validated predictive CFD or physical drag measurement. See
`docs/solver_qualification.md` for the acceptance boundary and remaining S3B/S3C work.

The subsequent S3 boundary slice constrains inlet velocity within one full-domain
qualification pressure solve, with a free outlet under the discrete transpose
operator. It exposes actual pressure iteration usage/convergence, permits an
explicit qualification budget up to 512 iterations, and rejects over-budget full
grids at validation. Physical outlet pressure and drag remain unvalidated.

S3 now includes sampled wake time histories, explicit warmup/two-window steady
screens, and matched spatial/temporal/outlet sensitivity assessment. These gates
reject insufficient, clamped, unconverged, drifting, or mismatched evidence.
Fixed object placement supports controlled outlet extension. Passing the numerical
screens does not qualify physical outlet pressure or surface forces.

The subsequent [S3 accuracy audit](solver_accuracy_audit.md) identifies measurable
inviscid transport damping and removes a per-tick uniform velocity reset at the
qualification outlet. The replacement copies adjacent velocity into the outlet
predictor before projection; legacy Wind is unchanged. Smaller-timestep,
finer-grid, and paired outlet runs still fail numerical sensitivity acceptance.
Pressure convergence is passing, but physical outlet/drag acceptance remains
blocked by transport, boundary, and resolution evidence. Desktop remains S2.

The next [transport slice](transport_accuracy.md) replaces qualification velocity
advection with bounded MacCormack correction and visibility-checked solid donor
sampling. Independent amplitude/phase, viscous transport, monotonicity and wall
separation tests cover the method. Session health reports the scheme and counts
of corrected, limited and fallback component samples. Legacy visual transport and
the installed Desktop remain unchanged. Improved transport accuracy does not
close physical outlet, momentum-conservation or force-acceptance gates.

A separate [incompressible channel verification slice](cfd_channel.md) is now
available through the existing local agent session tools (`cfd_channel` template,
`incompressible_channel_fv_v1` model). It numerically evolves fully developed
parallel-wall flow using finite-volume viscous stresses and implicit time stepping.
Physical pressure drop is prescribed; wall shear and flux are computed. It exposes
momentum/energy balances and physical pressure/shear samples with digest-bound
reduced-field results. Three-level analytical verification passes. This is a
reduced laminar baseline, not the general 2D/3D pressure-velocity solver, outlet,
turbulence or AMR implementation. Desktop remains unchanged.

A [staggered 2D incompressible channel](cfd_mac2d.md) now extends that baseline
through `cfd_channel_2d` / `incompressible_mac2d_v1`. It computes conservative
face momentum and spatial pressure correction, with actual divergence, pressure
residual, wall traction, flux and momentum diagnostics through the existing
agent contract. The 12-case channel campaign, pressure recovery/refinement,
solenoidal preservation, coupled agent run and regression/sanitizer checks pass.
This is periodic-X laminar verification with first-order upwind transport;
nonuniform transient accuracy, obstacles and physical outlets remain unqualified.
Desktop installation remains S2.

The [nonuniform MAC transient study](cfd_mac2d_transient.md) now passes temporal
self-convergence but fails the spatial velocity gate. Fine-grid timestep checks
show small temporal contamination; amplitude sensitivity suggests interacting
spatial errors. Production numerics are unchanged and physical force acceptance
remains open.

The [manufactured transient campaign](cfd_mac2d_manufactured.md) now provides
known-answer errors and separately controls transport, pressure timing and wall
gradients. A verification-only limited flux reduces fine-grid velocity error
about 22 times; production transport remains upwind pending boundedness and
long-run qualification. Obstacles, outlets and forces remain unqualified.

[Boundary traction and momentum qualification](cfd_boundary_forces.md) now
calibrates physical surface forces and exposes MAC `boundary_force_budget`
wall loads and explicit qualification status. Obstacle geometry, open outlets
and body drag remain unsupported/unqualified, separate from channel wall proof.

The [stationary obstacle projection](cfd_mac2d_obstacle.md) now implements
matched solid-face pressure topology and measured pressure reaction. Leakage,
known-pressure recovery and projection momentum tests pass. Full obstacle
time stepping remains explicitly disabled pending viscous/advective wall
momentum; agent obstacle authoring and open outlets remain unsupported.

The [masked obstacle momentum extension](cfd_mac2d_obstacle_momentum.md) now
advances complete stationary-obstacle steps and accounts for pressure/viscous
reactions. It supersedes the pressure-only stepping restriction. Physical drag,
agent obstacle authoring and open outlets remain unqualified/unavailable.

The [surface-pressure correction and CFD assessment](cfd_boundary_pressure_correction.md)
now reconstructs pressure at obstacle walls and recovers the .075 N baffle
reference at all tested grids. Discrete reactions and unresolved boundary-volume
drive remain separate; arbitrary drag and open outlets are still unqualified.
This remains a 2D CFD core; the existing 3D Wind path is separate.

The [active CFD baseline goal](cfd_baseline_goal_status.md) now has independent
obstacle surface/control-volume force comparison and a true nonperiodic
velocity-inlet/pressure-outlet prototype. Force accuracy is still failing its
consistency gate; outlet qualification and agent-lab integration remain incomplete.

CFD continuation: [agent lab](cfd_agent_lab.md) and [active baseline gates](cfd_baseline_goal_status.md) record the exploratory 2D obstacle interface, outlet-exit evidence, the corrected periodic-obstacle force gate, and remaining open-boundary/reference qualifications.


### Current CFD baseline continuation (supersedes earlier incomplete-gate notes)

The bounded 2D open-boundary baseline now passes channel recovery, signed
backflow, disturbance exit/domain length, fine-grid pulse energy and one confined
low-Re rectangle total-drag reference screen. Surface drag differs from the
independently refined Stokes reference by 1.393%; surrounding momentum drag by
1.115%. Reference outlet formulations differ and are screened by domain extension;
formal equivalence and general drag certification are not claimed.

The open channel/obstacle agent presets support live physical pressure, forces,
conservation diagnostics, histories, refinement comparisons and hashed exports.
All 19 agent regression tests pass. Initial paused flux/divergence readbacks now
come from actual fields. See [baseline evidence](cfd_baseline_goal_status.md) and
[agent laboratory](cfd_agent_lab.md). Individual force components, masked energy,
curved/moving bodies, turbulence and 3D CFD remain unqualified. This is Main Edit
source evidence; the Desktop package has not been refreshed by this work.


### Per-run acceptance and component/energy continuation

`run_assess` now reports explicit numerical acceptance for open 2D runs,
including every-step whole-field steady windows, conservation, energy,
force consistency and matched spatial/temporal refinement. A real channel
campaign passes all numerical gates; physical accuracy is never inferred.

Masked energy is now calibrated and passes the bounded 64² confined-rectangle
steady (.980%) and transient (1.082%, .1..1.1 s) 2% gates, including half-dt checks.
Corner-refined independent reference evidence revises separate pressure/viscous
errors to 8.48%/9.26% at 64². A verification-only 128² run reduces them to
5.58%/6.07%, still failing 2%. Total drag remains within 2%. The component gate
therefore remains open and 3D qualification has not started. See
[measured assessment](cfd_component_energy.md). Runtime agent grids remain capped
at 64; no Desktop package refresh or commit occurred in this continuation.

### Main Edit refined CFD development checkpoint

The [pre-3D resolution work](cfd_pre3d_resolution_goal.md) now includes sparse
balanced fixed local refinement, coupled velocity/pressure multilevel solving,
implicit viscosity, manufactured spatial/temporal verification, and separate
force/energy acceptance on the fixed confined steady Stokes rectangle. The older
uniform MAC component failures above remain historical/model-specific; they do
not describe the new `incompressible_refined2d_v1` path.

The existing local agent session exposes refined scene templates, SI fluids,
fixed refinement regions, numerical memory limits, asynchronous controls,
physical-pressure sampling, digest-bound leaf/face artifacts, refinement
comparisons and explicit reference-accuracy statuses. Mesh levels and actual
minimum spacing, setup/transport/solve/observation CPU costs, previous-publication
and final-export wall costs are inspectable. The smooth channel template defaults
to uniform base cells; body scenes retain local refinement. No default is an
accuracy certificate.

Physical scope remains bounded 2D laminar verification with a fixed stationary
aligned rectangle. No moving-body, arbitrary STL, turbulence, general transient
force or 3D certification is implied. Numerical budgets exclude JSON and total
process RSS; benchmark reports disclose RSS separately. This is Main Edit source
work, not canonical adoption or an installed Desktop package refresh.

Fixed velocity-PC correction checkpoint: [diagnostic](cfd_3d_velocity_refine_checkpoint.md).
Inner inverse accuracy improves but exact cube reaches180s with no field; no adoption.
Pressure localization/conditioning is the next bounded investigation.

Exact cube [pressure coverage/conditioning diagnostic](cfd_3d_pressure_modes_checkpoint.md)
completed under caps. Sampled scale disparity motivates complementary pressure mass
scaling; outside-span residuals are retained and full spectrum is not certified.

[Complementary pressure scaling10](cfd_3d_complement10_checkpoint.md) qualifies the
optional exact L8 reference recipe:168iter/114.126s/1531.141MiB/full1.020e-11,
60.6percent fewer iterations versus accepted mass. Bounded reference force tests
can resume. Original raw1.757percent force mismatch remains failed; native/general
physical certification and desktop adoption remain open.

[Matched shorter-domain proof](cfd_3d_force_resume_checkpoint.md): same complement10
recipe completes L4 full1.204e-11/85.314s/1360.875MiB; original force/domain energy
failures remain. Both domain lengths are ready for bounded mesh/force experiments.

[Six-interval edge-strip screen](cfd_3d_edge_redistribution_checkpoint.md): all three
profiles rejected on both domains before factorization because local/global shape
or conditioning worsened. No candidate field or force accuracy claim.

[Balanced eight-interval screen](cfd_3d_balanced_edge8_checkpoint.md): edge/global
quality improves, but both profiles fail the outer body band; no numerical solve.

[Minimum-width finer grid screen](cfd_3d_floor_edge8_checkpoint.md): all local bands
pass, but global volume-weighted shape fails on both domains. No numerical solve.

[Quality-passed finer cube](cfd_3d_floor_balanced8_checkpoint.md): paired finer geometry
passes all original shape gates. Its one L4 numerical attempt rejects at fresh memory
admission2087.527MiB>1800, before factor/solve/field. Velocity factor dominates;
[bounded preconditioner memory reduction](cfd_3d_floor_balanced8_next_goal.md) is next.

[Fixed-pattern velocity factor](cfd_3d_block_ic0_checkpoint.md): independent controls
pass, but the small cube fails balanced velocity coarse reproduction before a solve.
[Positive omitted-fill compensation](cfd_3d_fillcomp_ic0_checkpoint.md) passes setup
but reaches3000iterations with original full residual.71075, dominated by momentum.
Both remain rejected optional research candidates; successful pressure complement10
plus complete Cholesky remains the qualified reference recipe.

[Equal-central-interval screen](cfd_3d_equidistributed6_checkpoint.md) rejects smaller
first strips on local conditioning. [Held-first-strip force sensitivity](cfd_3d_heldfloor6_checkpoint.md)
passes paired geometry and produces a strict full L4 field:78iter/82.627s/
1296.141MiB/full8.905e-12. Force testing has resumed; separate components change
less than.355percent, but raw surface/reaction mismatch worsens1.7702->1.8339percent.
Decline mesh promotion and conditional longer-domain follow-up. No physical/native
certification. [Next controlled-fill investigation](cfd_3d_heldfloor6_next_goal.md).

[Controlled-fill velocity factor](cfd_3d_bounded_fill1_checkpoint.md): eight support
controls pass; degree ordering and limited level-one fill reduce compensation,
but original small-cube restart6 hits3000iterations/full residual.6380. No field
or larger trial. [Larger-basis control](cfd_3d_fill1_restart30_checkpoint.md) keeps
physical operator and PC identical; four support controls pass, actual V/Z/work
fully reserved. Restart30 improves full residual to.002305 at3000iter/40.210s,
still far above strict1e-10. No numerical/physical/default adoption. The subsequent
[distributed macro P2 investigation](cfd_3d_distributed_p2_checkpoint.md) is now
measured and rejected: full9.966e-7 at3000/64.873s. Follow-up cubic controls below
reach strict full convergence but remain subject to separate usefulness gates.


[Cubic velocity correction](cfd_3d_distributed_p3_checkpoint.md): eight new support
controls pass, original fullphysical equations retained. DistributedP3/localtriple
produces strict original4992tet field at498iter/43.358s/full1.061e-11, but misses
unchanged23.502s whole usefulness limit. Nonlinear[CG8 pressure setup](cfd_3d_distributed_p3_cg8_checkpoint.md)
is rejected by unchanged symmetry gate before anyouter solve. Separately[fixed
pressureproxy plus flexibleCG8](cfd_3d_distributed_p3_cg8_pressure_checkpoint.md)
passes alloriginalfullchecks at113iter/24.321s/full8.935e-12, stillcost-rejected.
[One-pass coarse action](cfd_3d_p3_cg8_scalar_checkpoint.md) is39.58percent faster
in independentactioncontrol and reaches113iter/23.636s/full9.014e-12, narrowly over
same23.502s gate. [CG16](cfd_3d_p3_cg16_scalar_checkpoint.md) reaches105iter/29.993s/
full8.207e-12; moreinner work is slower and rejected. Allseparateforce/scalar outputs
match qualifiedoriginalcube within1e-7; completedowners/bases retire before fullFE.
35 new independent passed controls across thisbatch, plus immutableprior proof;
native/default/accepted exactcoupled complement10 recipe unchanged. No larger/finer
trial admitted by thesecandidates, no physicalforceconvergence certification.
[Next pressureproxy and modecoverage diagnostic](cfd_3d_p3_cg16_scalar_next_goal.md)
will distinguish the remainingcost/iteration floor before another distinctcandidate.
Quality-passed43008tet finerL4 remains withheld; exactfactor gap287.527MiB against
1800MiB cap preserved. Existingmatched L4/L8 and heldfloor6 force-sensitivity fields
remain available, with rawforce/energy acceptance failures explicitly retained.

## GrowthSim surface-source communication (2026-10-04)

The source-only offline adapter now admits GrowthSim Fire v1 copied frames/bundles
under an explicit pinned policy and journals replay-safe admission/allocation plans.
Area intersections and half-open uniform-rate substeps preserve transferred J/kg
budgets on a declared stationary XY fluid-facing layer. This general fluid/atmosphere
contract lane does not require a wind-tunnel scene and does not alter native solver,
fields, time, forces, live MCP capabilities or installed packages. See
[commands and acceptance boundaries](surface_source_admission.md). Thermal state,
physical injection, buoyancy and atomic fluid/source restart remain unimplemented.


## Passive atmosphere source milestone (2026-10-04)

The offline body-free periodic constant-property passive3d model is implemented
and independently qualified in Main Edit source. It carries sensible energy J,
smoke tracer kg, derived K/kg/m³ fields and explicit budgets; existing momentum,
pressure, dye and buoyancy behavior is unchanged. Native/sanitizer and nine Python
controls plus a twice-reproduced native GrowthSim source experiment pass.
See [passive atmosphere runbook](passive_atmosphere.md). This is isolated offline
transport: no coupled checkpoint restart, open atmosphere, rising plume, ash,
MCP/install/release or renderer adoption claim.

## Consuming checkpoint and combined renderer boundary (2026-10-04)

The prescribed-flow passive receiving model now atomically checkpoints fields,
accepted input history, source consumption and receipts. Copy/reopen, partial
intervals, replay, stale revisions and failed publication are tested. It replays
pinned native history and does not serialize the separate transient CFD backend.
GrowthSim exports a contained runtime fire-surface scene with native VF3D attachment,
SI/model field sidecars and same-clock checkpoint/source binding. Existing RayTracing
preflight and a64x48 native render succeed without RayTracing source changes.
Open boundaries, buoyancy, airborne ash, fire-field shading and installed/live
adoption remain subsequent gates. See coupled_passive_checkpoints.md and GrowthSim
docs/contracts/combined_fire_atmosphere_scene_v1.md.

## 2026-10-04: evolving periodic atmosphere source lane

`periodic_evolving_passive3d_v1` now advances the existing unforced periodic
momentum solver and passive thermal energy/smoke together. Native checkpoint
continuation restores BDF/warm-start/scalar state and executes only new steps;
source consumption and that result publish atomically in a separately bound
SQLite journal. Fixed momentum dt, body-free periodic boundaries and no thermal
momentum feedback are explicit. Source-driven GrowthSim export carries evolved
velocity and solved pressure into the existing Fire-surface/VF3D scene contract.
See [evolving_atmosphere.md](evolving_atmosphere.md) for proof/limits. Open boundary
fluxes, buoyancy, GUI/MCP exposure and real-fire/visual acceptance remain open.

## 2026-10-04: open atmosphere and bounded buoyancy

The separately named xy_periodic_z_reservoir_projection3d_v1 model now implements
open bottom/top reservoir policies, explicit per-face energy/smoke inflow and
outflow, independent chunk/lifetime balances and native checkpoint continuation.
Atomic source consumption includes these flux counters. Opt-in small-contrast
Boussinesq feedback passes independent hydrostatic, thermal-response and parity
controls; oversized contrast rejects the joint candidate. Real Fire-source
transport demonstrates boundary outflow, and an explicitly authored synthetic
high-heat-capacity control demonstrates Fire-driven flow from zero velocity.
See [open_atmosphere.md](open_atmosphere.md) for meaning/proof/limits. This is a
first-order pressure projection with reservoir closures, not a full natural
traction or ground/real-fire-air model. Existing periodic lanes remain unchanged.
GUI/MCP/package/visual acceptance and broader convergence remain subsequent work.


## Open atmosphere bounded convergence qualification

Three additional independent controls qualify advected viscous shear spatial
refinement, source-driven thermal temporal refinement and pressure/thermal-force
superposition. Expected first-order error reduction and hydrostatic cancellation
pass. Worker/adapter bytes and accepted journals remain unchanged. This is targeted
analytic qualification, not general plume or joint 3D convergence. See
[open_atmosphere.md](open_atmosphere.md). Prioritize reusable coupling orchestration
next; ground/body policy needs a concrete surface consumer before implementation.

Grounded plume development, 2026-10-05: the explicit
`solid_bottom_open_top` / `no_slip_bottom_zero_gradient_top` policy adds an
impermeable stationary no-slip bottom to the existing all-fluid Cartesian plume
lane. Mixed pressure projection, zero ground heat/smoke receipts, hydrostatics,
restart and rejection are tested; see `docs/open_atmosphere.md` and
`tests/test_ground_atmosphere.py`. The low-temperature-contrast envelope and
periodic XY remain explicit limits. This is Main Edit source evidence, not an
installed/released hot-fire model.

### Local plume transport refinement candidate (2026-10-05)

See `docs/plume_transport_refinement.md` for the opt-in MUSCL transport, explicit
64-cube resource limits, pressure-operator reuse and unchanged numeric digest
contract. Grounded/open/coupled/native rollback gates and independent transport
controls qualify the local candidate. These source changes are in Main Edit;
this entry does not establish installation, adoption or physical-fire acceptance.

Full-duration local qualification, 2026-10-05:
`docs/plume_full_duration_qualification.md` records the opt-in eight-second,
3200-step sparse native runner and independently accepted binary64 samples.
Exact dense/sparse controls and full-duration 32³/64³ legacy-state parity pass.
The explicit work ceiling is one billion; ordinary resource defaults, 120-second
adapter timeout and numerical gates remain unchanged. GrowthSim owns the plume
time/grid/heat/diffusion comparison; these mechanics are Main Edit evidence.

Native full-burn movie mechanics, 2026-10-05 start: the explicit `--sparse-movie`
mode retains the existing solver with 32 s / 6400 steps / 160 binary64 sample
bounds and an explicit two-billion scalar-work ceiling. Packet/configuration
and sample bounds remain 64 MiB, numerical allocation 256 MiB for the 64³ case.
Only movie mode permits a bounded four-hour wall allowance; ordinary and
qualification limits remain unchanged. Compact movie fields pass the same
independent numeric gates without ordinary checkpoint serialization; binary
packet identity is distinct from canonical tagged-state identity. Six focused
movie parity/rejection tests pass, including exact dense/compact data and
separate wall-limit selection. GrowthSim's complete 32-second run passes its
gates and exact 2/4/8-second prior candidate parity. This is duration/media
qualification, not 32-second convergence or hot-fire/production acceptance.
See `docs/plume_full_duration_qualification.md`.

2026-10-06 explicit tall-domain qualification: `--sparse-domain-qualification` and Python `domain_qualification=True` admit up to 64×64×128 / 524,288 cells, retaining 8 s / 3200 steps / 3 samples / one-billion scalar work / 64 MiB packet bounds and an explicit numerical budget up to 512 MiB. Ordinary/movie admissions retain their previous caps. Optional physical-depth surface mapping preserves conserved horizontal/vertical overlap with explicit layer masks. See `docs/plume_domain_qualification.md`. Full-resolution 0.2-second source/ground/conservation and original-VF3D proof passes; no full-duration domain convergence or remote capability claim.

## Local numerical evidence lifecycle (2026-10-06)

The active box/cube/native-accuracy retained runners now use configurable
`data/experiments` storage outside normal cleanup roots; reference tools use
`data/tools`. Current regressions and exact archive checks are separated in the
first repaired slice. See [output and cleanup contract](cfd_evidence_lifecycle.md)
and [validated repair status and remaining gaps](cfd_lifecycle_repair_status.md).
Missing historical receipts do not block UI source work; fresh consumer and
relevant compatibility/visual checks remain required.
