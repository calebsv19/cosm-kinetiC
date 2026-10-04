# Initial 3D CFD improvements and revised direction

2026-09-30, existing PhysicsSim Main Edit. The initial reliability and traction
consistency slice is complete. **Obstacle physical accuracy remains unqualified.**
The [predeclared scope](cfd_3d_initial_improvements_goal.md) and original C3D-8
component, energy, refinement and distance gates are unchanged. No commit,
canonical adoption, package, installation or release occurred.

## Delivered solver and agent improvements

A direct native probe reproduced acceptance of NaN density and NaN flow. Native
cube initialization now independently rejects nonfinite fluid/flow values,
nonpositive values, and null state. The session layer already rejected nonfinite
parameters; direct C callers now have the same basic protection.

The unit-pressure Stokes response is still the same equation and solution. After
flow scaling, the solver checks the actual candidate velocities/pressure for
finiteness and its absolute SI cell divergence before changing accepted fields or
observations. A direct test deliberately bypassing scene admission with very large
flow reproduces scaled continuity failure and verifies preservation of the previous
accepted velocity, pressure, inlet pressure and energy. No tolerances or solve
coefficients were relaxed. Ordinary numerical allocation peak is unchanged.

Obstacle snapshots explicitly state that zero normal wall derivative is imposed
from the exact flat no-slip incompressibility identity. They state that traction
and energy do not share the unrestricted stress of one interpolant. The energy
interpolant remains non-solenoidal. Existing load values and field schemas remain
compatible. Capabilities now disclose the parameterized verification geometry,
unsupported arbitrary scene objects, stationary physical-time semantics, last
accepted inspection state, and limited assessment authority.

A separately built source worker has SHA-256
`c41559fb574bb4bab193adc7ad86f068c0ebbc0a164e7840ff842f722459eff8` under
`build/c3d-initial-improvements/native-build/physics_sim_session_worker`.
The historical `build/cfd-optimized` worker remains byte-preserved. One retained
new-worker n16 agent run checks 32512 fluid/velocity/outlet values against the saved
predecessor: maximum difference exactly zero, with matching solid/null semantics.
Old audits are historical and retain their original identities; they are not
rewritten to describe this source change.

## What the new diagnostics establish

The new solved P2/P1 Stokes fixture uses an exactly representable tangential
quadratic velocity, affine pressure and independent continuous forcing. It tests
all three flat-wall orientations, two anisotropic meshes and two pressure datums
(12 solves). It recovers velocity, pressure, traction and zero divergence to
roundoff; all declared 1e-8 error/residual tests pass. The selected wall is no-slip;
other faces have exact Dirichlet velocity, with one pressure gauge. This proves a
basic solved assembly/traction path, not cube-corner accuracy or the open boundary.

Independent diagnostics retain raw surface traction. On a stationary no-slip
P2 face, tangential derivatives of the zero trace vanish: normal viscous traction
is exactly the corresponding boundary-divergence contribution. Per-face identities
pass. The volume test uses two P2 lifts equal to e_x on the body and zero on the
external boundary. It independently integrates vector-Laplacian and symmetric
stress loads; their difference is the integrated divergence correction. A separate
non-solenoidal zero-trace test proves this identity against independently assembled
forms and checks gauge invariance. These are diagnostics, not new physical gates.

Four frozen gamma0 cube controls retain pressure and velocity coefficient snapshots.
Same-mesh pressure/viscous/reaction/Pin/dissipation match saved predecessors within
1e-7 (actual agreement much closer). No different mesh allocation was introduced.

| Genuine first X-normal subdivision | L4 matched12 | L8 directional12 |
|---|---:|---:|
| Pressure-force change | 1.438% | 1.611% |
| Raw viscous-force change | 4.880% | 5.454% |
| Weak total-load change | 0.00646% | 0.03537% |
| Fraction of viscous change from normal stress | 94.73% | 97.71% |

The volume-lift vector-Laplacian load agrees with the discrete weak reaction.
Symmetric-stress divergence corrections are only 0.094–0.103% of total weak load.
Changing the lift shell from .25 to .4 m changes its symmetric load by .017–.028%.
The integrated bulk weak-form correction therefore does not explain the large
raw boundary stress sensitivity. The evidence isolates a boundary normal trace
problem much more sharply than a small global linear residual or L2 divergence.

This is not proof of a particular inf-sup instability or of the physical corner
exponent. A stable weak total load does not certify its pressure and viscous
components. The independent pressure-force change still exceeds 1%; deleting
normal stress cannot cure that second uncertainty. The earlier pressure attribution
remains provisional. No diagnostic lift, projected normal stress or favorable
reference replaces the original raw surface/component acceptance.

Total reference child wall time was 123.08 s, individual 21.35–41.16 s, maximum
observed RSS 918.70 MiB, with 41088–47232 tetrahedra. All original per-child
50000-tet, 1800-MiB, 180-s and 3000-iteration caps were retained. This is bounded
local cost, not an isolated benchmark or total elapsed time.

