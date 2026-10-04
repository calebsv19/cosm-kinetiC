# C3D-8A/B/C stationary obstacle checkpoint

Historical predecessor measurements. Current corrected diagnostics and remaining
reference/pressure gates are in the [correction checkpoint](cfd_obstacle3d_correction_checkpoint.md).
The historical audit is retained in `build/c3d-obstacle/correction-v1/predecessor-completion-audit.json`.

2026-09-30. Implemented and measured in PhysicsSim Main Edit. **C3D-8 is
implemented but physically unqualified.** The predeclared
[cube/boundary/reference contract](cfd_obstacle3d_goal.md) remains unchanged.
No commit, package, installation or canonical source adoption occurred.

## Delivered scope

A declares one aligned stationary 1 m cube in a 2 x 2 m no-slip duct, at Q=.008
m3/s, rho=1 kg/m3 and mu=.1 Pa s. Both X ends apply natural vector-Laplacian
traction, mu du/dn-p n=-P n. Pout=0 and Pin is solved from the flow constraint.
These are creeping Stokes equations, without inertial transport; mean body Re=.02.
The scene rejects mean Re>.1, unaligned body faces and insufficient wall padding.
The model is confined; no free-space drag coefficient is used as an acceptance
reference.

An independent conforming tetrahedral P2/P1 Taylor-Hood solver implements the
continuous weak Stokes equations with this same geometry, fluid, boundaries and
Q. It shares no native stencil or force integration code. Its library method is
the [scikit-fem block Stokes example](https://scikit-fem.readthedocs.io/en/stable/listofexamples.html#example-32-block-diagonally-preconditioned-stokes-solver).
It measures pressure force, raw symmetric viscous force, weak boundary reaction,
physical strain dissipation and boundary power separately. A second viscosity
trace enforces exact flat no-slip/divergence boundary identities, but the audit
retains raw symmetric stress as the primary reference instead of choosing a more
favorable result after measurement.

B adds an app-owned compact masked Cartesian mixed solver. Solid cell/face
unknowns are removed, normal body velocity is eliminated, and tangential wall
diffusion uses the actual half-cell distance. Integrated divergence and its
negative transpose couple velocity and Pa pressure. Cached velocity multigrid
and owned numerical budgets reuse existing infrastructure. The steady path omits
a pressure hierarchy whose contribution was identically zero; the n=8 numerical
peak fell from 1,127,864 to 880,096 bytes without changing solved fields.

All six closed cube faces report signed pressure and physical viscous forces,
XYZ totals and zero closed-area vector. Discrete boundary row reactions and
momentum/energy balances remain separate from physical Cauchy stress and strain
dissipation. Quadratic interval-average pressure and wall-shear traces replace
point-value extrapolation; exact no-slip trace identities remove an inconsistent
normal viscous wall term. Physical dissipation uses improved interior/near-wall
gradients. The attempted cubic wall-shear trace worsened momentum closure and was
rejected; its failed control is retained.

C executes four uniform cross-section grids, three outlet lengths and a separate
inlet extension. Existing session/MCP entry points expose immutable
`cfd_obstacle_3d` scenes, `steady_obstacle_duct`, Pa, XYZ planes/world probes,
solid masks, closed force/energy diagnostics, downstream centerline recovery,
comparisons and digest-bound full fields. Upstream measurements use world probes;
pressure deficit is evaluated by the audit against the matched empty-duct
background. Sampling does not advance the solver. Candidate fields remain private
and cooperative cancellation preserves the last accepted state. The template
requires one stationary solve, keeps physical time zero and rejects temporal
comparisons. Single-run assessment always keeps reference accuracy
`not_established` and physical certification false.

## Gate results

| Gate | Result |
|---|---|
| Empty-domain independent reference calibration | Pass: pressure .0374%, dissipation .0400% against continuous Fourier reference |
| Independent body component convergence | Unpassed: L=4 last pressure/viscous changes 2.28%/3.51%; L=8 center=4 changes .592%/3.00%; limit 1% |
| Independent reaction versus integrated total force | Pass: .447% for L=4, .404% for L=8; limit 1% |
| Numerical momentum, divergence, flux, discrete energy and discrete momentum | Pass on all seven cases |
| Native separated force/energy physical accuracy | Unpassed; screening values below use unresolved references |
| Declared three-grid decreasing component errors | Unpassed: viscous error increases from n=16 to n=32 |
| Fixed-spacing outlet and inlet distance | Pass for this creeping cube case; pressure background normalized consistently |
| Native, sanitizer, exact numerical cap/cleanup/cache/cancel | Pass |
| Actual MCP discovery/create/start/sample/step/assess; live sample/cancel | Pass, 3 agent tests |
| Full native versus agent field readback | Pass: all cells, three lower faces, solid mask/Pa, upper X, all seven cases; maximum difference 0 |
| Preserved prior 2D/3D numerical source and focused regressions | Pass; 62 predecessor CFD files unchanged, 3 session/observation bridge files extended |
| Application source link | Pass; no GUI interaction/package acceptance inferred |

Reference convergence percentages use the previous mesh as denominator.
Reference uncertainty blocks certification even where a native screening error
falls inside its limit. Closed pressure/viscous components and physical energy
must independently pass; a small total drag difference is insufficient.

## Uniform-grid measurements

Same L=4 m, cube center=2 m, geometry, fluid and Q on every row. The unresolved
L=4 reference reports Pin=.0223044052 Pa, body pressure=.0244807622 N,
raw viscous=.0149754984 N, and D=.000178979283 W. Values below are comparisons
against that reference, not certified error bounds.

| Grid | Pressure force error | Viscous force error | Total force error | Physical D error | Pin error | Physical momentum closure | Physical energy imbalance |
|---|---:|---:|---:|---:|---:|---:|---:|
| 16 x 8 x 8 | 36.15% | 9.21% | 25.93% | 58.47% | 26.37% | 23.39% | 43.49% |
| 32 x 16 x 16 | 16.31% | 7.18% | 7.40% | 25.31% | 9.87% | 13.16% | 16.96% |
| 64 x 32 x 32 | 7.19% | 8.70% | 1.16% | 8.83% | 3.33% | 7.01% | 5.50% |
| 96 x 48 x 48 | 4.59% | 7.19% | .119% | 4.84% | 1.82% | 4.75% | 2.88% |
| Declared finest limit | 5% | 5% | 5% | 3% | 3% | 2% | 3% |

At the finest grid, pressure force is too low while viscous force is too high.
Their cancellation makes total drag appear substantially more accurate. Algebraic
momentum residual is 3.37e-14 and max divergence 1.14e-12 s^-1; these demonstrate
a resolved linear solve, not a correct physical surface-stress discretization.

Sharp cube edges have singular/less regular stress, so smooth second-order wall
stress convergence is not assumed. The difference between physical surface stress
and exact discrete momentum reaction, reference component oscillation and native
nonmonotonic viscosity screen identify the next error-isolation work. Body-edge
control volumes, boundary quadrature and reconstruction are candidates, not yet
an independently proven dominant defect. A higher-order formula alone was already
shown not to guarantee improvement.

## Boundary distance and independent extended-domain check

At fixed h=.0625 m, extend L=4/6/8 with center=2. Compare the last two lengths;
then shift center from 2 to 4 in L=8 for inlet independence. Use the independently
executed same-grid empty-duct pressure gradient .00141737487658 Pa/m to subtract
ordinary added resistance: excess Pin=Pin-G L; pressure deficit=p-G(L-x).
Recovery uses common body-relative physical probes, with max difference normalized
by the largest magnitude in the profile. Raw outlet-gauged pressure legitimately
increases when downstream duct is added and must not be compared directly.

Outlet L=6 to 8 changes each closed X-force and excess Pin by less than 4.2e-10%.
Maximum recovery velocity/pressure-deficit changes are .000234%/.000468%.
Inlet extension changes pressure force .00191%, viscous force .0000551%, total
force .00109%, excess Pin .00134%, with the same recovery bounds. These pass 1%.
The L=4 to 6 control remains retained; final distance checks are independent of
physical force certification.

The reference was also run on L=8, center=4, with two edge-refined meshes.
Reference components are still unresolved. Its corresponding native 128 x 32 x 32
run screens at 5.08% pressure force, 4.89% viscous force, 1.16% total force,
7.30% dissipation and 2.71% Pin error; physical momentum/energy imbalances are
6.74%/4.51%. The longer-domain check therefore does not rescue the force gate.
A matching finest extended-domain grid is not yet admitted/qualified: uniform
h=1/24 m on L=8 would exceed the existing 262144-cell contract. No cap was bypassed.

## Cost and efficiency

Serial optimized local CPU source worker; one measurement per case, not a hardware
performance guarantee. Solve time includes observation and checkpoint service.
Numerical memory excludes JSON, executable and exports; process RSS includes them.

| Case | Agent end-to-end | Solve/observe wall | Numerical peak | Worker peak RSS | Final full export |
|---|---:|---:|---:|---:|---:|
| n=8, L=4 | .224 s | .0465 s | .839 MiB | 7.34 MiB | 2.33 ms |
| n=16, L=4 | .836 s | .734 s | 7.05 MiB | 21.64 MiB | 15.8 ms |
| n=32, L=4 | 10.68 s | 10.32 s | 58.35 MiB | 105.45 MiB | 128 ms |
| n=48, L=4 | 45.55 s | 44.56 s | 199.24 MiB | 397.86 MiB | 421 ms |
| n=32, L=6 | 24.12 s | 23.62 s | 89.58 MiB | 177.52 MiB | 262 ms |
| n=32, L=8, center=2 | 36.35 s | 35.63 s | 120.81 MiB | 228.53 MiB | 388 ms |
| n=32, L=8, center=4 | 29.16 s | 28.57 s | 120.81 MiB | 246.52 MiB | 254 ms |

The n=48 case contains 221184 domain cells and 207360 fluid cells; n=32 L=8
contains 131072 domain cells and 126976 fluid cells. They use compact topology
but uniform physical spacing, not local refinement. The finest solve services
260 checkpoints, totaling .337 s; ordinary prior publication takes 1.83 ms.
Cancellation is tested inside an n=32 solve below two seconds, with preserved
accepted fields. No worst-case largest-grid pause/cancel latency is certified.

Independent reference peak RSS was 1.59 GiB on L=4 (45024 tetrahedra,
203814 velocity/9950 pressure DOFs, 8.08 s), and 1.59 GiB on L=8
(42048 tetrahedra, 186054/8594 DOFs, 7.15 s). These are verification-only Python
process costs, not native app memory. Continuous forms use polynomial-exact order-2
volume quadrature and order-4 boundary quadrature, avoiding unnecessary volume
basis storage. Edge-only refinement focuses reference work near sharp edges.
Broader uncapped refinement was rejected by automatic approval review after a
prior 2.6 GiB peak; the safer alternative completed with a 50000-tetrahedron
preassembly cap, 1800 MiB own-process RSS cutoff and 180 s deadline per mesh.
The resource caps were preserved; reference tolerances were not loosened.

## Evidence and reproduction

Exact source worker SHA-256:
`f4cd67ce0bc5130e6ddd6ff302b166403b199c79cd22de8a09d36f8b1ddec8c2`.
The tested source uses static json-c 0.18, as established in C3D-7; no global
package-manager dependency was changed. Reference dependencies are isolated in
`build/cfd-reference-venv` (scikit-fem 12.0.2, SciPy 1.18.1, NumPy 2.5.3,
PyAMG 5.3.0).

- `build/c3d-obstacle/completion-audit.json`: current hashes, separate numerical,
  reference, physical/refinement/distance gates, costs and explicit false certification.
- `build/c3d-obstacle/agent-evidence/qualification.json`: seven exact-worker runs,
  original costs, request identities, assessment and full-field native readback.
- `build/c3d-obstacle/reference*-final*.json`, `reference8-L8-cx4-edge-l*.json`:
  independent reference meshes and measurements; bounded receipts alongside them.
- `build/c3d-obstacle/native-empty32-calibration.json`: executed same-spacing duct.
- `contracts-final.log`, `agent-controls-mcp-final.log`,
  `preserved-regression-final.log`, `source-gui-build.log` in the same evidence root.
- `predecessor-sources.json`: pre-C3D-8 hashes; all 62 other CFD files remain exact.

Run from Main Edit:

```sh
make test-cfd-obstacle3d-contract test-cfd-obstacle3d-sanitize
make test-cfd-obstacle3d-agent-session
make verify-cfd-obstacle3d-agent-evidence
make audit-cfd-obstacle3d
python3 scripts/audit_cfd_obstacle3d.py --require-qualified
```

The final command **must exit 1 while these gates fail**. Audit success means the
retained evidence was checked, not that the physics passed. Repeating the matrix
with the unchanged worker verifies and retains executed costs rather than
relabeling cached inspection latency as solve time. To change numerical source,
build a new worker; its digest creates a distinct matrix evidence root.

Reference CLI is `scripts/cfd_fem_reference3d.py`; use
`scripts/cfd_obstacle3d_reference_bounded.py` for this declared bounded edge-only
lane. Existing outputs are preserved. Do not run broad unbounded refinement to
work around a failed resource admission. macOS ASan/UBSan passed; LeakSanitizer is
unavailable, so exact owned-live-byte cleanup is tested explicitly. Linux runtime,
Desktop visual interaction and packaged MCP were not exercised.

## Next bounded correction and stop boundary

1. Establish converged **separate** reference pressure and raw viscous forces
   on the accepted extended domain within a reviewed resource budget. Investigate
   boundary-divergence trace/edge quadrature before simply increasing all volume
   resolution. Keep raw, boundary-consistent and weak reaction measurements.
2. Independently derive and test masked body-edge dual volumes, boundary momentum
   rows and physical traction/strain integration. Isolate reconstruction error from
   equation discretization error; do not tune forces toward the reference.
3. Rerun the same four-grid controls and fixed-spacing distance tests with the
   same gates. Qualify pressure/viscous components, D and momentum independently.
4. Only after that gate, consider fixed local 3D refinement at matched physical
   accuracy and measured memory/runtime cost. Do not add moving/curved/STL geometry,
   inertial wakes, turbulence or atmosphere/water to conceal this unresolved gate.

Suggested continuation:

```text
/c3d-next Read docs/cfd_obstacle3d_goal.md and docs/cfd_obstacle3d_checkpoint.md in /Users/calebsv/Desktop/CodeWork/_worktrees/physics_sim_main_edit. Continue only the unpassed C3D-8 reference and body-edge traction/energy correction gate. Establish converged separate reference forces, isolate discrete boundary geometry versus physical reconstruction, and rerun the unchanged force/energy/refinement/distance thresholds. Preserve 2D and C3D-1..7, measured resource caps and failed controls. Stop before new geometry, moving bodies, turbulence or local 3D refinement. Do not commit or package.
```
