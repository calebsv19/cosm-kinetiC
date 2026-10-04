# C3D-8A/B/C: predeclared stationary 3D obstacle gate

2026-09-30, before new measurements. Complete only A+B+C, preserve existing
2D and C3D-1..7 numerical kernels and fixtures. No commit/package/Desktop install.

## A: geometry and independent answer

Stationary aligned cube, edge 1 m, center (2,1,1) m in a 4x2x2 m duct.
No-slip body and Y/Z walls; both X ends have natural vector-Laplacian traction
mu du/dn - p n = -P n. Pout=0; unknown Pin is determined by Q=.008 m3/s.
Solve one unit-pressure response and eliminate the scalar Q constraint; prescribed
Q alone is not accuracy evidence. rho=1 kg/m3, mu=.1 Pa s; creeping Stokes equations
(no inertial transport). Report actual characteristic/body-local Reynolds numbers;
reject a scene above the declared creeping regime (Re based on mean/edge <=.1).

Independent conforming tetrahedral Taylor-Hood P2/P1 finite-element solver uses
continuous weak Stokes forms and the identical geometry, walls, tractions and Q.
Calibrate first on the empty rectangular duct against its continuous Fourier
pressure and dissipation reference. Refine independently, including cube edges;
separate pressure, symmetric viscous and reaction forces. Require last reference
changes <=1% for each nonzero X-force component and dissipation, plus reaction
versus integrated total force <=1%. Otherwise reference remains unresolved.
Library method: https://scikit-fem.readthedocs.io/en/stable/listofexamples.html#example-32-block-diagonally-preconditioned-stokes-solver
No free-space sphere or mismatched 2D force reference is admissible.

## B: native boundary, diagnostics and agent usability

Compact fluid cells and staggered face unknowns; eliminate all stationary normal
body faces, half-distance no-slip tangential diffusion, natural half-dual-volume
X-end faces. Integrated divergence B and -B^T pressure coupling; true momentum
<=1e-11 and max cell divergence <1e-8 s^-1. Reuse owned budget/MG/scene session;
keep masked topology/momentum app-owned, defer generic core_math extraction.
No shared module API/version/adoption change. Preserve earlier numerical sources
byte-for-byte; extend adapters additively with an immutable obstacle template.

Integrate all six closed body faces. Report pressure and full symmetric viscous
force separately (3 components), signed side contributions and closed-area vector.
Physical traction is separate from discrete row reaction and their discrepancies.
Report outer-wall forces, fluxes, physical strain dissipation, natural and physical
boundary power, discrete diffusion, momentum closure, upstream/downstream probes,
pressure deficit and a reproducible centerline wake/recovery profile. Stokes
recovery is not a turbulent separated wake claim. No copied reference inside solver.

Candidate solve/force observations publish only on acceptance; cancellation and
failed setup preserve accepted fields. Enforce exact numerical cap/one-byte-below,
cleanup, cached repeated solves, non-mutating samples and digest-bound full fields.
Pa/XYZ/solid mask/force/energy/cost available through existing MCP/session contract.
Numerical success, reference accuracy, refinement and boundary independence remain
separate typed statuses; incomplete comparison cannot become physical certification.

## C: quantitative acceptance

Native uniform cross-section grids initially n=8/16/32, Nx=2n (body aligns exactly).
If finest fails, add n=48 or finer within the admitted cell budget without relaxing
gates. Preserve failed coarse controls. Require three declared grids with decreasing
pressure/viscous/total X-force and dissipation errors; no assumed smooth second-order
stress convergence at sharp reentrant fluid edges. Finest separate pressure,
viscous and total body X-force errors <=5%, dissipation <=3%, required Pin <=3%.
Physical momentum closure <=2% of applied axial traction; physical boundary-energy
imbalance <=3%; discrete energy relative balance <=1e-9; flux imbalance <=1e-9.
YZ body forces <=1e-7 of total X force (symmetric geometry).

At fixed fine spacing/geometry/Q, extend downstream L=4/6/8; require final two
extensions <=1% changes in each body X-force component, Pin minus added empty-duct
pressure loss, and fixed physical recovery probes. Test inlet extension separately
by shifting the body upstream distance from 2 to 4 m while retaining the same
sufficient downstream distance. If distance gates fail, extend once or record an
unpassed boundary gate; never count real finite-domain changes as solver error.
Independent reference must use the accepted extended domain for final force proof.

Native operator adjoint/symmetry/constant-pressure closed-force tests, sanitizer,
memory/cancel tests, actual agent run/sample/assess/compare/export readback, and
preserved 2D/periodic/steady-open/transient source/regression evidence are required.
Record mesh/unknown count, residual/iterations, numerical peak versus process RSS,
solve/observation/publication/export cost. Stop before moving bodies, curved/STL
geometry, turbulence, local 3D refinement, atmosphere or water.
