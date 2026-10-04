# Current CFD physical-accuracy batch assessment

2026-10-04, PhysicsSim Main Edit. Accuracy before performance. The latest pass
retains a useful pressure reconstruction diagnostic and establishes a full native
steady-Stokes known-answer regression. It rejects a cube surface redistribution
that passes numerical checks but worsens physical force balance. The broad CFD
goal remains active and cube reference force qualification remains open.

## Completed useful evidence

The [native manufactured-solution series](cfd_3d_native_manufactured_checkpoint.md)
measures actual velocity and pressure field errors against independent analytic
solutions. Fine128x64x64 errors are0.4221%/0.1351%, with about second-order refinement.
Analytic forcing integral controls pass, and original native numerical equations,
pressure loads and strict residual/conservation checks remain. This smooth case is
a usable physical-accuracy regression, not qualification of singular cube forces.

The [four-interval pressure diagnostic](cfd_3d_four_interval_checkpoint.md) reduces
reconstruction discrepancy13.7–13.8% on both saved reference projections. At the
finer archived native grid, total pressure-force difference improves only8.329%
to8.028%, so native solved pressure still needs work. Five polynomial/layout/gauge/
limited-regularity/readback controls pass. The unchanged native pressure action is
also [calibrated independently](cfd_3d_native_pressure_gradient_checkpoint.md):
quadratic gradients are exact to roundoff, cubic dual-volume gradient bias is h^2/4,
and constant pressure supplies open-end traction rather than a null global mode.

The [graded stress identity](cfd_3d_graded_stress_checkpoint.md) closes to4.81e-14N.
About77–78% of signed drag gap is allocated to the innermost0.025m centroid edge band.
Scores are diagnostic rankings, not force-error bounds. Eight edge/normal geometry
controls preserve physical boundaries; only four pass the declared quality screen.

## Rejected trial and next true refinement

The edge0.04m redistribution L4 field completes637.955s/4640.266MiB owned, full FE
6.448e-12, flux1.737e-11, maximum divergence2.823e-10/s and energy imbalance1.128e-11.
Separate force/refinement and fixed-length scalar checks pass, but raw equilibrium
worsens1.4037%→1.4343%. The frozen work-selection gate therefore rejects it; no L8
counterpart is authorized by that gate. Sources/receipts remain sealed as evidence.
Its inherited triangle-preservation flag was inaccurate: surface triangle count
stays constant, but actual coordinates change. The checkpoint explicitly corrects
that metadata without altering fields or forces. Future local adapter metadata
correctly distinguishes unchanged physical planes from changed triangulation.

Genuine one-pass local edge refinement passes physical boundary/volume/area,
reflection/YZ topology, global shape/conditioning and120000tet size screens at both
radii0.025/0.04m. Radius0.04m uses88320/107136tet. Actual paired inner-cell connectivity
matches, with maximum translated coordinate difference8.88e-16m. The old12-digit
rounded-string check gave a false mismatch. Three controls now verify actual cells,
reject candidate-key collisions and detect genuine coordinate/connectivity changes
at the original1e-12 precision. Meshes and equations are unchanged by this diagnostic
correction. Body-relative remeshing was investigated and is not selected.

The next radius0.04m local L4 solve has been launched under the original guarded
8192MiB/1800s allowance, full1e-10/retained1e-11 and original physical gates. Its
result remains pending at this assessment checkpoint. Inspect its immutable receipt
and assess complete fields before any acceptance. Proceed to L8 only if accepted L4
reduces raw mismatch at least10% relative; original physical1% remains mandatory.

## Next major work

1. Finish the genuine local trial, preserve or reject it from complete physical
   results, and continue true spatial refinement to close raw force equilibrium.
2. Retain the native manufactured case as a regression and use known-answer
   pressure/operator/reconstruction controls to assess native accuracy changes.
   Require native force, energy, conservation and domain qualification for adoption.
3. After these stationary foundations, qualify reusable additional objects and
   explicit physical transient/inertial transport. Keep general-object, wake,
   turbulence, moving-body and other model extensions separately validated.

The [six-step wider direction](cfd_3d_graded_accuracy_assessment.md) remains; this
pass changes its measured immediate actions. No performance optimization, commit,
package, installation, memory write or native default promotion is part of this
checkpoint. Only current-truth/index preexisting docs are updated. Source/test proof
is separate from packaged/installed product proof.
