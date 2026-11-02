# Independent transport and object-heating accuracy references

This opt-in source/headless diagnostic lane exercises native operators against
independent continuous references. It does not change the ordinary solver,
transport defaults, installed UI or released worker.

## Scalar reference

`accuracy-reference-probes` builds `physics_sim_scalar_accuracy_probe.c` against
the ordinary open-atmosphere operator. Periodic XY, zero vertical flow and a
constant X velocity isolate a scalar coordinate without skipping native momentum.
The manufactured initial temperature rise is a positive Fourier mode represented
by exact cell averages. Translation, physical diffusion and their combination
are checked at16/32/64/128cells along X, with4cells in each transverse direction.
The independent Python reference integrates the mode over cells and applies
exact translation and diffusion decay. Errors are measured relative to the
perturbation above the1K baseline; phase and amplitude are reported separately.

At64cells, combined0.8s error is about0.205%. This smooth result is deliberately
narrow. It does not establish open-boundary, boundary-layer or faint-front accuracy.

`accuracy-compact-probes` reuses the unmodified native compact cos⁴ pulse tool
with production minmod. Independent positive clipped-cell quadrature evaluates
the exact translated zero-background pulse. At3.2s, field relativeL2 errors are
62.5054%,46.6906%,25.4383%,9.39945% for32/64/128/256cells. The64halfstep result
is46.8459%. Optimized/sanitizer profiles agree exactly. This demonstrates strong
localized spatial sensitivity despite the smooth-wave result; it does not
uniquely identify a solver defect or authorize changing ordinary transport.

## Authored object reference

`physics_sim_object_accuracy_probe.c` exercises the existing fixed physical
support with h=2W/(m²K), C=10J/K and depth0.125m on the actual low box. Heating,
cooling and nonuniform initial exposure are compared with the independently
solved finite-gas/object two-reservoir ODE. Gas is neither prescribed nor reset.
Native signed heat and total gas/object energy are checked independently.
The current references agree to about1.4e-10K; halfstep reduces the pair-splitting
residual. This qualifies the authored nonlocal exchange calculation, not a
resolved wall-gradient law, radiation, material calibration or hot-fire physics.

## Fixed-input gust comparison

`coupling.accuracy_gust_cases.configuration` provides exactly three named
controls:32³dt0.005s,64³dt0.005s and64³dt0.0025s. The world-space source/box,
object properties and physical gust onset0.4s/release1.2s remain fixed. Every
case replays the same retained native Fire pulse with1.2062387959304033J;
there is no live source feedback or source-dose fit. Source delivery ends1.6s,
horizon3.2s; journal checkpoint period0.2s.

`fire_fluid_accuracy_gust.py` requires a passed independent-reference result,
worker/pulse paths and fresh output/budget roots. Existing conservative source
mapping, exact native clocks, packet acceptance, immutable lossless SQLite,
thermal/mechanical ledgers and consumer-checked staging retirement remain owners.
The three numerical histories and full51-checkpoint/12-snapshot audit are
complete. Raw object-curve grid/timestep-half differences are
10.141010% / 0.235604%. See current_truth.md for the measured envelope. Completed roots are never reused, and failure reproducers stay.

Compare extensive heat/smoke by sums and normal MAC velocities by face-area
averaging, at matched physical times and fixed wake regions. Report raw heating,
input-normalized sensitivity, support exposure and boundary losses separately.
Two grids do not establish convergence, and nonlinear differences cannot be
subtracted into an additive timestep/voxel error budget.

## Local probe commands

With a fresh task-owned `BUILD_DIR`, `make accuracy-reference-probes` emits
`accuracy-reference/scalar`, `object` and their sanitizer variants. The scalar
arguments are `<cells> <dt_s> <mode> <horizon_s>`, where mode0is translation,
1diffusion and2combined. Supported scalar cells16/32/64/128,dt0.005/0.0025 and
horizon0.8/20s keep the reference bounded. Example: `scalar 64 0.005 2 0.8`.
The object arguments are `<side> <dt_s> <mode>` for sides16/32/64 anddt0.01/0.005;
mode0heats,1cools,2uses nonuniform warm gas,3is the cooling control. The object
horizon is3.2s. Example: `object 64 0.005 2`.

Use `accuracy-compact-probes` for the existing compact native reference. Its
arguments are `<cells> <dt_s> <conductivity> <axis> <half_width_m>`; the retained
comparison uses `compact 64 0.0025 0 0 0.0625`. Keep fresh output, compact native
logs and exact inputs; retire successful disposable builds with the owning
lifecycle only after consumer checks. Never rebuild or clean retained workers
or reuse a scientific case root to rerun a command.

The retained17×17source at7.8125mm spacing spans125mm: only4Fluid cells across
at32³and8at64³. Its heat-weighted horizontal standard deviations under ordinary
nodal dual-cell mapping are about21mm, or1.3–1.4fine cells. This describes the
source distribution, not a resolved plume/turbulence scale. All80retained Fire
frames conserve energy/smoke and restrict exactly between the two Fluid grids,
so identical delivered heat also has a matched physical deposition footprint.
