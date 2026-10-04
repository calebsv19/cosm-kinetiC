# Higher-order cube reference: verified support, force gate still open

2026-10-03, PhysicsSim Main Edit. Force convergence testing is operational again
with the sparse velocity factor and exact DG pressure mass preconditioner. This
continuation tests P4 velocity/DG-P3 pressure on the same Alfeld geometry and
continuous Stokes equations. Polynomial support and empty-duct accuracy improve,
but the cube raw-traction gate still fails. The [predeclared scope](cfd_3d_quartic_goal.md)
and broader object/wind-tunnel goal remain active. Stage 1 is not complete.

## Implemented and checked

The reference lane adds a 35-node continuous tetrahedral P4 element with sorted
edge/face orientation and 20 pressure nodes per discontinuous P3 tetrahedron.
Bounded-cell assembly, the implicit mixed operator, the existing scalar velocity
factor, cubic pressure mass, raw pressure/viscous traction and weak reactions
remain separate. There is no force smoothing, pressure penalty, equation change,
weak-force substitution or relaxed residual/resource gate.

Seven support tests check nodal values, gradients, shared-face continuity for
random coefficients, quartic reproduction, cubic divergence span, pressure mass
scaling, volume/boundary quadrature and orientation rejection. Local macro bubble
coupling has 79 nonconstant pressure modes out of 80 on each of six affine test
macros; this support check is not a whole-cube condition bound. Twenty-four solved
manufactured Stokes cases cover quartic velocity/cubic pressure, three axes, two
meshes, two pressure offsets, Dirichlet and natural traction boundaries. Maximum
velocity error is 3.79e-13, pressure error 9.53e-11, pressure traction error
9.15e-11, viscous traction error 7.59e-15 and true residual 1.41e-11.

A library rule requested as tetrahedral order 6 does not integrate degree-6
products exactly (maximum tested monomial error 1.09e-5), producing a rank-14
cubic pressure mass with 20 unknowns. The lane verifies the order-7 rule against
all degree-6 monomials and rejects a singular/under-integrated mass. Boundary
power and manufactured natural-traction products require degree 7; the order-8
triangle rule is independently checked. Initial failing sources/logs are retained,
including a separate manufactured-fixture full-volume assembly correction.
Neither an under-integrated mass nor an under-integrated natural load is used in
the retained cube runs. Raw cube traction agrees between face orders 4 and 8.

## Measurements under original caps

Caps remain 50,000 tets, 1800 MiB observed/own RSS, 180 seconds and 3000 iterations.
Successful children retain immutable source bundles, supervisors, receipts,
fields, raw forces, both residual blocks and geometry/matrix/RHS identities.

| L4 cube control | Tets | Iterations | Wall s | Observed RSS MiB | Pressure N | Viscous N | Reaction N | Raw/reaction mismatch |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Original P4/DG-P3 | 4,992 | 610 | 32.02 | 832.41 | .025061751 | .013771233 | .040301398 | 3.6436% |
| First-normal split | 6,720 | 590 | 49.83 | 1098.80 | .023900760 | .013811180 | .040252074 | 6.3106% |
| Body resolution 4 | 10,752 | 580 | 77.51 | 1511.45 | .024818506 | .013953147 | .039973636 | 3.0069% |
| Original, tighter solve | 4,992 | 773 | 37.65 | 1017.84 | same to <2e-9 relative | same to <2e-9 relative | same to <2e-9 relative | 3.6436% |

Normal subdivision changes pressure by 4.6325%, viscous force by .2901% and
reaction by .1224%. Body resolution changes pressure by .9706%, viscous force by
1.3210% and reaction by .8133%; pressure/dissipation scalar changes are .5563%/
.5593%. Every pair fails the unchanged 1% component/raw-reaction gate. Higher
polynomial order is not adopted as a qualified cube force reference.

The ordinary original-cube residual is 8.73e-11; the tighter control reaches
3.89e-12. MINRES stops internally before the requested 1e-12 callback target,
which is explicitly not attained. The measured force changes are below 2e-9
relative, orders of magnitude smaller than the spatial differences.

Empty L4 and L8 ducts pass the independent Fourier-series calibration. Pressure
relative errors are .0011028%/.0011067%; dissipation errors .0013508%/.0012307%.
Pressure error is about eleven times smaller than the previous cubic empty-duct
control. This verifies smooth-flow support, not cube stress convergence.

The 13,824-tet body4 normal split is stopped at observed 1900.97 MiB after 11.70 s.
A smaller 128-cell assembly batch also fails: observed 1927.91 MiB after 17.59 s.
The second run already reports own assembly peak above the cap. Both failures
retain logs and receipts without an accepted field or physical force result.
Smaller batches alone are not an established memory improvement. RSS is sampled;
reported peaks include the observed overshoot that triggered termination.

Maximum affine Jacobian condition is 215.37 for all tested cube meshes. Median
condition changes from 19.37 to 23.70 for the original normal split. These are
geometric diagnostics, not full mixed-system condition numbers.

## Next bounded improvement and remaining integration

The linear solve no longer prevents force testing; the remaining reference
problem combines stress approximation, mesh conditioning and memory at the next
refinement level. The next experiment should use an independently checked stress
observer for the quartic field and a graded conforming mesh that reduces long
outer-flow cells while preserving symmetry, cube geometry and true boundaries.
The previous cubic observer found large volume stress defects outside the corner
bands; blindly refining only body edges or selecting signed force cancellation
would not be justified. Check full pressure rank/mode coverage on each changed
mesh rather than interpreting this small support test as an inf-sup certificate.

Use a matched original-geometry control to prove any assembly/storage or static
condensation improvement before spending its memory savings on a larger force
pair. Keep the failed quartic normal pair and its original caps. Qualify separate
raw pressure/viscous forces, reaction consistency and scalars on L4, then L8;
continue tighter-residual controls when needed. Only a qualified independent
reference can support native pressure/traction correction, authored stationary
objects and subsequent transient/outlet wind-tunnel qualification. General
objects, moving bodies, curvature and inertial wakes remain later gates.

Run `make test-cfd-reference3d-quartic` and `make audit-cfd-3d-quartic` in Main Edit.
Evidence is in `build/c3d-quartic/checkpoint-audit.json`, `focused-tests.log`,
`rejected-intorder6-source/`, and immutable `runs/<source-digest>/` children.
Native sources/headers, predecessor fields, workers and audit files are preserved.
No commit, package, installation, canonical adoption or accuracy certification is
implied by this source-only checkpoint.
