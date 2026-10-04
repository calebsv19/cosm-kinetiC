# C3D-8 symmetric reference and pressure attribution contract

Declared 2026-09-30 before new solve measurements. Main Edit only; no commit,
package or adoption. Keep all prior failures and original physical thresholds.
The native solver, reconstruction and eight prior field artifacts stay unchanged
unless an independently demonstrated defect justifies a separately tested change.
Existing allocator/MG/session APIs are reused. Generic core_math extraction is
deferred; geometry-specific reference and stress policy remain app-owned. No shared
API/version/adoption changes are planned.

1. Independently check planar surface traction integration on analytic polynomial
   velocity/pressure. Separate raw symmetric normal/tangential contributions,
   pressure, body faces and edge bands. Compare facet quadrature orders 2/4/8;
   P2 gradient and P1 pressure are linear on each planar facet, so integration
   should already be exact. Retain normal stress; do not substitute trace-projected
   stress or weak reaction as the required physical reference.
2. Refine the independent reference near cube edges in one fluid octant, then
   reflect the conforming octant mesh. Use graded base8, gap4, long6, gamma=0;
   sequence no local refinement, one edge pass at distance .12 m, two passes
   (.12,.06 m), optionally a third (.12,.06,.03 m) only if admitted by the
   existing cap. Preflight counts before any matrix assembly. Preserve rejected
   meshes. Both L8/cube4 and L4/cube2 use the same geometric policy.
3. Reference caps remain 50000 tetrahedra before assembly, 1800 MiB own-process
   RSS and 180 s per solve, original 3000 linear iterations and true residual
   <1e-8. Qualification requires final separate raw pressure/viscous force and
   physical dissipation changes <=1%, and physical surface total versus weak
   reaction <=1%; continuous empty-duct calibration remains required. Adaptive
   reference meshes do not authorize native adaptive 3D grids.
4. If local bisection cannot adequately resolve normal stress under the cap,
   predeclare a separate allocation control before running it. Never cherry-pick
   a favorable pair, hide a normal stress, or loosen thresholds to certify.
5. Independently integrate the P1 reference pressure over Cartesian near-body
   slabs by exact affine integration of tetrahedron/box intersections. Apply
   the native interval-average pressure trace to those projected references.
   Validate clipping with known affine fields/volume and the native trace with
   constant/linear/quadratic cell-average pressure on both duct lengths.
   Decompose pressure-force discrepancy into reference trace reconstruction
   error and error of the native reconstructed-pressure functional. This does
   not claim a full-field norm or prove a singular field's convergence.
6. Apply the measurement/projection method to both lengths even if a reference
   gate fails; label all uncertainty. Preserve the matched native grids, retained
   reference controls and all failed receipts. Stop with evidence/cost/limitations
   before STL/moving bodies, inertia/turbulence, native refinement or releases.

## Mesh allocation control declared after count-only preflight

Two broad local edge-bisection passes exceed the cap (L4 79576, L8 72440 tets);
retain count receipts without assembling. Narrow second passes either still
exceed the cap or add too few cells to establish general edge accuracy. Run only
one broad edge pass per length as a diagnostic control, not a converged sequence.

Use a separate fixed allocation sequence: cosine body subdivisions 12/14/16/18,
cubic outer gaps3 and long intervals3, mirrored gamma0, on both lengths. These
concentrate degrees of freedom at all body faces/edges and remain below 50000
tets. Compare body16 with long4 against long3 to test coarsened background,
requiring <=1% force/Pin/D changes. Calibrate the body18/gap3/long3 empty mesh
against continuous Fourier Pin/D (<=1%). If its empty mesh exceeds 50000, use
body16/gap3/long3 and retain the rejected preflight. Record all four grids and
background controls, not a selected favorable pair. Qualification needs original
final component/reaction gates plus background/calibration controls; report any
nonmonotonic or unresolved component separately. Do not infer an asymptotic rate
from a small last difference. No change to native grids or equations.

The count-only empty-grid formula also rejects body16/gap3/long3 at 63888
full-fluid tets. Retain both empty18 and empty16 rejections, then calibrate
body14/gap3/long3 (48000 tets), the finest admitted empty allocation. This fallback
is declared before calibration results and never changes the mesh admission cap.

## Follow-up allocation declared after coarse-gap measurements

The gap3/long3 sequence has small final component changes but fails physical
surface/reaction agreement; the body16/long4 background control also reveals
sensitivity. It cannot qualify by a small last difference. Retain that whole
sequence as failed controls. Run gap4/long4 body8/10/12/14 on both lengths,
keeping the previously used transverse resolution. Final body14 has 47424 tets.
Test body12/long4 versus body12/long6 background resolution on both lengths;
L8's prior matched mirrored body12/long6 result is retained, and L4's matched
control is newly executed. Preserve <=1% background force/Pin/D and original
component/reaction/calibration gates. This control is declared before its force
results. Use body10/gap4/long4 empty calibration (46656 full-fluid tets) if admitted.

## Directional allocation declared after gap4/long4 controls