## Verification and evidence

- Native invalid input, candidate rejection, accepted-state preservation,
  adjoint/SPD, gauge, cache, cancellation, exact-cap/cleanup and ASan/UBSan pass.
- Two reference test groups pass; 12 solved known-answer cases are retained.
- Actual obstacle MCP/agent tests: 3 pass, including in-solve inspection/cancel,
  exact independent fields, assessment and memory rejection.
- Periodic/cartesian 3D agent tests: 4 pass. Transient 3D: 5 pass. Refined 2D: 5 pass.
- The transient control probe measured sampling 106.6 ms and cancel receipt
  302.9 ms at [64,32,32], with last accepted fields exactly preserved. Sustained
  inspection showed zero measured RSS growth over 150 warmed samples.
- Source worker builds after the existing JSON ownership test. No GUI interaction,
  full matrices, maximum-grid latency or installed Desktop acceptance was run.

`build/c3d-initial-improvements/completion-audit.json` verifies source, logs, frozen
receipts/snapshots, predecessor equivalence and retained agent artifacts. It keeps
`physical_accuracy_certified=false`. Old root C3D-8/refinement-v2 audits and fields
are untouched; new evidence is separate.

```sh
make test-cfd-reference3d-consistency
make run-cfd-reference3d-consistency  # reuses exact retained receipts
python3 scripts/audit_cfd_3d_initial_improvements.py
```

Use the separately built worker explicitly for the agent regressions documented
above. Default source/package workers were not replaced.

## Revised sequence toward useful 3D CFD

1. **Qualify one stronger reference method.** Stop mesh/penalty sweeps. The next
   slice is a small element/support and boundary-stress feasibility gate, targeting
   both boundary incompressibility and pressure accuracy. A divergence-conforming,
   stress-consistent method is justified to evaluate, not proven necessary. The
   installed scikit-fem inventory has tetrahedral P1/P2, mini/CR and RT elements,
   but no ready higher-order tetrahedral H1 divergence-free pair. RT availability
   alone does not supply tangential no-slip and viscous coupling. First establish
   a viable stable element/mesh/solver under measured cost, test known-answer
   flat-wall/channel and natural traction, then apply it to exactly the existing
   cube pair. Acceptance: actual raw normal stress convergence, pressure/viscous
   component normal refinement <=1%, reaction agreement <=1%, and original empty
   calibration. Stop with unsupported/cost failures instead of launching a matrix.
2. **Repair native spatial pressure/traction accuracy.** Once references qualify,
   use exact projection to distinguish solved pressure from reconstruction. Test
   bounded reconstruction/operator changes independently; retain all coarse,
   distance, momentum/energy and agent controls. Accept original 5% separate forces,
   3% Pin/dissipation and distance gates. This is an accuracy slice, not a mesh-size
   increase disguised as an operator fix.
3. **Create a reusable physical scene contract.** Start with authored stationary
   aligned boxes, explicit SI boundary conditions, flux/pressure driving and force
   selection. Route those objects through the same qualified topology and immutable
   revisions as MCP/native inspection. Reject unsupported shapes. Acceptance:
   authored scene -> async run -> live accepted-state diagnostics -> separate
   physical assessment -> artifact readback, with one validated physical example.
4. **Add physical transient low-Re obstacle flow.** Reuse coupled implicit momentum,
   but provide conservative transport across masked dual faces, resolved initial
   conditions, controlled timestep/CFL and actual wake/outlet/backflow policies.
   Qualify unforced physical behavior separately from manufactured transport.
5. **Improve cost and recovery from measured profiles.** Compare uniform versus
   targeted fixed refinement at matched component accuracy. Investigate nested
   velocity solve cost without silently loosening true residuals. Add bounded
   solve progress/deadline and recovery contracts when the real workloads justify
   them. Restart requires full integrator/pressure/control provenance; current
   final fields are not restart checkpoints. Set a maximum-grid latency gate before
   claiming interactive response at every admitted resolution.
6. **Extend geometry and physical models independently.** Curved/STL geometry needs
   geometry-consistent normals, volumes and fluxes. Moving bodies need geometric
   conservation, boundary velocity and coupling. Free-surface water needs interface,
   pressure-jump and gravity/capillary treatment. Atmosphere needs its own transport,
   buoyancy and boundary scope. These are larger solver developments, not template
   aliases over the approximate Wind path.

For reference qualification, an engineering resource cap is not a statement that
50k tetrahedra must resolve every corner. If a sound method cannot satisfy the
physical gates within the cap, return its measured support/DOF/cost obstacle and
choose a bounded revised method or resource contract explicitly. Preserve failed
controls and original physical tolerances.

Shared reuse remains unchanged: cfd_memory/mixed/checkpoint and core_scene session
ownership are adopted; FE/stress policy and local supervision remain app-owned.
Generic core_math extraction and core_jobs migration are deferred. No shared
implementation, API, minimum version or adoption metadata changed.
