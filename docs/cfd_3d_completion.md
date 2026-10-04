# Bounded 3D CFD baseline: implementation and evidence

2026-09-29. Implemented and verified in the existing `physics_sim_main_edit`
worktree. Canonical source and Desktop package were not changed. Source remains
uncommitted; prior 2D and unrelated Main Edit work is preserved.

C3D-6 subsequently passes the separately documented
[stationary open straight duct gate](cfd_open3d_completion.md). The C3D-1..5
measurements and source/executable identities below are retained historical
baseline evidence; the new completion audit is `build/c3d-open/completion-audit.json`.

Read `cfd_3d_goal.md` for the reference and gates declared before numerical tests.
The rectangular-duct reference is a continuous Fourier solution, evaluated
independently of the native stencil. It is consistent with the Fourier-transform
reference family described by https://doi.org/10.1016/0735-1933(94)90046-9 .

## Requirement audit

| Step | Implementation and evidence | Scope |
|---|---|---|
| C3D-1 conservative operators | Uniform staggered XYZ geometry; one shared face per periodic pair; SI volumes/areas; divergence-gradient negative adjoint; anisotropic, independent-axis, conservation, constant-mode and true-residual tests | Cartesian periodic operators; duct half-cell wall diffusion |
| C3D-2 steady rectangular duct | Full XYZ diffusion retains streamwise stencil, no-slip in Y/Z; prescribed Q determines unknown pressure gradient; physical pressure jump, separate four-wall shear, independent strain dissipation, three grids plus unequal-spacing rectangular case | Fully developed periodic-X duct; no open inlet/outlet claim |
| C3D-3 fully 3D transient | All three velocities, unequal XYZ lengths, continuously derived manufactured forcing, conservative dual-face transport, implicit viscosity, BE startup/BDF2, exact commuting periodic pressure coupling, full momentum residual | Periodic constant-coefficient manufactured Navier-Stokes; separate spatial/time refinement |
| C3D-4 agent integration | Immutable `cfd_duct_3d` and `cfd_manufactured_3d` templates; distinct `incompressible_cartesian3d_v1`; pause/step/continue/cancel; XYZ slices/probes, Pa, comparisons, scoped assessment and digest-bound staggered field artifacts | Existing trusted-local source session service; no package refresh |
| C3D-5 cost qualification | Cached matrices/MG/Krylov; Z-aware aggregation; numerical budget/cleanup, process RSS and publication/export cost; optimized agent runs, sanitizer and existing 2D regression | Serial CPU baseline; no 3D adaptive mesh/GPU/scaling claim |

The duct determines G from the flow constraint: solve A*u=1, compute response Q,
then solve the scalar flow constraint for G and scale u. Its physical pressure is
p=G*(Lx-x), with downstream gauge zero. G is not supplied by the scene. This is
an exact linear constraint elimination for a fully developed duct, not a general
open-domain pressure solve. Prescribed Q agreement is an integral consistency
check; pressure/velocity/shear reference errors establish resistance accuracy.
The transient separately exercises the full three-component pressure coupling.

## Steady duct accuracy

Fixed case: 4x2x2 m, density 1 kg/m3, dynamic viscosity 0.1 Pa s,
Q=0.008 m3/s (mean 0.002 m/s). Finest acceptance: 1% velocity/pressure/flow;
2% each integrated wall shear and physical dissipation.

| Grid | Velocity relative L2 | Pressure-drop error | Max four-wall shear error | Physical dissipation error |
|---|---:|---:|---:|---:|
| 16x8x8 | 3.568% | 5.551% | 5.551% | 11.548% |
| 32x16x16 | 0.978% | 1.474% | 1.474% | 3.149% |
| 64x32x32 | 0.251% | 0.375% | 0.375% | 0.807% |

The first two runs correctly fail overall physical assessment; the third passes.
The accepted pressure drop is 0.00566950 Pa. Integrated momentum/discrete energy
balance is below 1.3e-13 relative, while independently reconstructed physical
energy remains a separate finite-grid error. Numerical residual is 4.01e-12,
divergence below 8e-15 s^-1. The unequal-spacing 4x1.5x3 m, 32x24x32 test passes:
velocity 0.385%, pressure 0.563%, wall shear 0.990%, dissipation 1.240%.
Viscosity doubling doubles physical pressure and wall forces at fixed Q;
density changes leave this steady Stokes resistance unchanged.

## Fully 3D transient accuracy

Periodic box 2x2.5x3 m; three-component manufactured flow and physical pressure
vary across XYZ; continuously derived Navier-Stokes forcing. Matched t=0.4 s.

| Grid, dt=0.001 s | Velocity RMS error (m/s) | Pressure RMS error (Pa) |
|---|---:|---:|
| 8 cubed | 1.4184e-4 | 1.9139e-5 |
| 16 cubed | 3.5804e-5 | 4.8612e-6 |
| 32 cubed | 8.9664e-6 | 1.2197e-6 |

Spatial orders are 1.986/1.998 velocity and 1.977/1.995 pressure. Temporal
self-convergence uses the same 16 cubed mesh at dt=0.04/0.02/0.01/0.005/0.0025 s,
so the spatial floor cannot masquerade as time error. Velocity orders
2.078/2.047/2.026; pressure 1.926/1.972/1.988. All exceed the expected-order
screen of 1.8. Transport preserves each integrated momentum component and has
negligible periodic central-transport kinetic work; repeated BDF2 steps allocate
no numerical storage after the one-time startup operator transition.

