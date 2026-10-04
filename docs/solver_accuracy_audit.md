# S3 transport and outlet accuracy audit

Status: local Main Edit source investigation, 2026-09-27. No commit, package,
Desktop refresh, or physical CFD acceptance is implied by these results.

## Assessment

The remaining error is not explained by pressure iteration budget alone. Four
additional seven-second sphere runs, a transport derivation test, and an outlet
regression establish three separate issues:

1. First-order semi-Lagrangian velocity interpolation introduces substantial
   damping even with physical viscosity set to zero. Smaller timesteps can
   increase that damping at a fixed grid.
2. The qualification receive outlet was resetting velocity to a uniform target
   on every tick, although the projection allowed outlet velocity to change.
   This implicit predictor forcing has been removed in qualification mode.
3. The sphere is only 4.8 or 7.2 cells across in these tests. Grid sensitivity is
   large, and no three-level spatial convergence result exists yet.

The pressure solver and reconstructed flux balance can pass while wake accuracy
fails. Neither a settled wake nor a low divergence residual establishes physical
momentum transport, physical pressure, or reliable drag.

## Derivation check: inviscid transported shear

The new `transported_shear` case in `tests/solver_qualification_test.c` carries a
transverse cosine at constant U = 2 m/s with zero physical viscosity. Its exact
inviscid amplitude should remain unchanged. The test measures an interior Fourier
amplitude after 0.2 s, away from appreciable exterior influence.

For wave number k = 2 pi / m, cell size h, and fractional backtrace
f = frac(U dt / h), linear interpolation gives per-step magnitude

    G = sqrt((1 - f + f cos(k h))^2 + (f sin(k h))^2)
    retained amplitude = G^(T / dt)

| h (m) | dt (s) | Measured retained amplitude | Loss |
|---|---|---|---|
| 0.0625 | 0.020 | 0.836498 | 16.35% |
| 0.0625 | 0.010 | 0.713989 | 28.60% |
| 0.0625 | 0.005 | 0.661351 | 33.86% |
| 0.03125 | 0.010 | 0.914904 | 8.51% |

All measured values match the interpolation prediction within 2e-5 absolute
amplitude ratio. This is a characterization of the current algorithm, not an
accuracy acceptance pass. A replacement transport scheme must update this
algorithm-specific expectation and add an independent amplitude/phase accuracy
budget rather than preserving the loss as desired behavior.

The mode-specific equivalent diffusivity, -ln(retention)/(T k^2), is
0.02261, 0.04267, 0.05237, and 0.01126 m^2/s respectively. The first three are
roughly 1,400–3,300 times the configured air kinematic viscosity of 1.59318e-5
m^2/s. These values characterize this smooth mode only: they are not measured
viscosities for the complete Wind simulation, effective Reynolds numbers, or a
quantitative attribution of the entire wake error.

