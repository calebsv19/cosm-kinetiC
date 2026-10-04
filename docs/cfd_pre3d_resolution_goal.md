# Completed 2D resolution and efficiency implementation

The original five-step goal is complete in Main Edit source. The final
[completion audit](cfd_pre3d_completion.md) supersedes chronological outstanding
items below and records the exact scope and limits. No commit, Desktop refresh,
3D implementation or shared API/version change is included. Existing
scene/session/agent interfaces are retained.

| Requirement | Current evidence | Status |
|---|---|---|
| 1. Localize error and establish fair cost baseline | Reference-field reconstruction, directional tests, surface/corner refinement, serial optimized local/uniform cost screen, CPU phase profile | Error mechanism and measured cost established for fixed rectangle; no matched passing uniform pair |
| 2. Scale solver efficiently | Cached pressure/velocity multilevel preconditioning, optimized worker, bounded allocations including setup, same-equation speedup | Implemented and tested; iteration growth remains disclosed |
| 3. Fixed conservative local refinement | Balanced sparse multi-level mesh, shared fluxes, transfer/operator tests, evolving channel, crossing transport, body force/energy and outlet checks | Implemented; physical qualification limited to stated reference cases |
| 4. Remove explicit viscosity timestep restriction | Implicit viscosity, continuous manufactured transient, spatial/time refinement, CFL/CPU study, agent observed timestep bound | Implemented and verified for synchronized constant steps; no dynamic subcycling claim |
| 5. Close physical and efficiency gates with agent access | Two passing body resolutions, separate components/energy, refinement sensitivity, real agent control/sample/comparison/artifacts, numerical memory admission | Agent integration and matched-accuracy transient time/RSS verified; body qualification separate |

## Error localization

`scripts/cfd_fem_reference.py --sample-root ...` samples the independently
corner-refined P2/P1 reference onto native staggered face and pressure locations.
The native reconstruction probe evaluates the existing pressure/cubic shear
estimator without evolving those fields. Results:

| Grid | Pressure reconstruction error on reference flow | Viscous reconstruction error on reference flow |
|---|---:|---:|
| 16² | 13.05% | 6.97% |
| 32² | 8.23% | 5.88% |
| 64² | 5.44% | 4.46% |
| 128² | 3.70% | 3.26% |

Thus higher flow-solve accuracy alone cannot close component accuracy at these
sample spacings. Surface reconstruction/local boundary resolution is a measured
part of the error. These differences cannot simply be subtracted from the full
solver errors as an independent additive uncertainty budget.

Native directional refinements preserve physical domain and body. Increasing
X cells from 64 to 128 with Y fixed at 64 gives pressure .003433495412 N and
viscous .002528757803 N; increasing only Y to 128 gives pressure .003329869783 N
and viscous .002530453764 N. Both still fail separate 2% gates. The strong X
sensitivity matters because the base 4x2 m / 64x64 grid has dx=2dy.
Uniformly finer sampling and X-only refinement are diagnostics, not substitutes
for the requested conservative local-refinement implementation.

## Pressure and observation implementation

The unchanged fine-grid pressure matrix is preconditioned with a cached hierarchy.
Aggregation uses injection P, restriction P^T, and exact Galerkin coarse matrices.
Paired forward/backward Gauss-Seidel sweeps keep the V-cycle symmetric; the
bottom solve uses a fixed symmetric sweep count. True pressure residual and
post-projection divergence checks remain unchanged. Odd rectangular grids,
linearity, symmetry and positive quadratic forms are tested. Coarse topology
only accelerates the fine solve; it does not redefine physical obstacle geometry.

A matched 16/32/64/128 benchmark runs identical evolving fields against the
previous unpreconditioned solver at -O2. The one-sweep cycle reduced 128² pressure
iterations from 45,432 to 3,363 over 80 steps. Recorded runtime gain is about 2x
at 128² and 1.6x at 64²; the smallest grid is approximately break-even.
See the saved final benchmark for exact host-dependent timings. Velocity
agreement is within 1e-11 m/s. Hierarchy storage adds memory and is disclosed;
iteration reduction alone is not a claim of proportional wall-time speedup.

Open force observation reads native velocity strides directly, without allocating
and copying a whole field. Body bounds are retained from immutable geometry.
The worker reuses the preceding step's completed energy and momentum inventory.
Every-step velocity range monitoring remains active but uses dynamically sized
storage rather than fixed 64² arrays. Pressure/predictor milliseconds and pressure
workspace bytes are exposed in health. Diagnostic/serialization phase timing
can be refined further without weakening the every-step acceptance measurements.

`make physics-sim-session-worker-optimized` builds a separate -O2 worker under
`build/cfd-optimized/`, avoiding mixed debug/optimized object caches. Debug and
optimized real agent runs agree on forces, energy and flux; result hashes verify.
No installed application is updated by this target.

## Evidence and reproduction

Evidence root: `build/s4-resolution/`. The directory name does not rename the
original S3/S4 product roadmap; this is the current pre-3D numerical goal.

- `make test-cfd-pressure-mg`: algebraic properties, including odd grids.
- `make test-cfd-pressure-scaling`: matched old/new solver fields and cost.
- `make test-cfd-run-acceptance`: monitor and live open-agent checks.
- Existing periodic lab, MAC, reduced-channel and general session suites pass.
- Outlet, masked transient energy and warm/cold pressure checks pass.
- Address/undefined sanitizers pass the hierarchy and dynamic monitor fixtures.
- `tests/cfd_reference_reconstruction_probe.c` evaluates sampled reference fields.
- Directional harness: compile with `CFD_OPEN2D_GRID_LIMIT=128`, run
  `cfd_open2d_reference_test 64 .002 4 1 128` and
  `cfd_open2d_reference_test 128 .002 4 1 64`.

## Historical next implementation boundary (superseded by checkpoints below)

The fixed mesh/operator and initial evolution checkpoints are recorded below.
Proceed to coupled nonuniform/energy verification and then the refined body.
Retain uniform flow as the regression oracle. Do not claim AMR from
sparse storage alone, do not replace the goal with stretched-grid-only work,
and do not accept total-drag cancellation as component qualification.

A bounded AMReX-Hydro/MLMG compatibility check is still appropriate before
building substantial custom coarse/fine infrastructure. Neither AMReX headers
nor an existing vendored adoption were found in the checked local locations.
The first multigrid acceleration above remains app-owned (`reuse-deferred` for
a general shared solver); it reuses existing application scene/session/data
interfaces, with no changes to shared core/kit APIs or versions.

