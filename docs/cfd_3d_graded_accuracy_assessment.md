# CFD accuracy assessment and next development steps

2026-10-04, PhysicsSim Main Edit. This continuation prioritizes physical accuracy
before speed. Force-convergence testing has resumed: two larger graded reference
fields complete under the original equations and strict residual checks. The broad
CFD goal remains active; this is a useful numerical checkpoint, not completed
physical qualification or a native/desktop release.

## What improved

The graded transition combines a thinner first side-face fluid layer with finer
outer intervals. It preserves the cube surface, physical boundary data, pressure
modes, original Float64 equation coefficients and quadrature. Global worst shape
and Jacobian condition do not increase, and volume-weighted shape improves by
36.1% (L4) and 43.8% (L8). The meshes are not a nested uniform-refinement sequence.

| Measurement | L4 | L8 |
|---|---:|---:|
| Tetrahedra | 81,792 | 100,608 |
| Independent reconstructed full FE residual | 9.046e-12 | 8.995e-12 |
| Raw surface/reaction force mismatch, previous | 1.7215% | 1.7265% |
| Raw surface/reaction force mismatch, graded | 1.4037% | 1.4066% |
| Largest separate force change versus previous same-length field | 0.6642% | 0.6188% |
| Relative physical energy imbalance | 4.004e-11 | 3.663e-11 |

The raw force discrepancy falls about 18.5%. Pressure, viscous and reaction forces
separately change below the original 1% refinement threshold; fixed-length inlet
pressure and dissipation also pass. Across L4/L8, each body-force component changes
less than 0.007%. Flux and divergence checks pass. The original raw surface/reaction
1% criterion still fails in both fields, so further stress convergence is required.
Length-dependent tunnel pressure/dissipation are reported as such, with per-run
energy balance checked independently.

The explicitly declared reference investigation allowance is 120,000 tetrahedra,
8192 MiB, 1800 seconds and 3000 iterations on the verified 16 GiB host. Both runs
finish within it. This is a bounded local reference allowance, not a public native
resource policy. Original physical acceptance thresholds are unchanged; speed or
factor savings no longer gate this lane. See the sealed
[graded checkpoint](cfd_3d_accuracy_graded_checkpoint.md).

## Native pressure accuracy is now separated into two measured contributions

The new cubic DG-pressure projection integrates all 20 polynomial moments through
the native sampling volumes. Independent analytic controls check clipped integrals,
discontinuous element identity and the actual pressure-force reconstruction stencil.
Using the accepted graded L4 pressure field, then matching archived native cube
fields with the same geometry, flow, viscosity and tunnel dimensions, gives:

| Native grid | Reconstruction contribution | Field-functional contribution | Total pressure-force difference |
|---|---:|---:|---:|
| 32 x 16 x 16 | -5.7226% | -11.6182% | -17.3408% |
| 64 x 32 x 32 | -3.5620% | -4.7673% | -8.3293% |

Each percentage uses the raw reference pressure force as denominator. The first
column measures applying native reconstruction to volume averages of the reference
pressure. The second is the remaining difference when the identical functional is
applied to the archived native solved pressure. Their signed sum exactly recovers
the total difference. Both native pressure computation and force reconstruction
therefore need attention. These are pressure-force functional measurements against
a reference whose raw equilibrium is still unqualified; they are not a complete
pressure-field error norm or a new native run. The calibrated
[projection checkpoint](cfd_3d_cubic_projection_checkpoint.md) and sealed
`build/c3d-cubic-pressure-attribution/assessment.json` retain the exact inputs.

## Backend correction and evidence

Twelve new support controls pass: six graded-mesh/algebra/resource/publication
controls, three cubic-projection controls and three resource-admission failure
controls. Two new flow fields, two pressure-projection observations and a matched
archived native attribution are sealed with input/source hashes.

A duplicated-key exception in the larger adapter's resource rejection path is
reproduced and corrected in a separate guarded entrypoint. The complete admission
estimate now reaches a structured resource stop; the successful path is unchanged.
Sources that produced the accepted fields remain frozen. Use the guarded local
reference CLI documented in the graded checkpoint for future experiments. Native
solver sources, protected worker and existing qualification records are preserved.

## Next six steps, ordered by physical usefulness

1. Continue cube near-edge stress convergence. Use this graded pair as the next
   diagnostic reference; measure signed stress contributions on both tunnels and
   select a further normal/edge refinement with geometry and conditioning checks.
   Preserve raw pressure and viscous surface forces separately. Require strict
   numerical, conservation, energy and original raw 1% equilibrium tests before
   declaring the reference physically qualified. If a larger allowance is needed,
   declare it before the run rather than relaxing physical thresholds.
2. Improve native pressure/operator and pressure-force reconstruction through
   independent known-answer controls. Measure gauge, boundary conditions, discrete
   pressure gradients and force functional consistency. Cubic projection allows
   reconstruction changes to be evaluated separately from solved-field changes.
   These independent controls can proceed while reference stress convergence is
   open; final adoption still requires native force, energy and domain gates.
3. Establish reusable stationary-object wind-tunnel cases with explicit SI inputs,
   geometry validity, boundary semantics, diagnostic outputs and independent
   spatial/domain convergence. Qualify additional shapes separately rather than
   transferring cube accuracy to arbitrary objects.
4. Qualify physical unsteady and inertial transport. Audit the governing terms,
   then prove temporal convergence, mass/momentum conservation, energy behavior
   and known low-Reynolds-number cases before using wake predictions as evidence.
5. Improve usability, saved diagnostics, restart/recovery and then performance
   once these physical cases are stable. Preserve verified equations and results
   while reducing computational cost.
6. Treat moving/curved bodies, turbulence, free surfaces and thermal coupling as
   separate model extensions, each with explicit validation evidence. The current
   steady Stokes reference cannot certify those behaviors.

The immediate boundary is steps 1 and 2. The most useful outcome is a converged
reference stress/force check plus measured native pressure accuracy, not a faster
preconditioner. No commit, package, install or default promotion is part of this
checkpoint.
