# Current accuracy assessment and next CFD steps

2026-10-04, PhysicsSim Main Edit. Accuracy precedes efficiency. The immediate
capability to resume force-convergence testing has been recovered; the wider useful
3D CFD goal remains active and incomplete.

## What now works

The [accuracy-first checkpoint](cfd_3d_accuracy_first_checkpoint.md) contains two
accepted finer steady Stokes reference fields, independently verified original full
FE residuals below 9e-12, conservation/energy checks, frozen sources and atomic
publication. Pressure, viscous and reaction mesh changes stay below 0.3%, and
fine-pair body-force domain changes below 0.2%. Numerical completion costs are
154.383/231.207 seconds and peaks 1933.078/2456.672 MiB under an explicitly declared
3072 MiB/600 s contract. Historical failed controls remain preserved. Execution
speed and factor savings no longer decide whether an accurate physical trial is
allowed in this lane.

Raw surface/reaction mismatches remain 1.7215/1.7265%, above the original 1% target.
Stable consecutive values and tiny linear residuals do not certify physical force
accuracy. The independent signed observer recovers the L4 -0.683054 mN stress gap;
large near-edge contributions and cancellation elsewhere guide refinement without
replacing physical pressure or viscous traction.

Inlet pressure and total dissipation are length-dependent in a no-slip duct; their
approximately 25.5% changes under L4-to-L8 extension are reported physical responses.
Fixed-length scalar mesh convergence and each run's energy balance remain checked.
The original domain checkpoint supports this interpretation; later sealed summaries
that called these domain scalar changes a failure are historical, not current rules.

## Prepared spatial controls and corrected quality values

The side-layer survey halves the first Y/Z normal spacing from 0.0625 to 0.03125 m.
Redistribution moves existing planes; bisection adds planes. Both preserve actual
cube triangles, boundary areas, volume, reflected cells and Y/Z exchange. Geometry
archives do not contain solved fields, and no new factorization has occurred.

| Geometry | Tetrahedra L4 / L8 | Max Jacobian condition L4 / L8 | Worst intrinsic shape L4 / L8 |
|---|---:|---:|---:|
| Accepted finer parent | 43008 / 49920 | 107.872 / 181.230 | 21.450 / 42.442 |
| Normal redistribution | 43008 / 49920 | 215.373 / 362.240 | 53.345 / 106.448 |
| Normal bisection | 62976 / 72384 | 215.373 / 362.240 | 53.345 / 106.448 |

This corrects a transcription error in the initial sealed side-layer document,
which listed parent intrinsic shape as 26.913/53.353. The survey and audit JSON
always held the correct values above; geometry and PDE measurements did not change.
The sealed document/audit are retained as history. Use this current assessment.

Neither control is selected for numerical trial. First-layer resolution improves
but transition anisotropy worsens. This is a mesh-design finding, not a force failure
or rejected numerical factorization. The geometry-only survey permits 75000 cells;
it does not change the current 50000-cell numerical contract.

## Recommended sequence

1. Design a graded transition around the identified side-face edge cells. Balance
   normal, tangential and streamwise resolution; measure local/global quality and
   conditioning, preserve the actual cube and matched-domain geometry, then perform
   complete fresh factor/scratch/both-basis admission. If an appropriate accuracy
   mesh requires more than the existing numerical contract, declare its bounded
   resource revision before execution rather than weakening a physical criterion.
   Solve L4, inspect separate force/scalar changes and raw equilibrium, then advance
   the matched L8 with the same physics. The speed of the solve is secondary.
2. Qualify native spatial pressure and traction. Once the reference is sufficiently
   qualified, project the reference onto native degrees of freedom to distinguish
   errors in solved pressure/operator from reconstruction and stress evaluation.
   Use manufactured/channel/flat-wall cases alongside obstacle force, momentum,
   energy and domain checks. Keep raw pressure and viscous components separate.
3. Make stationary-object CFD reusable through explicit SI geometry, boundary
   conditions, forcing and selected-object load contracts. Start with qualified
   aligned boxes and reject unsupported geometry rather than applying the cube
   verification template as if it solved arbitrary objects.
4. Add physical low-Re transient and advective/inertial transport with conservation,
   pressure/outlet consistency, real simulated time and temporal/spatial convergence.
   Known-answer tests must precede claims about wakes or general wind-tunnel behavior.
   The present steady Stokes lane does not contain those models.
5. Revisit performance, reusable factors, cache/recovery and broader size policy
   after the physical formulations and acceptance tests work. Resource accounting,
   cancellation and preservation of accepted state remain necessary throughout;
   wall-time speedups are not physical acceptance criteria.
6. Extend geometry and models deliberately: curved/rotated objects, moving-body
   boundary motion and geometric conservation, then separately justified turbulence,
   free-surface/interface and thermal/buoyancy models as needed. Each extension needs
   its own model scope, known-answer cases and convergence evidence.

Eleven support controls pass in this pass: five accuracy/resource/publication,
four physical comparison/observer, and two targeted geometry controls. Two solved
fields, one signed stress observation and four geometry observations are retained.
No native/default/package/installation/canonical/release adoption is claimed.
Evidence roots: `build/c3d-accuracy-first/` and `build/c3d-accuracy-side-layer/`.
