# Stronger 3D CFD reference: support passed, cube solve stopped

2026-09-30, existing PhysicsSim Main Edit. Implementation follows
[cfd_3d_reference_method_goal.md](cfd_3d_reference_method_goal.md). Stage 1's
method support, solved-boundary controls and empty calibration are complete.
**Stage 1 overall remains incomplete; obstacle physical accuracy remains false.**
The first cube reached the unchanged linear-iteration cap, so no force reference
was accepted and stages that depend on it were not started. No native equations,
shared APIs, dependency versions, scene admission or physical tolerances changed.
No commit, canonical adoption, package or installation occurred.

## Delivered implementation

The new independent reference uses continuous cubic tetrahedral velocity and
fully discontinuous quadratic pressure on an Alfeld mesh: one interior barycenter
and four children per macro tetrahedron, preserving all macro boundary facets.
Unlike P2/P1, the velocity divergence belongs to this pressure space. The pair's
mathematical support is described by [Guzman and Neilan](https://arxiv.org/html/1710.08044v1),
sections 1 and 6.1, for degree >= dimension on shape-regular barycentric meshes.
That result does not qualify our implementation or cube force accuracy.

An app-owned cubic element fills the installed library's missing tetrahedral P3
support. Sorted global vertex order makes the two edge DOFs consistent; no library
patch or new dependency is used. Independent tests cover all cubic monomials and
gradients, nodal interpolation, shared-edge/face mapping, split volume/boundary
preservation and the divergence-map rank of a zero-boundary macro element. On
that macro the map has exactly the constant-pressure null mode, with no extra
local pressure null modes at the test threshold.

Twenty-four solved, independently forced Stokes cases use cubic tangential
velocity and quadratic pressure. They exercise two meshes, all three flat-wall
orientations, two pressure datums and both Dirichlet and natural traction boundary
conditions. All declared 1e-8 field, traction, true residual and volume/wall
incompressibility checks pass. Maximum pressure-traction absolute error is
1.54e-11 N; maximum volume divergence is 1.29e-12 s^-1. These are representable
smooth-field controls, not cube-corner qualification.

The reference retains the same vector-Laplacian natural-traction Stokes problem,
mu=.1 Pa s, flow=.008 m^3/s and cube/duct dimensions. Raw symmetric stress is
integrated without zero-normal-stress replacement. The independent linear solve
uses scalar velocity AMG and exact per-element DG pressure mass inverses as a
preconditioner; neither changes the equations or introduces pressure stabilization.

Local supervision freezes numerical sources, records commands/artifact hashes and
retains both success and failure. It reports assembly/setup phases and true
residual progress. New receipts include typed failure diagnostics and runner
identity. Successful and failed receipts are reused without relaunch; changed
commands or artifacts are rejected. Four separate supervision contract tests pass.
The initial numerical receipts precede the additive typed-reporting change; their
frozen source/log identities and failures remain unchanged.

## Actual numerical results

| Control | Tetrahedra | Iterations | Child wall time | Observed own RSS | Result |
|---|---:|---:|---:|---:|---|
| Empty duct, n2 | 384 | 520 | 1.26 s | 91.72 MiB | Pass |
| Empty duct, n4 | 3072 | 1010 | 13.23 s | 433.09 MiB | Pass |
| First L4 cube | 4992 | 3000 | 66.88 s | 638.58 MiB | Iteration cap |

The n4 empty duct's inlet pressure error against the independent square-duct
Fourier series is 0.01258%; dissipation error is 0.01666%, both below the unchanged
1% calibration limits and smaller than n2. Maximum measured volume divergence is
3.24e-12 s^-1. Energy imbalance is 6.29e-12 relative. The exact continuous target
inlet pressure is .005690831017567345 Pa.

The first cube has 76362 velocity and 49920 pressure DOFs, despite only 4992
tetrahedra. Assembly takes .311 s and preconditioner setup .192 s; linear iteration
dominates cost. At the cap, true relative residual is 1.40538e-5, above the 1e-8
final requirement (the early convergence target is 1e-10). Neither result JSON
nor a field snapshot is produced for this failed solve. Its retained log/receipt
are evidence of a failed numerical gate, not physical force evidence. The mesh
Jacobian condition numbers have median 19.37 and maximum 215.37; their dependence
on vertex order makes them diagnostics rather than a stability certificate.

Each numerical child stays within the original 50000-tetrahedron, 1800-MiB and
180-s caps, and the original 3000-iteration limit. There was no refinement or
penalty sweep after the cube failure. An earlier JSON integer-conversion error
was corrected before numerical operation; its frozen failed receipt is retained.
The cube's failure is numerical and distinct from that reporting defect.

## Revised next steps and acceptance

1. **Finish the stronger-reference gate.** Instrument momentum and divergence
   residual blocks separately, diagnose graded-mesh conditioning/global pressure
   modes, and compare a bounded stronger velocity/pressure preconditioner against
   this exact first cube. The tiny local support test does not establish the
   global cube inf-sup constant. Prefer a mesh-aware or auxiliary-space velocity
   preconditioner and evaluate pressure Schur approximation from the measured
   residuals; these are candidates, not demonstrated fixes. Keep equations,
   true-residual thresholds and resource caps. Stop again if a candidate cannot
   satisfy them. Only then run the genuine normal-refinement and component
   convergence controls for both L4 and L8, retaining raw stress and <=1% pressure,
   viscous and reaction gates. No reference uncertainty has yet been reduced for
   the actual cube.
2. **Native accuracy** still follows a qualified reference: isolate pressure
   operator error versus force reconstruction with exact reference projection,
   then require original separate-force, pressure, dissipation and distance gates.
3. **Authored aligned-box scenes** follow that qualified stationary solver, with
   explicit SI driving/boundaries, immutable revisions and artifact assessment.
4. **Physical transient obstacle flow** follows conservative masked transport,
   timestep controls and tested wake/outlet/backflow behavior.
5. **Cost and recovery:** reference progress, capped children and immutable failure
   reuse are implemented. The measured first priority is linear-preconditioner
   cost, not matrix assembly. Native maximum-grid latency, explicit deadlines and
   full-provenance restart still need their own workload-backed implementation.
6. **Extended geometry and models** remain later independent gates: curved/moving
   surfaces, free-surface water and atmosphere each require new conservation and
   boundary treatment.

All native source/header hashes remain identical to the start of this slice.
Both existing worker identities, predecessor reference artifacts and prior test
logs are verified unchanged. No native/GUI rebuild or regression matrix was
repeated because this slice changes only the independent reference and its local
supervision. Eight focused unittest methods pass (including the 24 numerical
cases). New evidence is `build/c3d-reference-method/completion-audit.json`; it
explicitly records `stage_1_complete=false` and `physical_accuracy_certified=false`.

```sh
make test-cfd-reference3d-method
make audit-cfd-3d-reference-method
python3 scripts/run_cfd_reference3d_method.py --case empty4  # retained pass
python3 scripts/run_cfd_reference3d_method.py --case cube2   # retained failure, exit 1
```

The supervisor is a trusted local reference-development helper, not the agent
session's native worker or a remote submission service. Existing cfd_memory,
mixed-solver, checkpoint and core_scene ownership/reuse remain unchanged.