Physical strain, forcing power, and energy derivative are exposed separately.
The finest dt=0.005 agent run reports a physical budget residual 4.26e-5 W,
about 0.97% of dissipation. This is a measured reconstruction/time-budget error,
not an exact conservation claim. Pressure is H*phi in Pa; phi itself is a
projection correction and is never advertised as physical pressure. The exact
factorization relies on periodic, constant-coefficient commuting operators.

## Measured cost and memory

Optimized local source worker, serial runs; timings are one bounded measurement,
not a hardware-independent performance guarantee. Step time includes physical
observation. Process RSS includes worker/JSON/artifacts; numerical budget does not.

| Accepted/readback case | Last step wall time | Numerical peak | Process peak RSS |
|---|---:|---:|---:|
| 64x32x32 steady duct | 83.3 ms | 32.5 MiB | 76.3 MiB |
| 16 cubed transient | 5.46 ms | 4.74 MiB | 17.1 MiB |
| 32 cubed transient | 57.4 ms | 37.9 MiB | 101.5 MiB |

The accepted duct run took about 0.225 s including startup and result export;
final full-field export was about 78 ms, ordinary snapshot publication about
1.55 ms. The 32 cubed 80-step transient took 4.63 s. Identical duct reference
points in X are evaluated once per Y/Z location, avoiding duplicate diagnostic
work. Existing 2D MG uses its original bin/coordinate allocation shape; its
2,104,993-byte exact-cap test still passes and one byte below still fails safely.

Memory rejection is typed and releases all owned numerical allocations. ASan and
UBSan pass. LeakSanitizer is unavailable on this macOS platform; deterministic
numerical live-byte cleanup checks cover all exercised setup/failure paths.
Control receipts remain serviced at step boundaries, not inside PCG; the 16 cubed
pause/cancel fixture checks pause response within two seconds and idempotent retry.
This does not establish worst-case response at the largest admitted grid.

## Evidence and reproduction

- `build/c3d/completion-audit.json`: requirement audit, current source/worker digests,
  artifact verification and explicit limitations.
- `build/c3d/operators.log`, `duct.jsonl`, `transient-spatial.jsonl`.
- `build/c3d/transient-qualification.json`: numerical and separate spatial/time gates.
- `build/c3d/agent-evidence/qualification.json`: retained scene/run/artifact evidence,
  physical assessment, cost and exact worker digest. Runs are retained in a digest
  subdirectory; repeat with unchanged executable is idempotent.
- `build/c3d/final-numerical.log`, `2d-regression.log`, `final-regression.log`,
  `cost-final.log`, `sanitize.log`: executed verification.

```sh
make qualify-cfd-3d
make test-cfd-3d-session-sanitize
make test-cfd-cartesian3d-agent-session
make verify-cfd-3d-agent-evidence
make test-cfd-memory test-cfd-sparse-mg test-cfd-refined-transient \
     test-cfd-refined-energy test-cfd-refined-channel-units test-cfd-refined-mixed
make test-cfd-refined-agent-session
```

The agent model admits 4..256 cells per axis, at most 262144 cells, a selectable
numerical cell budget and 1..4096 MiB numerical memory cap. All controls and samples
use the same scene/session ownership contract. Existing 2D APIs remain intact. The native inspector now selects pressure_pa for
CFD, reads the correct field column/units and numerical health/history. Its source
GUI build passes; visual interaction and Desktop installation were not exercised.

## Next bounded sequence

1. **C3D-6: complete.** [Open straight duct evidence](cfd_open3d_completion.md) records the passing boundary gate, cost and limitations. Stop here; the following steps need separate continuation.
2. **C3D-7: complete for the full declared A/B/C contract.**
   [Final wall/transport/startup/open-transient evidence](cfd_wall3d_completion.md)
   records known-answer coupled wall flow, conservative transport and separate
   phase/pressure/wall/energy tests, physical startup, evolving open extensions,
   47 exact-worker agent cases and full requirement audit. The cost audit also
   corrected a source JSON dependency leak. Stop before C3D-8.
3. **C3D-8: physically unqualified; reference robustness now isolated.** Native
   body-edge correction and eight numerical/readback controls are byte-preserved.
   Improved symmetric references pass small last-grid component/reaction changes,
   but genuine normal refinement fails. Grad-div is not a proven shortcut. Exact
   reference-pressure projection separates native trace and solved-pressure
   functional errors on both lengths; estimates remain provisional. Read the
   [reference robustness evidence and next gate](cfd_obstacle3d_reference_refinement_checkpoint.md),
   [experiment contract](cfd_obstacle3d_reference_refinement_goal.md) and
   [original gates](cfd_obstacle3d_goal.md). Stop before new geometry/refinement.
4. Only then evaluate fixed local 3D refinement using matched physical accuracy,
   cells near walls/body/wake, memory and time; do not assume an adaptive hierarchy
   is cheaper on a smooth channel.

Historical C3D-6 invocation (now completed; see the new completion document for the stop boundary):

```text
/c3d-next Read docs/cfd_3d_completion.md and docs/cfd_3d_goal.md in PhysicsSim Main Edit. Implement only C3D-6: the open straight 3D duct, with declared pressure/traction boundaries, independent pressure/shear/energy reference gates, three-grid and outlet-distance checks. Preserve the completed 2D and periodic 3D proofs. Stop after a verified boundary gate and report evidence, cost, and remaining limitations. Do not commit or package.
```