For long waves and Courant number C < 1, the same derivation gives approximately
nu_num = U h (1 - C) / 2. Reducing dt at fixed h therefore does not remove spatial
transport error. Higher resolution helps; timestep reduction alone is not a
reliable quality control. Background on semi-Lagrangian dissipation is available
in [Bridson's fluid simulation course](https://www.cs.ubc.ca/~rbridson/fluidsimulation/).

## Controlled full-simulation measurements

Sphere D = 0.3 m, U = 2 m/s, rho = 1.1612 kg/m^3, mu = 1.85e-5 Pa s;
requested Re is approximately 37,661. Runs last seven seconds, sample every
0.2 s, and use a maximum 256 CG iterations with early convergence. Reported
wake values are central-aperture axial velocity averaged over the last one-second
window, not drag or pointwise error against a reference solution.

The timestep/grid comparison uses an immutable pre-repair worker. The outlet
comparison uses a separate immutable post-repair worker. SHA-256 identity is
recorded in every request and in the evidence manifest. Different worker
identities are never mixed into a refinement acceptance study.

| Pre-repair grid | dt (s) | Near wake (m/s) | Far wake (m/s) | Steady screen |
|---|---|---|---|---|
| 32 x 16 x 16 | .020 | -.332137 | .393533 | pass |
| 32 x 16 x 16 | .010 | -.278525 | .586765 | pass |
| 32 x 16 x 16 | .005 | -.262164 | .723538 | pass |
| 48 x 24 x 24 | .020 | -.208170 | .786881 | pass |

All recorded pressure solves converged, with no velocity clamps in these cases.
For dt .01 -> .005, near/far changes are 0.82% / 6.84% of inlet speed. Differences
are decreasing, but the far wake fails the 2% finite-level screen. This is not
formal temporal convergence at a continuum spatial limit.

Grid 32 -> 48 changes near/far wake by 6.20% / 19.67% of inlet speed. Two levels
are insufficient for the required spatial study, and the difference also fails
the 2% screen. Different voxel occupancy and nearest-cell sample planes contribute
to this comparison; it does not isolate a formal truncation-error order.

## Outlet defect and bounded repair

`wind_write_receive_outlet_slab` copied adjacent density and pressure but reset
velocity to a uniform target. The reset was applied each tick, without a dt-scaled
relaxation. In qualification mode it now copies all three adjacent interior
velocity components into the outlet predictor. Projection still determines the
subsequent correction. The legacy visual Wind path retains its original behavior.

An all-six-face boundary regression failed before this change and passes after
it. Worker physics metadata explicitly calls the new condition a zero-gradient
velocity predictor followed by the free transpose projection, not a calibrated
pressure outlet.

| Post-repair length / grid | Near wake (m/s) | Far wake (m/s) | Steady screen at 7 s |
|---|---|---|---|
| 2 m / 32 x 16 x 16 | -.547479 | -.312837 | fails drift and fluctuation |
| 3 m / 48 x 16 x 16 | -.430455 | -.003378 | passes |

Both use h = .0625 m and dt = .02 s; object and physical sampling stations remain
fixed. Recorded pressure solves converge without velocity clamps. Length changes
near/far wake by 5.85% / 15.47% of inlet speed, but the short case is unsettled,
so these numbers are not a comparison of two accepted steady solutions. Removing
the uniform reset substantially changes the wake; it does not establish that the
new wake is physical. Longer integration could resolve drift, but cannot by
itself establish physical outlet correctness or eliminate transport error.

The outlet remains unqualified. Reverse flow, momentum/energy treatment, and the
relationship between projection correction and physical pressure need their own
boundary formulation and tests. No physical pressure/viscous force implementation
should be accepted on this evidence.

## Next bounded acceptance work

1. Establish a transport accuracy gate with inviscid translation and viscous
   transported shear, measuring amplitude and phase across grids/timesteps.
   Prototype bounded, lower-dissipation velocity transport and check monotonicity,
   wall interaction, divergence, and cost. Do not tune viscosity to cancel
   numerical damping or accept a sharper-looking image as validation.
2. Specify a consistent momentum/face-flux and pressure-boundary contract for the
   CFD lane. The current collocated graphics-oriented representation and masked
   wall sampling need review before selecting a production discretization.
   A higher-order interpolation patch alone will not provide conservative
   momentum or calibrated pressure forces.
3. Establish low-Reynolds-number, independently known baselines before returning
   to this high-Re sphere. Control walls, blockage, geometry resolution, domain
   length, and sampling location; use at least three grid/timestep levels where
   required. Unsteady cases need statistical convergence criteria, not forced
   steady-state acceptance.
4. After transport and outlet gates pass, implement pressure/viscous surface
   forces with dimensional calibration and reference uncertainty. Do not infer
   universal shape drag coefficients from these wake tests.

## Proof and artifacts

Local evidence: `build/s3-derivation-audit/` contains immutable before/after
workers, binary hashes, compact measurements, three assessment JSONs, test logs,
and links to full per-run reports and raw wake samples.

Passed: analytic solver qualification, all-six-face outlet regression, 10 agent
qualification tests, 9 agent session tests, and the complete `test-stable` lane.
The pre-repair outlet regression failure is preserved as expected defect evidence.

Physical acceptance remains `not_qualified`; force measurement readiness remains
false. Installed Desktop remains the prior S2 app. Source tests and local runs
are not package, GUI, release, or predictive-CFD acceptance.
