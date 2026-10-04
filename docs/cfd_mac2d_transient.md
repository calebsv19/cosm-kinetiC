# S3 nonuniform transient refinement gate

## Outcome

**Temporal gate passes; spatial gate fails.** The production solver is unchanged.
This checkpoint adds a reproducible coupled nonlinear transient study rather
than treating pressure residual convergence as physical accuracy.

Run `make test-cfd-mac2d-transient`. It writes raw staggered fields and
`build/s3-mac2d-transient/report.json`, then deliberately exits nonzero when the
accuracy gate fails. This target is separate from the stable regression suite.
The earlier `test-cfd-mac2d` tests still pass; the new transient harness also runs
under address/undefined-behavior sanitizers without reported errors.

## Problem and comparison

Domain is 2 x 1 x .5 m, periodic X, stationary no-slip Y, density 1 kg/m3,
dynamic viscosity .01 Pa s, and zero imposed mean pressure gradient. Initial
streamfunction is `psi = A sin(pi x) sin²(pi y)` in m²/s, with A=.01.
Its staggered discrete curl supplies divergence-free initial face velocities;
normal wall velocity is exactly zero and the continuous tangential wall velocity
is zero. Both velocity components vary in space. Integrate the unforced
nonlinear equations to T=.1 s using the actual production MAC solver.

This is **self-convergence**, not an analytical solution or independent-solver
comparison. The finest field is not called exact. Successive nested solutions
are compared by face-aligned restriction: average tangential fine face samples
onto coarse faces; average pressure over fine cell blocks. Initial velocities
come from the same continuous streamfunction at every resolution. The pressure
gauge is zero mean on each grid. The L2 velocity difference combines squared
component differences divided by Nx*Ny (zero normal wall rows add no error).
Pressure L2 uses cell samples. Observed order is log2(Dcoarse/Dfine).

Spatial study uses 8,16,32,64 cells per active axis and dt=.00025 s. Temporal
study holds 32x32 cells and uses dt=.002,.001,.0005,.00025 s. All runs take
exactly one internal substep per tick, so adaptive stability subdivision does
not silently erase the requested timestep differences. The final stored split
pressure is compared as reported by the solver, including its time staggering.

## Measurements

| Successive spatial grids | Velocity L2 difference m/s | Pressure L2 difference Pa |
|---|---:|---:|
| 8 to 16 | 1.22389e-5 | 4.03171e-5 |
| 16 to 32 | 1.69111e-5 | 2.15691e-5 |
| 32 to 64 | 1.07183e-5 | 6.62816e-6 |

Velocity orders are -0.466 and .658; pressure orders are .902 and 1.702.
The spatial gate requires decreasing differences for both fields at every
refinement and therefore fails. These numbers do not justify a global formal
order claim; coarse-grid cancellation and multiple error sources can distort
self-convergence rates.

| Successive timesteps s | Velocity L2 difference m/s | Pressure L2 difference Pa |
|---|---:|---:|
| .002 to .001 | 2.54693e-7 | 1.94535e-7 |
| .001 to .0005 | 1.27032e-7 | 9.72774e-8 |
| .0005 to .00025 | 6.34382e-8 | 4.86415e-8 |

Temporal orders are 1.004/1.002 for velocity and 1.000/1.000 for pressure,
inside the [.7,1.3] first-order gate. This is temporal convergence to the fixed
grid's discrete solution, not proof that the continuum solution is correct.

At 64x64, halving dt again changes velocity by 3.04976e-8 m/s and pressure by
2.47318e-8 Pa: approximately .28% and .37% of the 32-to-64 spatial differences.
Thus timestep contamination is small relative to the measured spatial issue.
Reducing initial amplitude by ten gives monotonically shrinking velocity
differences with orders .746 and .792; pressure orders .785 and 1.857.
Amplitude changes both nonlinear transport and the resulting pressure, so this
implicates an amplitude-dependent spatial interaction but does not isolate the
upwind term as the sole cause. Wall/splitting error remains a candidate.
Divergence and streamwise momentum checks pass throughout the campaign.

## Next implementation gate

Add an independent manufactured transient solution respecting the same no-slip
walls, with analytically derived body forcing. Compare velocity and pressure
against that known solution, including interior versus near-wall pressure error.
Use the existing conservative scheme as the baseline, then evaluate a bounded
higher-order conservative flux and pressure splitting changes separately.
Require improved errors at matched grid and timestep while retaining divergence,
momentum and boundedness checks. Do not advance outlet/drag acceptance or claim
that more pressure iterations will repair this spatial behavior.

The harness uses the existing app-owned solver directly. No shared numerical
API, agent contract, production numerical method, package or desktop deployment
changes are introduced by this verification-only slice.
