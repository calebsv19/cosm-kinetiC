# Bounded 3D CFD implementation contract

Declared before numerical testing, 2026-09-29. Work stays in the existing Main Edit;
no desktop release, commit, obstacle, turbulence, GPU, or adaptive 3D mesh is implied.

## C3D-1: compatible operators

Uniform Cartesian staggered faces, cell pressure, independent dx/dy/dz and SI units.
Each interior face has one velocity/flux shared by both cells. Periodic divergence
and gradient are negative adjoints under volume weighting. Pressure has a zero-mean
gauge. Tests include anisotropic dimensions, axis permutations, conservation,
adjoint identity, constant/null modes, and independently sampled gradients.
Tolerance for algebraic identities is 1e-12 relative (absolute floor 1).
Numerical allocations use cfd_memory; failed setup must clean up fully.

## C3D-2: steady rectangular duct

Prescribed volume flow Q, periodic X with an unknown pressure jump; no-slip in both
Y and Z. The flow constraint determines G=-dp/dx, rather than supplying G and calling
it a recovered pressure. Physical pressure is p(x)=G*(Lx-x), outlet gauge zero.
This is a fully developed duct boundary condition, not an open inlet/outlet proof.
The full Cartesian diffusion operator retains X/Y/Z terms; the reference solution
is invariant in X and has two nonzero cross-section gradients. The duct alone does
not establish arbitrary three-component pressure/velocity coupling.

Independent continuous reference: separation of variables for -laplacian(u)=G/mu,
zero velocity at y=0,H,z=0,W. Odd Fourier sine modes in y use a stable exponential
form of 1-cosh(k*(z-W/2))/cosh(k*W/2). The exact mean response per G/mu is
H^2/12 * [1 - 192*H/(pi^5*W)*sum_odd tanh(n*pi*W/(2H))/n^5].
Integrated individual wall shear comes from differentiating that series, not from
our native stencil. Reference implementation and tail sensitivity are tested.
Context: https://doi.org/10.1016/0735-1933(94)90046-9

Fixed case: Lx=4,H=2,W=2 m, rho=1 kg/m3, mu=0.1 Pa s, mean=0.002 m/s.
Three grids: 16x8x8, 32x16x16, 64x32x32. Also a non-square, unequal-spacing case.
Finest gates: relative velocity L2, pressure drop and volume flow <=1%; integrated
shear on each of four walls and independently reconstructed physical dissipation
<=2%. Numerical true residual <=1e-11; divergence <1e-8 s^-1.
Report measured errors/order; no pass inferred from process exit or a prescribed Q.

## C3D-3: fully 3D transient

Periodic Cartesian manufactured Navier-Stokes with nonzero u/v/w and variation in
all three axes, continuous forcing, physical pressure, conservative face transport,
implicit viscosity, constant-dt BE startup/BDF2. Separate spatial/time refinement.
Expected second-order trends, true residual and divergence gates as above; expose
energy terms separately rather than disguising discrete dissipation as physical
strain. Reject excessive CFL. Pressure splitting is exact only for the periodic,
constant-coefficient commuting operators; it does not qualify wall/open splitting.

## C3D-4: agent integration

New distinct model/template through the existing immutable scene/local session
worker. Budget admission, async run controls, XY/XZ/YZ slices, world probes, Pa,
artifacts, matched comparisons and scoped assessment. Sampling must not advance
simulation. Preserve all completed 2D interfaces and numerical behavior.

## C3D-5: cost and closing audit

Optimized builds, reusable Krylov workspaces and Z-aware multilevel preconditioning;
record phase CPU/wall, exact numerical allocation peak and process RSS separately.
Report matched-accuracy grids and cost; include budget rejection/cleanup, repeat-step
allocation checks, sanitizer and existing 2D regressions. Completion needs evidence
for each step, not merely a duct pass.

## Shared ownership

PhysicsSim owns CFD equations and boundary/reference policy. Reuse the existing
app numerical allocator, sparse MG and scene/session ownership contracts. Extend MG
with a backward-compatible Z-aware constructor. core_sim owns cadence, core_scene
owns scene meaning; no shared API/version change. Generic core_math extraction is
deferred until a second solver consumer establishes a stable reusable requirement.

C3D-6 continuation is separately predeclared in [cfd_open3d_gate.md](cfd_open3d_gate.md) and verified in [cfd_open3d_completion.md](cfd_open3d_completion.md). It does not alter these periodic baseline gates.

C3D-8A/B/C is separately predeclared in [cfd_obstacle3d_goal.md](cfd_obstacle3d_goal.md).
Its [implemented but unqualified checkpoint](cfd_obstacle3d_checkpoint.md) retains
the unpassed reference/physical gates and does not alter these earlier baselines.
