# Cube preconditioner goal achieved: force testing resumed

2026-10-01, existing PhysicsSim Main Edit. The
[bounded preconditioner goal](cfd_3d_preconditioner_goal.md) is achieved: the exact
previously failed cube now converges under unchanged caps, and a genuine normal
subdivision has also converged. Force-convergence testing has resumed.
**The physical cube reference remains unqualified; stage 1 overall remains open.**

## Diagnosis and implementation

New probe code reuses the unchanged mesh, cubic/DG-quadratic element, Stokes
assembly, traction and consistency helpers. Separate momentum/continuity residuals
are measured directly from Kx-b; the continuum divergence norm uses the independently
verified pressure mass inverse. All existing reference sources and receipts remain
intact. Numerical source, supervisor source, commands, artifacts and failed controls
are retained in `build/c3d-preconditioner/`.

A deliberately shorter 1000-iteration AMG control on the exact 4992-tet cube gives
momentum residual .000600028 and continuity residual .00000872622, both normalized
by the common RHS norm. The momentum block is 68.76 times larger. These blocks
have different physical units; the common algebraic normalization identifies the
stalled linear residual, not a direct physical-error ratio. A separate mass-dual
continuity diagnostic measures actual volume divergence of the unit response.

A global 1248-dimensional macro-constant pressure restriction of the diagonal-
velocity Schur proxy has eight smallest eigenvalues .003091–.009557, with none
below 1e-12. Constant pressure has nonzero B-transpose action, consistent with
natural ends fixing its datum. This is a global coarse-subspace check, not a full
DG pressure-space nullity or inf-sup certificate. Mesh Jacobian condition numbers
have median 19.37 and maximum 215.37; they depend on vertex ordering and are
conditioning indicators rather than a shape-regularity theorem.

Replacing only the scalar velocity AMG action with a sparse factorization of the
same free velocity matrix clears the solve obstacle. The existing per-element DG
pressure mass inverse remains unchanged. The factor uses 2425940 L/U nonzeros
(fill ratio 2.673), and is reused for all velocity components and iterations.
No pressure penalty, stress projection, matrix regularization, changed boundary
condition or relaxed residual is introduced. This is a bounded reference solver,
not a demonstrated scalable factorization at every allowed mesh size.

Matrix, RHS, mesh and free-DOF hashes are identical across the AMG, factor and
tighter cube controls. Independent tests verify mass scaling against library mass
assembly, symmetric positive preconditioner actions, recovery of a known mixed
solution with either action, unchanged matrix entries and the small-domain natural
pressure mode. Four supervision tests verify immutable success/failure reuse and
rejection of changed commands/artifacts. Eight focused tests pass.

## Results under the original caps

| Control | Tetrahedra | Iterations | True relative residual | Child wall time | Observed own RSS |
|---|---:|---:|---:|---:|---:|
| Exact cube, AMG diagnostic stop | 4992 | 1000 | 6.00e-4 | 21.38 s | 684.69 MiB |
| Exact cube, factor preconditioner | 4992 | 690 | 9.78e-11 | 8.63 s | 730.59 MiB |
| Exact cube, tighter control | 4992 | 849 | 5.60e-12 | 10.10 s | 740.63 MiB |
| Empty duct, factor | 3072 | 150 | 4.50e-11 | 1.67 s | 481.44 MiB |
| Cube, first X-normal subdivision | 6720 | 680 | 9.10e-11 | 13.30 s | 970.70 MiB |

The historical exact cube had reached 3000 iterations with residual 1.405e-5 in
66.88 s. Its failed receipt is unchanged. The new diagnostic control is intentionally
shorter and is not an accepted field. All actual child runs retain the same
50000-tetrahedron, 1800-MiB, 180-s and 3000-iteration hard caps. The final numerical
requirement remains residual <1e-8; ordinary early convergence target is <=1e-10.

The tighter control requested callback target 1e-12, but MINRES stopped internally
at 5.60e-12; that requested target was not reached. Its actual residual is measured
and substantially tighter than the ordinary target. Relative to the ordinary cube,
pressure force changes 3.74e-9, viscous force 2.41e-9, reaction 3.67e-11, and inlet
pressure/dissipation about 3e-11. This establishes stability at the measured
residuals without claiming an unmet target.

The empty duct still passes independent Fourier calibration: pressure error
.01258%, dissipation error .01666%. It reproduces the prior same-mesh reference
within 1e-7 (actual relative differences around 1e-11). Converged cube controls have
finite velocity and pressure, maximum measured volume divergence below 2.2e-9 s^-1,
flux error below 1e-8 and physical energy imbalance below 3e-11. Boundary divergence
is measured from the field. Raw normal viscous load is about 1e-12 N without
imposing zero in the observer.

## Resumed force test and remaining physical gate

The normal control adds exactly two X-axis intervals, bisecting the first
body-adjacent normal interval at both front and back; Y/Z nodes are unchanged.
Raw surface pressure and symmetric viscous loads use the existing independent
traction integrator, with matching order-4/order-8 whole-face quadrature.

| Genuine normal subdivision | Relative change |
|---|---:|
| Pressure force | 11.064% |
| Raw symmetric viscous force | .2412% |
| Weak reaction force | .3280% |

Raw total surface load differs from weak reaction by 3.351% on the first mesh and
4.022% on the normal-subdivided mesh. Thus the unchanged <=1% pressure-component
and raw/reaction gates fail. The small viscous change and tiny normal stress do
not certify the reference. These are coarse controls; no native force-error
comparison or reference accuracy claim is made from them.

The next bounded slice is pressure/corner spatial convergence. First preflight
DOF and memory cost, then test one pressure-relevant grading/refinement change
with this working preconditioner. Preserve raw force and reaction checks; inspect
how pressure load distributes across body faces/edge bands. Numerical stress
integration is already quadrature checked, so distinguish approximate pressure
trace/spatial error from integration arithmetic. Avoid a broad parameter sweep.
The reference must eventually pass component/reaction <=1% robustness on both
L4 and L8 before native pressure/reconstruction changes can use it as ground truth.

Further reference meshes must fit measured memory as well as the tet limit:
4992 tets already carry 76362 velocity and 49920 pressure DOFs, and the first normal
split uses about 971 MiB. Factorization scalability and basis/observation memory
need measurement before assuming that an arbitrary <50000-tet case fits. If the
next required mesh exceeds the same cap, investigate bounded memory reductions
or a stronger iterative action explicitly, rather than raising the cap silently.

## Evidence and boundaries

`build/c3d-preconditioner/completion-audit.json` records `force_testing_ready=true`,
`force_testing_resumed=true`, `stage_1_complete=false` and
`physical_accuracy_certified=false`. It verifies source/equation identities,
resource receipts, snapshots, failed controls, calibration and force gates.
Native source/headers, both existing workers and predecessor evidence are unchanged.
No commit, package, installation or canonical adoption occurred; no native/GUI
regression matrix was repeated for this independent-reference-only change.

```sh
make test-cfd-reference3d-preconditioner
make audit-cfd-3d-preconditioner
python3 scripts/run_cfd_reference3d_preconditioner.py --name cube-factor -- --kind factor
python3 scripts/run_cfd_reference3d_preconditioner.py --name cube-factor-normal -- --kind factor --split
```

The last two commands verify/reuse retained results and do not relaunch a matching
child. A changed command or artifact is rejected. The local helper remains outside
public upload, remote worker and native session admission contracts.
Shared reuse remains adopted for existing cfd_memory/mixed/checkpoint/core_scene
ownership; generic FE/core_math extraction and core_jobs supervision migration are
deferred. No shared implementation, API or version/adoption surface changed.