Gap4/long4 still fails surface/reaction agreement (~1.84% on L8), despite small
body-grid changes. Shortening the long-axis allocation increased the first normal
interval next to the X body planes from .0162 m to .0547 m. A fixed background
must preserve that normal resolution. Test mirrored body counts [8,k,k] for
k=8/10/12/14 with gap4/long6, both lengths, gamma0. The last mesh has 48672 tets.
This changes the allocation of reference cells, not physical geometry or native
mesh. It keeps all long-axis nodes of the original reference and refines both
transverse directions together. Compare [8,12,12] to the matched original
[12,12,12] gap4/long6 control to expose unresolved longitudinal body variation;
require each force/Pin/D change <=1%. Preserve all components and normal stress.
Use existing empty gap4/long6 calibration with same prescribed length/Q or a
fresh matched mirrored empty10/gap4/long6 if admitted. No favorable subset or
component replacement can certify. All previous allocations remain in the audit.

## Short-duct matching control declared after directional measurements

L8 directional final raw force changes pass (P .447%, V .885%), but L4's last
viscous change is still 1.691%; retain it as unresolved. Match the near-body X
fluid nodes physically across duct lengths instead of rescaling the whole X
interval: take distances 3.5*(i/6)^3 from the body plane, truncate at the actual
outer boundary, and append that boundary. L4 has five fluid intervals per end,
L8 has the original six. Run L4 body counts [12,k,k], k=10/12/14, gap4, these
matched physical X nodes, gamma0. Final has 49776 tets, below the unchanged cap.
Compare the matched grid [12,12,12] with original L4 [12,12,12] long6 to expose
X-normal node sensitivity (<=1% force/Pin/D for independent reference acceptance).
Test empty [10,10,10] under the same node policy against Fourier Pin/D. Retain any
uncertainty rather than treating a small last change as a complete certificate.
This allocation is declared before its physical outputs and is not a native change.

## Genuine X-normal refinement control

The physically matched L4 family passes final raw component changes and
surface/reaction checks, but differs from the earlier rescaled-X family by
~3% in raw viscous force; the old family's nonzero normal stress remains a failed
robustness control. Before accepting the improved family, insert a new node at
half the first physical X-normal interval, retaining every old physical X node.
Run L4 [12,12,12] and L8 [8,12,12] with this subdivision; counts 47232/45888.
Require <=1% P/raw V/Pin/D changes against their corresponding unsplit matched
meshes. Keep original rescaled-node sensitivity visible; do not relabel its
failure as passing. This control is declared before results, preserves the caps,
and distinguishes actual normal refinement from changing the node policy.

## Divergence robustness control declared after true normal refinement

The genuine L4 normal subdivision changes P 1.438% and raw V 4.880%, failing
robustness even though unsplit last-grid changes pass. Do not certify either
family by small last differences. Test consistent grad-div gamma=mu=.1 and
10 mu=1 on the final matched L4 [12,14,14] and directional L8 [8,14,14] meshes.
Use a pressure-mass Schur approximation scaled by mu+gamma, appropriate for the
added divergence stiffness; retain the previous gamma1 iteration failure.
A monitored true-residual stop at <=1e-10 may terminate MINRES independently of
its preconditioned recurrence tolerance; this is stricter than the original
required <1e-8 true residual, keeps 3000 iterations/180 s/1800 MiB, and preserves
separate physical/stabilized reactions. Prove gamma0 same-mesh measurements still
match the prior implementation within 1e-7. Any gamma-dependent changes >1% or
raw surface/weak reaction mismatch >1% prevent acceptance; do not fit gamma to
native drag. If the final controls fail, report the reference blocker explicitly.

The stronger gamma=1 final short reference fails surface/stabilized-reaction
agreement (1.545%); retain it rather than substituting the unstabilized reaction.
Test gamma=mu=.1 on the two genuine normal-subdivision meshes as a final bounded
robustness check. Compare against the corresponding unsplit gamma=.1 fields;
require <=1% P/raw V/Pin/D changes. If either fails, stop the reference
qualification attempt and report the pressure/velocity/normal-trace blocker;
do not add more mesh allocations or stabilization coefficients in this slice.

## Reproduction commands (development lane, existing reference venv)

```sh
python3 scripts/cfd_obstacle3d_reference_refinement.py
python3 scripts/cfd_obstacle3d_reference_gap4.py
python3 scripts/cfd_obstacle3d_reference_directional.py
python3 scripts/cfd_obstacle3d_reference_short_match.py
python3 scripts/cfd_obstacle3d_reference_normal_split.py
python3 scripts/cfd_obstacle3d_reference_divergence.py
python3 scripts/cfd_obstacle3d_reference_divergence_normal.py
build/cfd-reference-venv/bin/python scripts/assess_cfd_obstacle3d_pressure_attribution.py --short-reference L4-matched14
make test-cfd-obstacle3d-reference-measurement test-cfd-obstacle3d-pressure-trace
make audit-cfd-obstacle3d-reference-refinement
```

Successful and failed reference receipts are immutable and reused by these
commands. Each successful record verifies its output and pressure snapshot hashes.
The current default obstacle audit routes to this newer robustness audit; prior
correction audits are retained in their predecessor evidence files.
