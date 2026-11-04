# Opt-in live Fire–Fluid partition/window rig

Source development reference, 2026-11-01. The isolated Linux PC candidate passed
native qualification; both 12-second calm/gust variants and all eight comparison
phases completed successfully in 66 min 5.6 s. Diagnostics and decoded video are
fetched and hash-verified. At 12 s downstream/delivered smoke is 0% in calm and
29.49% in gust; source delivery differs by −0.09684%, so normalization is per case.
The diagnostic smoke is faint and sampled at 1 Hz; it does not qualify fine motion.
No canonical, installed app, fleet worker, VERSION or release change is implied.

## Runtime and geometry

`coupling.live_joint.LiveFramework` explicitly selects `physics_sim_live12/v1`:
20 ms Fire, 5 ms gas, 100 ms exchange, up to 120 accepted windows. Ordinary
Framework admission stays at 32 windows. The shared rational clock is unchanged.
Compact authenticated records cover every publication; selected full states plus
latest/prior heads retain lossless fields. Native state, operation receipt,
record and head publish in one SQLite transaction. Requesting a pruned full state
fails explicitly. Reopening never silently recomputes discarded fields.

`rig_cases` creates matched calm/gust inputs on a 16³ or 32³ unit chamber. Solid
shell walls and a 12.5 cm thick partition use native face blocking, pressure and
scalar exclusion. The partition's 25 × 50 cm window can be open or closed; west
and east 50 × 50 cm ports anchor each admitted pressure component. The ordinary wind profile acts only
at the west port/upstream strip. The static mask is independently checked against
physical boxes, component connectivity, aperture areas and blocked normal faces.
Sealed isolated components and moving window geometry are outside this profile.

Fire owns source evolution; the fluid worker owns gas transport. Diagnostics retain
actual delivered heat/smoke, closure, kinetic work, pressure/divergence, signed
port/window flux and downstream smoke normalized by delivered smoke. Equal
initial states and pre-gust prefixes support comparisons without assuming the
source delivers identical totals after feedback.

## Qualification and presentation

The fresh PC build and 21-phase qualification passed: legacy/default physical
parity, existing Framework tests, sparse history beyond 32 windows, exact reopen,
real process death before/after commit, controls at 16³/32³, short/long input
prefixes, native renderer and strict/sanitized numerical workers. A separate
ASan/UBSan/LSan fixture exercised four nonzero-source open/closed cases with zero
tracked native allocations remaining. Closed-window cold controls have exactly
zero aperture flux and downstream tracer. Open-window flux changes approximately
6.8% between these grids; this is a discrete control, not a convergence claim.

`rig_presentation` parses a fixed typed diagnostic-view edit bound to the parent
playback digest. The shared upstream camera/cutaway preserves field density,
physics masks and checkpoint bytes; omitted exterior walls remain numerically
closed. The pilot was rendered and visually reviewed. The pair uses 12 accepted
states per side at 1 Hz, held in a 25 fps 12-second comparison. Video does not
replace checkpoints or qualify motion between sampled states.

## Storage and operation

Large source/build/scratch/state outputs reside on the PC /home data filesystem.
Admission requires 8 GiB free, a conservative output projection below 4 GiB and
pilot-based runtime below two hours. Numerical processes have a separate 4 GiB
address-space limit and 512 MiB native allocation guard. Successful staging
retires after accepted publication and complete active-consumer checks; failed
unpublished attempts remain reproducible. No routine AI supervision is required.

Long jobs submit once and use sparse selected-job readback. A submitted job is
not a completed comparison. Stop at the first physical, identity, resource or
render failure; preserve accepted heads and the failure, without automatic retry.
General room-flow, combustion/radiation/ignition accuracy, finer-grid convergence,
30–120 second runs and installed UI/fleet adoption require further work.

## Studio visibility and viewing interpolation

`rig_studio` applies a strict parent-digest-bound dark studio edit to copied retained exports. Dark blue background/teal walls, downstream wall cutaway, cropped floor, aperture-visible camera and key/fill lighting improve separation. The display uses8×optical extinction (60000m²/kg), scattering albedo0.98 and isotropic scattering; this is authored visibility, not measured soot optics or increased simulated smoke mass. Physical density/source/journal bytes stay unchanged. Original1Hzaccepted images and a separately labeled25fpsoptical-flow viewing clip are retained. Optical-flow intermediate images are presentation estimates; future resolved motion requires denser native state retention. No numerical rerun, solver/API/default changes or latest-good promotion.

## Explicit native10Hzretention

`DenseLiveFramework` is a separately selected storage profile. It retains all121
full accepted checkpoints for120windows, including genesis, using the same
lossless codec,512MiBjournal limit and unchanged numerical live12path. The ordinary
sparse profile remains unchanged. The dense source is pinned in the binding.
Actual35window/all36state and32³0.8s/all9state/restart tests match the earlier
physical/Fire histories exactly. A bounded read-only rendering view validates
the complete journal/retained payload hashes once per four-frame batch, then
decodes/native-validates each selected payload and genesis. At most six selected
checkpoints are resident; no full-history materialization. Four-frame native PNG
hashes match the original exporter exactly.

The fresh12spair saves every0.1sstate and renders120native images per side into
a10fps12svideo with no interpolation. The same dark studio style is used. Full
execution and delivery completed in2h12min4.7s. All121full states percase decode/reopen and match prior physical/Fire traces exactly. Both120frame native sequences are unique and locally decoded10fpsvideo passes. Dense science/images remain PC-backed with zero native staging. This is not installed/fleet adoption.

## Optional broad spatial gust — source development2026-11-02

An explicit `rig_geometry.flow_profile: "broad_sweeping_shear/v1"` selects
distributed X momentum forcing over the upstream chamber(x<=0.4375m, interior
y/z), excluding blocked faces. A smooth positive0.3..1spatial envelope shifts
laterally/vertically with3s/4speriods, multiplying the existing signed target and
height ramp before relaxation. Strength zero still releases exactly; target zero
with positive strength brakes the driven region. Work remains the signed discrete
MAC kinetic difference, accepted with ordinary projection/scalar/pressure checks.
The option changes forcing extent and shear, not geometry; the same window still
constricts the flow. This is distributed body forcing, not a boundary gust or
turbulence/calibrated wind model. Missing option preserves the ordinary port
profile; unknown option values are refused. Source/config/worker hashes bind it.

Strict local and PC strict/sanitizer force/integration controls passed. New12s
wave/reversal comparison completed through an isolated frozen-parent candidate:
all18stages passed in2h36m13s,121full states/120native frames percase, actual
aperture reversal at5.3s after5.0scommand. The portrait10fpsvideo is fetched,
hash-verified and fully decoded locally. This establishes selected discrete
control/accounting, not general physical accuracy. The user
requested no extra controlpanel: one direct wave/top/reversal/bottom portrait
comparison, native10fps/0.1ssavedstates, no image interpolation. The temporal wave
is the existing16knot positive sine-like approximation, not an analytic sine.