## Fixed local-refinement operator checkpoint

The app-owned `cfd_refined_mesh` now constructs a fixed 2:1 rectangular patch.
Covered coarse cells are replaced by four leaves. Internal face segments are
shared once, so a coarse interface flux is the sum of its fine subface fluxes.
Volume-weighted restriction and physical-gradient prolongation preserve affine
cell averages. Topology tests include odd grids, full refinement and a patch
meeting the domain boundary. Arbitrary prescribed internal fluxes cancel to
roundoff, and an analytic linear vector field has the expected divergence in
all cells, including coarse/fine interface cells.

`cfd_refined_diffusion` is an additive operator prototype using cell and face
pressure unknowns. It follows the isotropic hybrid finite-volume energy of
[Eymard, Gallouet and Herbin](https://arxiv.org/abs/0801.1430). Shared face
unknowns enforce continuity of outward flux; stabilization vanishes for affine
pressure. This avoids the inconsistent two-point normal difference between
coarse/fine centers that are displaced tangentially. Exterior faces can use
Dirichlet pressure or prescribed outward diffusive flux, with at least one
pressure anchor. All-Neumann/gauge handling is not implemented.

Tests establish:

- Linear pressure and normal flux reproduced to solver tolerance with both
  full Dirichlet and mixed pressure/flux boundaries.
- Smooth independent manufactured pressure L2 error on base 8/16/32 grids:
  .00243253004, .00060163055, .000148997831; roughly 4x reduction each refinement.
- Shared flux continuity and per-cell source/flux balance pass independently.
- Projection of a channel plus an analytic gradient disturbance, including
  pressure-outlet and zero correction-flux elsewhere: maximum divergence
  reduces from about 1.1 to below 3.1e-10. Error relative to the known channel
  velocity decreases .00211730 -> .000544567 -> .000149570.
- Topology, diffusion and projection AddressSanitizer/UBSan tests pass.

Reproduce with `make test-cfd-refined-mesh test-cfd-refined-diffusion
 test-cfd-refined-projection`. Evidence is under
`build/s4-resolution/local-refinement/`.

At this operator checkpoint there was no evolving solver. The subsequent
evolution checkpoint below adds transport, implicit diffusion and a channel
prototype; obstacle, agent and performance qualification remain outstanding. The prototype caches local hybrid matrices and
uses diagonal-preconditioned CG; those extra unknowns/storage must be included
in the later cost comparison. Fewer leaf cells alone is not a speedup claim.
The next boundary is conservative velocity evolution on this mesh and its
channel/interface transient verification before introducing the body.

Reuse decision remains `reuse-deferred` for these numerical operators: shared
`core_math`/`core_space` and runtime/data interfaces do not supply this specific
coarse/fine CFD discretization. Existing scene/session control remains the
integration boundary; no shared API or version change is introduced.

## Refined evolution checkpoint

The hybrid operator now supports `mass*u - diffusivity*laplacian(u) = source`,
retaining conservative face fluxes and the prior zero-mass pressure API.
A backward-Euler first step followed by BDF2 is tested against a decaying sine.
The analytically known uniform-grid discrete eigenvalue isolates temporal error:
10/20/40 steps yield errors .000830233/.000197805/.0000485103 (ratios 4.20, 4.08).
Independent continuum spatial errors on locally refined base 8/16/32 meshes are
.00103401/.000250591/.0000645428. Cell balances pass and the scalar norm decays.
A large-step case remains stable at nu*dt/h_min^2 = 20.48; that demonstrates
removal of the explicit diffusion stability restriction, not equal accuracy
at arbitrary large timesteps.

`cfd_refined_transport` caches least-squares geometry and workspace, applies
limited linear upwind reconstruction, and transfers each face's transported
quantity exactly once. An SSPRK2 verification moves a Gaussian from x=.6 to
x=3.3 through both boundaries of the x=[1,3] fine patch. Exact Gaussian cell
averages are the reference. The spatial L2 errors are .139819/.0604952/.022011
on base 16x8/32x16/64x32 meshes; conservation error is below 1e-14 and no negative
values or overshoot appear in this case. Uniform-fine controls give lower
errors .102782/.0438203/.0126171. This loss in accuracy outside the patch must
be accounted for in a matched-accuracy comparison. The transported Gaussian is
a component/scalar test, not a coupled vortex or turbulent wake qualification.

`cfd_refined_channel` connects these operators with constant-step BDF2,
extrapolated explicit transport, implicit viscosity, and incremental pressure
correction. The channel has parabolic inlet, stationary no-slip walls, natural
diffusive outlet and zero outlet pressure. Unsafe transport Courant numbers
above .25 are rejected; numerical failure after prediction makes the state
terminal. Mesh/operator/work buffers persist across steps. Face component
traces are predictor values; only the normal face field is projected. They
are explicitly not final vector/traction observations.

At t=.5 s on base 16x8/32x16/64x32 locally refined meshes, velocity relative L2
errors against Poiseuille flow are .8071%/.2293%/.05889%; pressure relative L2
errors are 1.618%/.3971%/.09976%. Maximum divergence stays below 5e-13 and
inlet/outlet mismatch below 4e-15 in these runs. Halving dt on the middle mesh
leaves velocity error essentially unchanged, consistent with spatial error
being dominant here. A t=5 s middle-grid continuation remains bounded with
.2919% velocity and .2960% pressure error. This is not a steady-window or
independent energy/traction certificate.

Reproduction adds `make test-cfd-refined-transient test-cfd-refined-transport
 test-cfd-refined-channel`; logs live beside the operator evidence. The source
prototype is additive and has not been exposed through the worker/agent model
selector. Next: coupled nonuniform flow and energy verification, then a body
fully inside the fine patch with separate force references and matched-cost
comparison. Implicit diffusion tests do not certify the full coupled flow's
temporal order, which must still be measured.

The channel comparison also guards against an incorrect efficiency inference:
base 64x32 with a central patch uses 3584 leaf cells and has .05889% velocity
error at .5 s; uniformly fine base 32x16 uses 2048 leaves and has .05180% error.
For this smooth wall-driven field the chosen central patch is not an efficient
accuracy allocation. It is a consistency test for the later body-centered patch,
not evidence that local refinement always beats a uniform grid.

The implicit transient, transported Gaussian and evolving-channel checks also
pass AddressSanitizer and undefined-behavior sanitizer, including the 5 s run.

## Coupled nonuniform qualification and correction decision

A continuous manufactured streamfunction perturbation of the channel now
exercises nonuniform velocity, physical body acceleration, transport, viscosity
and pressure together. The native forced-step API accepts acceleration in m/s^2;
no spatial stencil is used to manufacture the exact forcing. The perturbation
vanishes at the walls/inlet/outlet and has zero normal derivative at the natural
velocity outlet. Spatial velocity L2 errors at base 16x8/32x16/64x32 are
.000207781/.0000556807/.0000141668 (ratios 3.73, 3.93).

The split refined prototype does **not** pass coupled temporal qualification.
At fixed base 32x16, successive timestep-difference ratios are 2.42, 2.30, 1.69.
The uniform-grid control gives 4.06, 3.97, 3.47. Two bounded controls localize the
problem: analytic divergence-free initial face integrals do not materially alter
the deficit, and removing the transport limiter also leaves it unchanged.
Two pressure corrections improve the ratios only modestly. Eight improve the
finest ratio to 3.95 but not all coarser ratios; this is not a qualified solution.
A 64-correction residual-controlled probe fails the coupled predictor-divergence
criterion on its first step (scaled residual .00251954 versus target 1e-6).
The projected final divergence alone is therefore insufficient to establish
consistent momentum/pressure coupling. The default remains the previously
measured one-correction **verification prototype**, not an agent runtime.

The next correction is now evidence-driven: use a compatible mixed momentum/
continuity system, rather than adding arbitrary repeated split solves. The
native hybrid matrix is exported through `cfd_refined_diffusion_matrix_visit`.
`scripts/verify_cfd_refined_coupling.py` assembles the same velocity diffusion,
cell mass and shared-face divergence into one saddle system, with the pressure
term the negative transpose of that divergence. The existing isolated SciPy
reference environment solves it directly for a bounded verification oracle.
This is not a new worker dependency, runtime backend, independent mesh reference,
or efficiency claim.

That mixed-system oracle passes steady-channel spatial convergence and unsteady
Stokes timestep convergence. Steady velocity L2 errors decrease
.000868744 -> .000230182 -> .0000584983; pressure errors decrease
.000739313 -> .000198453 -> .0000514406. On the middle grid, halving dt from .04
to .0025 produces time-difference ratios **4.074, 4.033, 4.016**, while divergence
remains below 7e-14. Thus the existing mesh and diffusion matrices can be retained,
but the native refined momentum/pressure coupling needs replacement before its
nonuniform, energy, obstacle and agent qualification proceeds.

Reproduction:

- `make probe-cfd-refined-flow-transient` prints diagnostic results and an explicit
  `coupled_temporal_order_qualified` boolean. Its process success means the probe
  ran, not that the time-order gate passed.
- `make test-cfd-refined-mixed-coupling` exports native matrices and runs the
  mixed-system spatial/time acceptance assertions. It uses
  `CFD_REFINED_REFERENCE_PYTHON` (default `build/cfd-reference-venv/bin/python`).
- Evidence: `coupled-*-initial.log`, `coupled-two-corrections.log`,
  `coupled-eight-corrections.log`, `coupled-initial-flux.log`,
  `coupled-unlimited.log`, and `mixed-coupling.json` in the existing evidence root.

This changes the immediate next implementation from adding an obstacle to
correcting the native coupling against the passing mixed-system oracle. It does
not reduce the five-step goal or remove the remaining force/energy/cost gates.

## Native compatible mixed implementation

The refined channel now defaults to `cfd_refined_mixed`: the native C11 system
assembles velocity diffusion and mass together with shared-face continuity,
using the negative transpose of that same divergence for pressure forces.
The reference Python environment is not a runtime dependency. The rejected split
path is retained only under `CFD_REFINED_VERIFY_SPLIT` for regression controls.

A sorted sparse row matrix, ILU(0) factorization with an enriched pressure graph,
and restarted/reorthogonalized GMRES are cached in the operator. Zero entries
on the pressure-neighbor graph permit preconditioner fill without changing the
physical matrix. Changing the mass coefficient rebuilds the factor; repeated
BDF2 steps reuse it. A true unpreconditioned residual gates completion. No
numerical pivot replacement changes the physical system. Fields are copied to
caller outputs only after a successful solve. The solver preserves its converged
state as the next initial guess; an identical repeat requires zero iterations.

Native steady channel fields match the direct mixed-system oracle with maximum
u/v/p differences 3.5e-15, 1.95e-12 and 2.80e-11 at base 16x8/32x16/64x32.
The native nonuniform manufactured Navier-Stokes run (transport active) now has
timestep-difference ratios **4.074, 4.033, 4.016**. The time-order test has actual
acceptance assertions in native builds; the old split build still reports its
failed qualification separately. The matching Stokes oracle is an algebra
comparison, not an independent obstacle-force reference.

The native outlet is **zero external vector-Laplacian traction**:
`nu*du/dn - p*n = 0`. It is not a separately imposed zero pressure plus zero
velocity derivative. The tested exact channel and manufactured perturbation
satisfy this condition. This matches the reference Stokes outlet family; outlet
sensitivity still needs checking for the eventual body problem. Pressure is
stored in Pa in the channel wrapper and Pa/rho inside the mixed operator.
Both final face velocity components participate in the mixed solve. Face
pressure is deliberately unavailable (NaN) until a trace/reconstruction is
qualified; the code does not fabricate wall pressure from a midpoint average.

The coupling correction has a visible spatial tradeoff. At t=.5 s the coarse
base 16x8 mixed channel has 3.715% velocity and 5.980% pressure error, failing the
old split model's 3% pressure check. That regression is retained in
`channel-mixed-regression.log`; it is not called a pass. The old split bound
remains in `test-cfd-refined-split-channel`. The native channel test reports a
separate one-percent physical gate per run, and requires it at the finest
verification grid. Base 64x32 passes with .2756% velocity and .2786% pressure
error. Middle-grid runs remain unqualified at one percent. A stable solution
and a converged linear solve do not imply a requested physical accuracy.

Efficiency remains open. The initial steady native mixed solves require
180/540/2760 iterations and about 1.13/4.41/17.42 MB of persistent operator
storage, excluding mesh, channel arrays and setup peak memory. The 50-step
finest channel uses 7260 combined iterations in total with factor/state reuse.
This is a correct bounded implementation, not yet an optimal scalable solver
or a matched-accuracy speedup claim. Pressure preconditioning needs improvement
if it dominates the body/quality benchmarks.

New checks: `test-cfd-refined-mixed`, `test-cfd-refined-flow-transient`,
`test-cfd-refined-channel`, and `test-cfd-refined-channel-units`. The mixed
operator and physical-units/rejected-input fixtures pass address/undefined
sanitizers. Holding kinematic viscosity fixed while doubling density/dynamic
viscosity preserves velocity and doubles physical pressure. Invalid acceleration,
unsafe initial transport timestep and later timestep changes are rejected
without advancing physical state.

This replaces the immediate coupling blocker. Next are independent energy and
refined body/force qualification, stronger pressure preconditioning and a real
matched-accuracy cost comparison, followed by agent/session integration. The
five-step goal remains open.

## Refined obstacle and physical-energy checkpoint

The fixed mesh now removes one grid-aligned rectangle composed entirely of
fine leaves before operator creation. Fluid volume, body perimeter, closed
normal sum and the geometric divergence theorem are checked. Partial-cell or
non-fine removal is rejected without mutating the mesh. Body faces have an
explicit solid tag; both velocity components are prescribed zero there during
transport, implicit momentum solution and initialization. Mesh/operator IDs
remain immutable after construction.

`cfd_refined_mixed_boundary_force` exposes separate pressure and viscous weak
boundary reactions, scaled by density and span. The pressure contribution uses
the mixed cell pressure; it is not a higher-order wall-pressure trace. The
viscous contribution is the boundary reaction of the velocity diffusion matrix.
No force is exposed before a successful solve. The whole-domain reaction closes
the steady momentum balance, and symmetric obstacle lift is negligible.

Against the existing corner-refined independent P2/P1 reference, the confined
rectangle (body [1.5,2.5]x[.75,1.25], domain 4x2, span .5, rho 1, mu .1, inlet
mean .002) gives:

| Base mesh | Fluid leaves | Pressure error | Viscous error | Total error |
|---|---:|---:|---:|---:|
| 16x8 | 192 | 41.39% | 12.19% | 29.93% |
| 32x16 | 768 | 21.95% | 5.51% | 11.17% |
| 64x32 | 3072 | 12.19% | 9.16% | 3.81% |

Thus the separate 2% force gate still fails. A diagnostic quadratic pressure/
cubic no-slip shear reconstruction changes the middle-fine result to 9.41%
pressure error and 5.45% viscous error; it does not qualify the components.
The coarsest patch lacks three fine normal samples at every wall and reports
that reconstruction unavailable instead of silently using partial stencils.
These are steady Stokes comparisons, not arbitrary-Reynolds drag certificates.

An evolving-body fixture runs .1 s of the native transport/implicit momentum
path. It checks no-slip on every solid face and closes the full-domain force
balance including backward-Euler/BDF2 fluid momentum and the explicitly
extrapolated transport contribution. Maximum momentum residual is 7.56e-14 N;
maximum divergence is 4.15e-14 s^-1. This is a conservation/implementation proof,
not physical force accuracy or a steady-state certificate.

`cfd_refined_mixed_energy` separately computes physical symmetric-strain
quadrature, discrete matrix diffusion energy, stabilization contribution,
weak boundary work, symmetric-stress boundary correction, body-force work,
kinetic energy and outward kinetic-energy flux. It does not manufacture an
energy derivative or declare acceptance. Exact Couette flow calibrates the
physical strain and boundary power to .1632 W; stabilization is roundoff, and
observation scratch does not alter the next solve. Kinetic energy retains the
explicit midpoint-volume quadrature error rather than claiming exact integration.

On the base 64x32 obstacle, physical steady-Stokes strain-energy imbalance is
**2.852%**, with about 2.988% stabilization contribution. Its algebraic energy
balance is near roundoff. The physical 2% gate therefore remains failed despite
excellent linear residual and algebraic conservation. Boundary work derives
from the weak reaction plus a separately evaluated stress correction; this is
not an independent high-order wall-traction reconstruction.

The next base 128x64 obstacle solve **fails** the linear gate after 6000 native
iterations (relative residual .0310), so no force/energy result is accepted at
that resolution. Bounded generated-code probes for full local fill structure,
one additional symbolic ILU fill level, three recycled Krylov directions, and
a simple velocity/pressure block preconditioner do not resolve that failure.
Their fine residuals remain approximately .0173, .0213, .0218 and .0152; none
is adopted. The physically unchanged medium-grid results agree between the
variants. Raising the iteration cap is not the next step.

The next implementation is a proper multilevel velocity-block preconditioner
for the compatible saddle system, with a pressure Schur approximation that
retains global coupling. Once the fine solve is reliable, continue the existing
component-force and physical-energy refinement gates; if sharp-corner errors
remain dominant, concentrate further resolution there instead of refining the
entire far field. This is still within the original resolution/efficiency goal.

Reproduction: `make test-cfd-refined-energy test-cfd-refined-obstacle-evolution
 probe-cfd-refined-obstacle`. The probe prints physical qualification separately
from process success. Evidence is under `build/s4-resolution/refined-obstacle/`.
The body evolution, body geometry/force fixture and energy calibration have
address/undefined-sanitizer checks. No agent selector, package or 3D change is
introduced by this checkpoint.

## Multilevel coupled-solver scaling correction

The preceding fine-grid linear failure is resolved by the default refined
mixed backend. `cfd_sparse_mg` supplies cached symmetric Galerkin V-cycles on
sorted sparse velocity matrices: geometric aggregation, injection P, restriction
P^T, two forward/backward Gauss-Seidel smoothing sweeps, and a cached Cholesky
solve on the smallest level. Apply operations allocate no memory. Hierarchies
copy their input matrices and validate their sparse indices and finite data.

The coupled preconditioner is block triangular. Velocity multilevel solves
supply the coupled pressure residual; the approximate inverse pressure Schur
complement combines `nu M_p^{-1}` with `mass L_p^{-1}`. The pressure Laplacian
is used only for acceleration, with no change to the physical matrix, natural
outlet, accepted true residual or boundary reactions. Its hierarchy persists
across timestep mass changes; the velocity hierarchy is rebuilt for a changed
mass coefficient and reused thereafter. The old ILU implementation remains
only under `CFD_REFINED_VERIFY_ILU` for comparison. Its memory is not allocated
in the default backend. Shared reuse remains deferred for this app-specific
numerical operator; there is no shared-library API/version change.

A steady-only pressure approximation resolved the obstacle but was costly in
transients. The mass-dependent correction retains the native nonuniform time
ratios 4.074, 4.033, 4.016, reducing the exploratory complete fixture runtime
from about 139 s to 20 s. Those exploratory timings were not an isolated matched
benchmark. The promoted default also passes the channel spatial/one-percent
finest-grid gate, physical units/rejected-input checks, exact Couette energy,
and evolving-body conservation. Native steady fields agree with the direct
mixed-system oracle to at most 4.0e-13 across the three tested meshes. Sparse
MG symmetry, linearity, positive quadratic form and residual-reduction checks
pass, including odd rectangular grids. Sparse MG and channel-unit paths have
address/undefined sanitizer checks.

The reproducible **serial** matched -O2 obstacle benchmark gives:

| Base grid | Fluid leaves | ILU iterations | Multilevel iterations | Multilevel wall time | Process peak RSS |
|---|---:|---:|---:|---:|---:|
| 64x32 | 3072 | 1320 | 240 | .518 s median | about 22.6 MB |
| 128x64 | 12288 | failed previously | 420 | 3.82 s | 104.4 MB |
| 256x128 | 49152 | not rerun | 660 | 23.33 s | 371.9 MB |

Three alternating medium-grid samples give a 4.27x median speedup and force
agreement within 1e-9 N. Timings include mesh/setup/solve/observations and process
startup; RSS includes setup peak rather than only persistent operator storage.
These are host-specific same-discrete-problem comparisons, **not** a matched
physical-accuracy advantage for local over uniform refinement. Iteration growth
still exists; this is an effective bounded correction, not mesh-independent
complexity proof.

The newly accepted finer obstacle results further localize the physical gap:

| Base grid | Weak pressure error | Weak viscous error | Total error | Physical energy imbalance |
|---|---:|---:|---:|---:|
| 128x64 | 7.422% | 8.029% | 1.357% | .892% |
| 256x128 | 4.790% | 6.083% | .522% | .300% |

The diagnostic wall reconstruction gives pressure/viscous errors 5.430%/4.628%
and 3.396%/3.282%, respectively. Thus separate component forces still fail 2%
even where total force and steady physical energy are below 2%. No result is
promoted based on cancellation. The next spatial work is balanced fixed local
refinement concentrated at surfaces/corners, with matched uniform controls and
outlet sensitivity kept separate; further whole-patch doubling is not the
preferred resolution policy. Refined agent/session integration and the full
physical/efficiency acceptance remain outstanding; 3D has not started.

Reproduction: `make test-cfd-sparse-mg test-cfd-refined-fine-linear`;
`make test-cfd-refined-flow-transient test-cfd-refined-mixed-coupling`;
`make benchmark-cfd-refined-preconditioner`. The fine-linear target requires
linear success only and retains printed failed physical qualification. The
benchmark records unrounded observations, elapsed time, and measured peak RSS
under `build/s4-resolution/refined-obstacle/multilevel-cost/`. Other regression
logs are `multilevel-regression.log`, `mg-oracle.log`, and
`mg-final-transient.log` in their corresponding evidence directories.

## Sparse localized surface/corner refinement and physical acceptance

`cfd_refined_mesh_init_regions` now builds a fixed hierarchy of overlapping
physical refinement regions with up to ten dyadic levels, a caller-supplied
leaf budget, and enforced 2:1 face-neighbor balance. Sorted start/end events
on leaf sides construct shared subfaces without a dense full-domain ownership
array at the smallest spacing. The constructor fails cleanly if requested or
balancing splits exceed the budget. Integer geometry carries an explicit
lattice scale; restriction uses actual leaf volume. The original two-level
constructor remains supported and its corresponding region mesh has identical
cells/faces. Whole refined leaves can be removed for the existing rectangle
boundary without requiring all removed leaves to have the same depth.

Geometry tests cover budget rejection, odd/non-aligned regions, deep level-10
local patches, balanced neighbor ratios, at most eight faces per leaf, geometric
divergence, affine transfer, shared-flux cancellation, and affine hybrid
operator accuracy. The deep localized test has only 248 leaves instead of a
finest-domain raster. Sparse topology and localized evolving-body paths pass
address/undefined sanitizers. Existing two-level mesh/diffusion/transport/units
regressions remain passing. The coupled manufactured transient on a three-level
mesh retains timestep ratios 4.0742, 4.0334, 4.0158. A four-level evolving-body
case closes momentum to 2.58e-12 N with maximum divergence 6.61e-11 s^-1.

For the fixed reference rectangle, the tested policy has the existing broad
level-1 patch, level-3 wall strips, and geometrically shrinking corner regions
through level 7. The finest spacing is confined to the corners. This is fixed
resolution before initialization, not time-dependent adaptive remeshing.
The multilevel preconditioner starts aggregation at the actual smallest mesh
spacing, so deeper local levels do not collapse into one overly coarse initial
aggregate.

| Base grid | Fluid leaves | Pressure force error | Viscous force error | Total force error | Physical energy imbalance |
|---|---:|---:|---:|---:|---:|
| 64x32 | 7296 | 3.739% | .209% | 2.353% | 2.306% |
| 128x64 | 18240 | 1.755% | .970% | .685% | .657% |
| 256x128 | 58560 | .979% | .982% | .209% | .194% |

The last two runs pass the unchanged separate 2% pressure/viscous, total-force
and physical steady-energy gates. Their pressure, viscous and total forces
change by .784%, .012%, and .477%, respectively; each change decreases from the
preceding refinement pair. Viscous reference error is not monotonic, so these
results are not a claimed Richardson error estimate. The coarse run remains
explicitly unqualified; a small viscous error alone is not sufficient.

These force results use the original weak boundary reactions, not a fitted
reference correction or the diagnostic wall reconstruction. Reconstruction
errors are also reported separately. The level-6 middle run narrowly fails
pressure at 2.019%; the level-7 result is a real additional refinement, not a
relaxed threshold. No solver tolerance or reference value was changed.

`test-cfd-refined-local-obstacle` asserts the component/total/energy gates and
true linear residual for the middle mesh. The digest-bound
`local-qualification.json` records all three runs and the last-pair <1% force
change gate; it explicitly leaves general CFD, outlet sensitivity and matched
physical-accuracy cost unqualified. This is a fixed low-Re steady confined
Stokes rectangle certificate, not a moving-body, arbitrary-shape or turbulent
wind-tunnel certificate.

Localized runs improve the cost/accuracy tradeoff: the 18240-leaf qualifying
run uses fewer cells than the earlier 49152-leaf broad-patch run that still
failed component accuracy. This is measured accuracy dominance, not yet a
matched-accuracy timing certificate. Exploratory timings can overlap other
verification work and must not be substituted for a serial matched benchmark.
The separate outlet-sensitivity, matched-accuracy cost, memory-admission and
agent/session integration boundaries remain open. No 3D work has started.

Reproduction: `make test-cfd-refined-mesh-local
 test-cfd-refined-local-obstacle test-cfd-refined-local-obstacle-evolution
 test-cfd-refined-local-transient`. Run the obstacle fixture at 32/64/128 with
arguments `0 7` after the grid argument, then use
`scripts/verify_cfd_refined_local_accuracy.py --logs <three logs> --output <json>`.
Evidence resides in `build/s4-resolution/local-refinement/local-*` and
`build/s4-resolution/refined-obstacle/local-*`.

## Refined outlet-distance qualification

The fixed rectangle probe now varies downstream domain length independently
of cell spacing: L=4, 6 and 8 m at base dy=dx=2/64 m, with unchanged body,
inlet mean, wall/corner refinement and natural vector-Laplacian traction outlet.
The refined-background control also compares L=4 and 6 at dx=dy=2/128 m.
Extended-domain runs do not apply the original L=4 reference-force gate;
reference errors are unavailable and certification remains false for that
changed problem until it has its own reference. The sensitivity verifier uses
raw component forces, never those reference-error fields.

| Background | Outlet extension | Pressure-force change | Viscous-force change | Total-force change |
|---|---|---:|---:|---:|
| n=64 | 4 to 6 m | .002984% | .0000731% | .001764% |
| n=64 | 6 to 8 m | 5.00e-8% | 5.97e-8% | 5.39e-8% |
| n=128 | 4 to 6 m | .002904% | .0001086% | .001708% |

All changes pass a separate 1% boundary-sensitivity limit, retain the linear,
divergence and whole-domain momentum checks, and the second extension changes
forces less than the first. The fine-background comparison confirms that the
observed small outlet effect is not peculiar to the medium mesh. Outlet
distance is therefore not the dominant force uncertainty in this fixed steady
Stokes configuration. This does not certify transient outflow, backflow, wake
shedding, or other inlet/outlet boundary formulations.

Reproduce with `make test-cfd-refined-outlet-sensitivity`. The target runs five
bounded problems and writes `outlet-qualification.json` through
`scripts/verify_cfd_refined_outlet.py`, including input-log hashes, independent
component comparisons and explicit scope. Existing baseline logs from before
the length option remain supported only as the known fixed L=4 fixture.
The current probe reports NaN reference errors for changed lengths; the first
exploratory extension logs retained numeric comparisons but explicitly marked
the reference inapplicable, and those comparisons are not qualification inputs.

Remaining goal work: serial matched-accuracy cost, bounded memory admission
including setup peaks, and refined scene/session/agent integration. No 3D,
package, version or commit change has been made.

## Enforced numerical allocation budget

The refined mesh, diffusion/transport operators, mixed solver, multilevel
hierarchies and channel runtime now use app-owned `cfd_memory` allocations.
A single-owner, thread-local scope charges each block to a supplied budget;
the budget must outlive its allocations. Blocks retain their owner when freed
or resized outside that scope. Reallocation admits the new block before freeing
the old one, accounting for the actual overlapping resize peak and preserving
the old block if admission fails. Multiplication/header-size overflow and
system allocation failure have distinct failure codes. Zero-byte budget means
reject allocations; an absent budget means explicitly unbounded.

The reported limit/live/peak bytes include requested numerical buffers and
allocation headers, including setup and hierarchy rebuild temporaries. This is
not total process RSS: libc bookkeeping, executable mappings, JSON, fixture
output arrays and unrelated application allocations are outside this scope.
The existing cell-count budget remains a separate topology guard. Process RSS
must still be measured in cost comparisons. Shared reuse remains deferred for
this app-specific accounting boundary; no shared allocator/API/version changes
are introduced.

`test-cfd-memory` exercises empty through progressively larger budgets, resize
failure preserving contents, zero initialization/alignment, overflow rejection,
nested scope ownership, and cleanup after mesh/operator/solve rejection. A
complete three-step fixture has an exact charged peak of 2,104,993 bytes: that
limit succeeds with identical numerical output, and one byte less rejects
cleanly. The third cached-mass step performs no new charged allocation. All
cases release their charged allocations, including destruction outside the
active scope. Address/undefined sanitizers pass these failure paths.

The qualified 18,240-cell physical obstacle also passes unchanged with an
explicit **192 MiB numerical limit**. Its charged setup peak is 168,085,196
bytes and persistent numerical allocation is 121,263,688 bytes. This is a
real enforced budget, not a limit inferred from final solver storage. The
`test-cfd-refined-local-obstacle` target now includes that cap. Mesh/MG/units
regressions and the physical force/energy checks remain passing.

Scene/session request admission and health/status still need to attach this
budget and expose rejection/peak fields; until then the existing agent model
is not claimed to offer the new bounded refined path. Remaining goal work is
that integration and the serial matched-physical-accuracy cost comparison.

## Refined agent integration and comparison checkpoint

The existing session worker now supports `incompressible_refined2d_v1` through
`cfd_refined_channel_2d` and `cfd_refined_obstacle_2d`. Authored fixed refinement
regions, SI fluids, leaf limits and numerical memory limits reach the native
refined solver. Background control, physical-pressure sampling, digest-bound
leaf/face artifacts and typed budget failures use the existing session contract.
Steady Stokes is one stationary solve with physical time zero; transient runs
advance physical time and retain the transport CFL restriction.

`run_compare` now accepts refined runs with matched immutable scene, fluid,
worker and physical time. Spatial comparisons require nested increasing base
grids and equal timestep; temporal comparisons require equal grids and decreasing
timestep. Steady Stokes rejects temporal comparison because its timestep is not
physical. Separate pressure/viscous/total force vectors and physical energy
integrals report absolute differences and relative-to-fine sensitivity. This
is not a field-error norm or a Richardson uncertainty estimate. `run_assess`
includes requested comparisons without promoting them to general physical
certification. The exact fixed rectangle reference gate remains separately scoped.

Reproduce with `make test-cfd-refined-agent-session`. The real-worker suite
covers asynchronous control and non-advancing pressure observation, allocation
budget rejection, the 192 MiB qualified rectangle, spatial component comparison,
matched-time transient comparison and invalid comparison rejection. Terminal
test workers are explicitly reaped before disposable roots are removed.
The direct C adapter and its saved address/undefined sanitizer checks cover
initialization, samples, stationary semantics and allocation-failure cleanup.

Remaining: serial matched-physical-accuracy cost comparison, complete resolution
policy/acceptance review, and transient efficiency evidence. Existing saved
progress/table entries above are chronological and should be read with this
checkpoint. No Desktop package or 3D adoption is implied.

## Serial accuracy/cost screen

`make benchmark-cfd-refined-accuracy` compiles one optimized native fixture and
runs local and uniform controls sequentially, with a 180-second per-case timeout
and explicit numerical allocation limits. Each row retains executable/log hashes,
process peak RSS, wall time, physical component/energy errors and unsuccessful
qualification. Timeout cleanup kills and reaps the owned process group. No
physical threshold is relaxed to manufacture a matched pair.

| Mesh | Leaves | Wall seconds | Process peak RSS | Pressure / viscous error | Physical gate |
|---|---:|---:|---:|---:|---|
| Local n64 depth7, 3 repeats | 18240 | 10.024 median | 231.8–240.7 MB | 1.755% / .970% | pass |
| Uniform n64 | 30720 | 9.984 | 249.2 MB | 7.117% / 8.448% | fail |
| Uniform n128 | 122880 | 77.132 | 872.3 MB | 4.711% / 6.187% | fail |

All runs finish and satisfy numerical residual checks. Local uses a 192 MiB
numerical cap; controls use 768 MiB. These are admission limits, not claims of
equal process memory. The fine uniform charged peak is 707602668 bytes; its
larger RSS remains separately disclosed. Process time includes setup, solution,
physical observations and fixture diagnostics, but not agent JSON export.

This establishes measured cost/accuracy dominance over the tested uniform
controls, not a speedup at matched passing accuracy: neither uniform control
passes the component threshold. `matched_accuracy_comparison_established` remains
false. Whole-domain doubling is not justified by these results; retain this
negative control rather than pursuing unbounded costly uniform grids. A matched
passing comparison is still outstanding and must not be claimed from this screen.
Evidence: `build/s4-resolution/refined-obstacle/accuracy-cost/report.json`.

The fixture now additionally exposes CPU phase times. A subsequent local run
with the same physical gates gives mesh .099 s, setup .081 s, coupled solve
9.834 s, physical observation .000725 s and diagnostic reconstruction .00907 s.
The physical results exactly match the serial fixture values. These phase times
are a separate profiled run (different executable hash), not retroactively added
to the original report. Solve efficiency is the measured remaining cost focus;
observation-copy or reconstruction optimization is not the next priority here.

## Transient resolution and timestep cost

The continuous manufactured nonuniform transient now records leaf count,
smallest spacing, maximum transport CFL, observed transport dt limit, a nominal
uniform-grid explicit-diffusion dt estimate, setup/evolution CPU seconds and
accumulated mixed iterations. Single-case `n dt` arguments permit bounded
reproduction without rerunning the full convergence suite. The existing default
suite and its unchanged spatial/temporal acceptance assertions remain intact.

On the fixed three-level local mesh over .4 s physical evolution, n=8/16/32
velocity L2 errors are .000709893/.000211952/.0000545767 m/s, with spatial
ratios 3.349 and 3.884. At n32 dt=.005, maximum CFL is .02590 against .25;
the smallest-spacing explicit uniform-grid diffusion estimate is .000152588 s.
The implicit solver therefore advances at roughly 32.8 times that nominal
estimate while retaining the manufactured-solution checks. This is not a
measured explicit-versus-implicit runtime speedup or a general nonuniform
explicit stability theorem.

At n16, dt=.04/.02/.01/.005/.0025 yields evolution CPU costs
.892/1.778/3.536/7.173/14.216 seconds. Successive velocity field differences
converge at ratios 4.074/4.033/4.016. Total exact-field velocity L2 error changes
only .175% between the largest and smallest timesteps because spatial error
already dominates. Thus automatically spending sixteen times as many steps
would be wasteful for this mesh and this requested field observable. Retain
paired dt studies for other cases and observables; do not use .04 globally.

Session health now exposes `transport_cfl_limit` and
`observed_transport_dt_limit_s`. The latter derives from the last accepted
step input fluxes and is unavailable before observations or for stationary
Stokes. Its scope explicitly excludes future stability and temporal accuracy;
constant dt remains required within a run. No unsafe automatic dt enlargement
or variable-step BDF history change has been introduced.

Evidence: `local-refinement/transient-cost-local.log` and hash-bound
`transient-cost.json` under build/s4-resolution. Direct session assertions cover
finite transient bounds and stationary unavailability. The real agent regression
is rebuilt after the diagnostic addition. Remaining work is the final
requirements audit and any identified missing physical/efficiency acceptance;
this checkpoint does not mark the full goal complete.

## Completion audit: evidence admission correction

The standalone local force verifier previously inferred its fixed domain and
did not require a printed true linear residual. It now rejects absent domain or
residual metadata, changed domain length/base spacing, nonfinite residuals and
residuals above 1e-11. Tests exercise those invalid records independently of the
numerical solver. Older logs remain historical evidence but cannot be passed
through this stricter verifier to generate a new certificate.

The current executable reran n32/64/128, depth7, L4. The regenerated report in
`refined-obstacle/current-qualification/report.json` passes the unchanged
component/total/energy and last-pair sensitivity criteria. It retains individual
log hashes, executable hash and current numerical source hashes. n32 remains
unqualified; n64 and n128 pass. True residuals at the passing resolutions are
3.13e-12 and 9.58e-12. Charged memory peaks are 168085196 and 555499900 bytes.
The profiled coupled solves take 9.751 and 52.018 CPU seconds respectively.

These two meshes meet the same 2% acceptance class, but they are both localized
meshes: this does not close a local-versus-uniform matched-passing comparison.
The larger mesh improves pressure and total force while viscous error remains
approximately .98%. Consequently n64/depth7 is the currently demonstrated
lower-cost choice for this fixed 2% reference target, with n128 as its refinement
check. Neither choice is an automatically certified preset for another scene.

Audit status: numerical local refinement, implicit evolution, physical fixed-case
checks, bounded allocations and real agent integration have direct evidence.
The promised local/uniform matched-passing accuracy comparison is unproven;
existing uniform controls fail even though they cost more. The next decision
must preserve that distinction instead of relabeling failed controls as matched
accuracy. Dynamic refinement, 3D, arbitrary geometry and turbulence are outside
this goal. No completion declaration has been made.

## Remove unnecessary work within coupled-solver restarts

The mixed GMRES implementation previously measured the accepted physical
residual only at restart boundaries. It now forms a candidate every ten inner
iterations and tests its unpreconditioned residual against the unchanged
1e-11 criterion. An unsuccessful check leaves the base solution and Arnoldi
basis intact. Boundary-elimination scratch is reused only after its assembly
use ends, so no numerical allocation or memory-cap increase is introduced.
The candidate is committed only when its true residual passes. The old behavior
is retained behind CFD_REFINED_VERIFY_RESTART_ONLY for cost verification.

Three alternating serial n16/dt.005 manufactured transient runs give median
wall times 7.4134 s before and 5.5537 s after: 1.335x speedup, about 25.1% less
time. Both variants solve the same equations at the same timestep and tolerance.
Velocity/pressure error norms agree within 1e-9 and divergence remains below
1e-8. This checks error-norm parity, not byte-identical or full-field equality.
Reproduce with `make benchmark-cfd-refined-residual`; executable/log hashes and
raw timings are retained in local-refinement/residual-cost/report.json.

The full local manufactured suite retains spatial ratios 3.349/3.884 and
fine temporal ratios 4.033/4.016. The 18,240-cell qualified obstacle now uses
330 rather than 360 iterations, with pressure/viscous errors 1.755%/.970%,
physical energy passing, true residual 7.50e-12 and divergence 2.04e-11.
Mixed channel spatial checks, SI-unit/rejected-input checks, exact Couette energy,
allocation-budget cleanup/cached reuse and five rebuilt real-agent tests pass.
The unit fixture also exercises the new scratch path under address/undefined
sanitizers. Historical evidence remains tied to its original executable; it is
not silently relabeled as a run of this optimization.

This improves a measured runtime bottleneck without changing the physics or
acceptance threshold. The local/uniform matched-passing obstacle cost requirement
remains open; the negative uniform controls are not replaced by this separate
same-discrete-problem optimization proof.

## Matched physical-accuracy transient comparison and mesh default

A separate smooth continuous manufactured transient now compares uniform and
local meshes against identical absolute velocity L2 <=1e-4 m/s, pressure L2
<=1e-4 Pa and divergence <1e-8 s^-1 limits. Both use dt=.005 over .4 s, identical
forcing/boundaries, optimized builds and current residual acceptance. Coarse
uniform n8 and local n16 controls fail the physical-error class, so the selected
passing resolutions are not substituted for already adequate coarser controls.

Three alternating serial samples at uniform n16 (2048 leaves) and local n32
(6656 leaves) all pass. Median wall times are 1.365 s uniform and 22.455 s local;
median process peak RSS is 17.65 MB and 69.62 MB respectively. Uniform velocity/
pressure L2 errors are 6.576e-5 m/s and 8.657e-5 Pa; local errors are 5.458e-5 m/s
and 7.112e-5 Pa. The fixed central refinement pattern spends extra work where
this smooth flow does not need it while its coarse outer region still controls
accuracy. This is a real matched-accuracy comparison with an unfavorable result
for local refinement, not an assumed local-refinement speedup.

New unobstructed refined-channel scenes now default to no local regions. Explicit
regions remain available; obstacle defaults retain body coverage. Existing
immutable authored scenes are not rewritten. Real-agent tests check the new
empty-channel request and retain control/sample, budget, reference-force and
refinement comparisons. This is an evidence-based default for the smooth
channel template, not automatic error-driven remeshing.

Reproduce with `make benchmark-cfd-refined-transient-accuracy`. Hash-bound logs,
executable identities, both rejected coarse controls, all repeats, error limits,
wall times and measured RSS are in
`local-refinement/matched-transient-cost/report.json`. Timeout cleanup targets
only the owned process group. The benchmark does not certify obstacle drag:
the uniform obstacle matched-passing gap remains explicit, and earlier negative
obstacle controls are retained. Together these studies distinguish when uniform
versus localized resolution is economically useful without expanding into 3D.

## Original-scope reconciliation and explicit per-run acceptance

The original five-step assessment was recovered from the task's recorded final
answer immediately before this goal was created. Step 3 asks for comparison
against uniform fine-grid flow at the same physical endpoint/local spacing;
step 5 asks for separate 2% force gates, preserved total/energy behavior,
improvement across local refinements, and wall time/memory at matched accuracy.
It does not require both a uniform and a local obstacle mesh to pass before 3D.
Later checkpoints incorrectly elevated that particular experiment to a mandatory
completion condition. That is corrected here without changing any physical gate:
uniform obstacle failures remain disclosed, while matched-accuracy transient
cost evidence and independent obstacle force qualification remain distinct.
Do not spend unbounded compute on a requirement added by the audit itself.

The original explicit deliverables still need a final evidence inventory,
including agent-visible mesh/cost/acceptance diagnostics and phase costs. This
scope correction is not by itself a completion declaration.

`run_assess` now exposes `reference_accuracy.status` as passed, failed,
not_established or not_applicable, with separate pressure/viscous/total and
physical-energy checks. A valid finite numerical solve is required to pass;
completion or a raw cached reference boolean is insufficient. Negative,
nonfinite, boolean or missing diagnostics cannot pass. Coarse failed reference
runs suggest mesh refinement; unsupported physical cases request a matching
reference rather than being labeled inaccurate. General physical certification
remains false. Three focused acceptance tests and all five real-agent tests pass,
including actual coarse failure, qualified reference success and transient
reference inapplicability. `make test-cfd-refined-assessment` reproduces the
invalid/missing-evidence and force-cancellation checks.

## Agent-visible mesh and cost diagnostics

The refined adapter caches level counts and actual minimum/maximum leaf minimum
spacing during construction, avoiding full-mesh rescans on each inspection.
The reported voxel size now reflects the smallest surviving fluid leaf rather
than merely the global integer lattice scale. Health exposes initialization CPU
cost and last accepted transport, coupled solve and physical-observation CPU
costs. Stationary transport and unavailable observations are null. Worker runtime
cost exposes previous completed snapshot/preview publication and final field
export wall times using the existing monotonic worker clock. The scope labels
explicitly separate CPU and wall timing, setup versus per-step cost, and previous
publication versus current export. No synchronous sampling advance is introduced.

All five rebuilt real-agent tests pass, including mesh count totals, the uniform
channel's base-level count, finite solve timing and nonzero publication/export
measurements. Existing qualified-body and memory-failure checks remain in this
same suite. Evidence: local-refinement/refined-phase-diagnostics.log.
