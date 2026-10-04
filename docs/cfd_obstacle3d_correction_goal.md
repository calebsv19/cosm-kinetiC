# C3D-8 reference and body-edge correction: bounded experiment contract

Declared 2026-09-30 before new measurements. Preserve the original C3D-8 gates
and failed controls. Work only in Main Edit; no commit, package or install.
No change to 2D or C3D-1..7 numerical kernels or shared API/adoption state.
Reuse the existing scene/session, numerical allocator and velocity MG. CFD
geometry, reference and physical stress policy remain app-owned; generic
core_math extraction stays deferred.

1. Independently test the current no-slip wall/pressure and energy reconstruction
   on smooth known-answer fields and half-cell/corner geometry. Separate sample
   interpretation, integration and equation errors. Do not replace physical
   diagnostics by exact algebraic row reactions or rescale to force balance.
2. Reduce reference storage by assembling scalar P2 momentum and three P2/P1
   divergence blocks. This is the same continuous Taylor-Hood weak Stokes system,
   not a new reference model. Prove same-mesh pressure/forces/energy match the
   retained vector-basis implementation within 1e-7 relative.
3. Independently refine a plane-graded conforming tetrahedral mesh on L=8,
   cube center=4, unchanged no-slip/natural boundaries/fluid/Q. Include cube
   planes exactly. Body-interval subdivisions 6/8/10/12, graded transverse
   outer intervals 3/4/4/4 and long X intervals 5/6/6/6. Body intervals use
   cosine grading at both ends; fluid gaps use cubic grading toward the body.
   Preserve these meshes as a sequence, not post-hoc selected force values.
   Empty calibration uses continuous Fourier pressure/D, never native values.
4. Keep the 50000-tetrahedron preassembly cap, 1800 MiB own-process RSS cutoff
   and 180 s deadline per reference. Record cost and cap receipts. Reference
   acceptance still requires <=1% separate pressure/raw symmetric viscous force
   and dissipation changes, and <=1% integrated total versus weak reaction.
5. Derive a native traction/strain correction only from independent boundary
   or known-answer evidence. Preserve current reconstruction/row-reaction
   differences as diagnostics; do not tune toward reference drag. Test gauge,
   adjoint/SPD, cancellation/cache/cap/cleanup and all-field agent readback.
6. Rerun the original four grids and fixed-spacing L=4/6/8/inlet controls with
   original 5% separate force, 3% D/Pin/energy and 2% momentum gates. Report
   an unpassed gate rather than relaxing it. Uniform finest extended-domain
   refinement must obey the existing 262144-cell admission.

Stop before geometry variety, moving bodies, inertial wakes, turbulence, local
native 3D refinement, atmosphere/water, GPU, commit or packaging.

## Additional bounded probes declared after the first correction measurements

The scalar-block plane-graded sequence improves component convergence but its
physical surface versus weak-reaction discrepancy remains about 1.3%. Probe a
consistent grad-div term gamma*(div u,div v), first gamma=mu on meshes 8/10/12,
then gamma=10*mu on mesh 12 as a parameter-sensitivity control. This term vanishes
for the exact incompressible solution. Keep physical raw stress, unstabilized
weak reaction and added stabilization reaction separate. No result can qualify
by hiding stabilization reaction: reference acceptance still needs component
convergence and <=1% physical surface/weak reaction discrepancy; parameter
sensitivity remains explicit. Do not change native equations or fit gamma to drag.
Resource caps are unchanged. Stop and retain cap/true-residual failures.

A finest extended-domain native n=40 case (160 x 40 x 40 = 256000 cells) is
admitted under the existing 262144-cell policy. Add it after the original seven
cases to test the final L=8, center=4 reference directly; do not substitute it for
the original grid/distance controls or bypass the cap with n=48 on L=8.

The graded tetrahedral reference also exposes spurious transverse body loads on
an exactly reflection-symmetric benchmark. Test a conforming mesh assembled by
reflecting one octant across the three actual symmetry planes, merging shared
vertices, while preserving the same graded coordinates, cell/tetrahedron counts,
equations and caps. Run the unchanged graded 8/10/12 sequence with gamma=0.
This is a reference mesh-geometry control, not force symmetrization after solving;
retain unmirrored and grad-div controls and all raw XYZ force components.
