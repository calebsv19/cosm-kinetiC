# Accuracy checkpoint: local cube rejection and reproducible native regression

Date: 2026-10-04. Authoritative source work is in PhysicsSim Main Edit. These are
local source proofs, with no native equation/default change or package/install
acceptance. Accuracy remains the first priority; resource limits supervise runs
without replacing physical acceptance.

## Completed results

The genuine radius-0.04 m local L4 cube refinement completed on 88320 tetrahedra.
Its independently reconstructed original P4/DG-P3 residual is 8.8801e-12, flux
error 5.1321e-11, maximum divergence 7.5617e-10, and physical energy imbalance
3.8725e-11. It meets all these original numerical gates. Wall time is 825.951 s
and owned memory 4876.984 MiB, inside the declared 1800 s/8192 MiB envelope.

| L4 field | Pressure force N | Raw viscous force N | Reaction force N | Raw mismatch |
| --- | ---: | ---: | ---: | ---: |
| Retained graded baseline, 81792 tet | 0.024785200 | 0.014289327 | 0.039630825 | 1.403700% |
| Rejected local refinement, 88320 tet | 0.024544260 | 0.014317061 | 0.039586231 | 1.831214% |

Separate pressure/viscous/reaction changes are 0.9721%/0.1941%/0.1125%; fixed-length
inlet pressure and dissipation changes stay below 0.079%. However, raw equilibrium
gets worse by 30.46%. The prospective L8 selection rule required at least a 10%
improvement. It fails, so no L8 candidate is run or adopted. The larger pressure
force movement is a measured two-field difference, not proof of its cause.

The unchanged reference traction observer now passes an independent calibration
on both actual L4 meshes. Prescribed quartic divergence-free velocity and cubic
pressure give analytic Cauchy stresses. Independent tensor Gauss integration checks
six individual face loads, including normal viscous stress; closed-surface sums
also match the analytic volume identities. Two pressure offsets (0 and 1.7 Pa)
and orders 4/8 produce eight controls. Maximum face-load error is 3.775e-15 N.
This supports observer signs, normals, polynomial interpolation and integration;
it does not establish accuracy of the solved singular cube flow or edge-band
allocations. The case prescribes fields and runs no PDE solve.

## Reusable native accuracy command

A new developer regression command freezes its complete native source/header/test
packet, compiles with C11 and warnings as errors, and runs the independently forced
smooth steady-Stokes case at n8/16/32/64. It verifies numerical residual/divergence,
physical error reduction, declared error targets and 84 independently integrated
forcing samples. A monotonic parent watchdog and sampled child RSS supervision
supplement each C fixture's owned-memory and wall bounds. Stdout/stderr, compiler
identity, frozen inputs and artifact hashes are retained in a unique run directory;
a failed compile or numerical gate gets a failed receipt. Existing names cannot
be overwritten. This is an application-specific accuracy test, reusing the existing
fixtures, NumPy and Python supervision; it introduces no shared runtime API.

From a source checkout with clang and NumPy available (the existing local reference
venv supplies NumPy):

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  build/cfd-reference-venv/bin/python \
  scripts/run_cfd_native_accuracy_regression.py --name my-new-accuracy-check
```

This is an optional developer verification command, separate from the public
headless first-start workflow. Use a fresh name for every run. Results live under
`build/c3d-native-accuracy-regression/runs/<source-bundle>/<name>/receipt.json`.
The initial frozen compile failure revealed a missing transitive header in the
packet. The complete header packet was repaired; the failed receipt is retained.
The second run passes all four numerical solutions and 84 forcing checks.

| Resolution | Velocity relative L2 error | Pressure relative L2 error |
| --- | ---: | ---: |
| 16x8x8 | 34.3487% | 8.1430% |
| 32x16x16 | 7.3061% | 2.1488% |
| 64x32x32 | 1.7066% | 0.5399% |
| 128x64x64 | 0.4221% | 0.1351% |

Final observed orders are about 2.015/1.998; the final numerical residual is
2.709e-14 and maximum divergence 1.241e-14. Errors repeat the previous standalone
calibration. The current known answer vanishes near the cube/walls/ends. It proves
smooth interior field convergence, not nonzero cube-wall shear, edge singular
forces, arbitrary objects, inertial flow or physical time evolution.

## Next physical improvements

1. Add an independently forced native known answer with nonzero shear at the cube
   wall and exact one-sided boundary derivatives. Keep the support away from cube
   edges so a boundary-stencil error can be distinguished from edge singularities.
   Compare face-average velocity, cell-average pressure and wall traction across
   grids; verify forcing without using the native matrix to generate it.
2. Diagnose pressure and viscous stress convergence near sharp cube edges on the
   retained graded baseline and rejected local field. Select a nested refinement
   that resolves both normal and tangential directions. Require actual paired-cell
   identity, physical geometry, mesh quality and unchanged full equations. Evaluate
   several refinement levels rather than assuming one bisection is sufficient.
   Original raw equilibrium 1%, separate force refinement 1%, fixed-length scalar
   refinement 1%, and paired-tunnel force 1% gates remain mandatory. Inlet pressure
   and dissipation vary physically with tunnel length.
3. Use these controls to choose native pressure/operator/traction corrections.
   Four-interval pressure reconstruction is currently a useful diagnostic only;
   it needs the boundary known answer and independent force comparison before
   native adoption. Preserve the existing native separate-force 5% and scalar 3%
   qualification gates against a physically qualified reference.
4. Then support reusable stationary aligned-box scenes with explicit SI inputs,
   consistent solver/observer geometry, reproducible artifacts and truthful
   asynchronous agent assessment.
5. Add physical transient/inertial transport and time-step convergence with
   conservation, energy, outlet and force histories. Steady Stokes convergence
   does not qualify wakes or time-varying wind behavior.
6. Qualify curved, moving, turbulent, thermal and free-surface behaviors using their
   own physical tests. Performance work follows demonstrated physical accuracy.

## Authoritative artifacts

- Local physical assessment: `build/c3d-graded-local-field/L4-physical-assessment.json`.
- Local terminal receipt: `build/c3d-graded-local-field/runs/997ff825bfdeaa090544262d1bfcf32dc67efc39d2c92dda93df8db11c6c311f/L4-local-r040-receipt.json`.
- Traction calibration: `build/c3d-cube-traction-polynomial/checkpoint-audit.json`.
- Native regression receipt: `build/c3d-native-accuracy-regression/runs/624d131df03daf549819804cb5cf73e2d12ccda90b05dcfdfa5e469b2b838dc2/accuracy-first-02/receipt.json`.
- Batch verification: `build/c3d-local-continuation/closeout-verification.json`.

The earlier [physical batch assessment](cfd_3d_accuracy_physical_batch_assessment.md)
retains its historical pending-launch state. This checkpoint records the terminal
result. The broader 3D CFD goal and cube physical qualification remain open.
