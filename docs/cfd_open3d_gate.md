# C3D-6 open straight duct: predeclared contract

Declared before C3D-6 numerical testing, 2026-09-29. Existing Main Edit only;
no commit, package, moving body, obstacle, wall transient, or arbitrary CFD claim.

## Equations and boundary meaning

Stationary incompressible Stokes: -mu*laplacian(u)+grad(p)=0, div(u)=0,
physical p in Pa. Uniform MAC face velocities and cell pressures. Integrated
continuity B shares every interior face, momentum gradient is -B^T. Each
component has its own staggered dual-volume diffusion operator, including the
half dual volume at the outlet-normal face. No periodic X wrap, imposed interior
pressure gradient, independently fixed outlet pressure, or projection shortcut.

X=0: prescribed fully developed continuous rectangular-duct inlet velocity;
zero transverse velocity. Inlet values are analytic face-area averages, so the
specified Q is recovered without grid-dependent profile rescaling.
Y/Z: stationary no-slip (normal faces fixed zero, tangential half-cell distances).
X=L: zero external vector-Laplacian traction mu*d(u)/dx-p*e_x=0. Its normal
component determines the pressure datum jointly with normal velocity diffusion;
tangential components specify d(v)/dx=d(w)/dx=0. Do not additionally pin p=0.
This is a fully developed do-nothing boundary for the vector-Laplacian form,
not zero full symmetric Cauchy traction. Actual symmetric-stress work and
physical strain dissipation must be evaluated separately. The distinction is
reported in diagnostics and cannot be hidden by an algebraic energy balance.

Solve H*u-B^T*p=f, B*u=g by a pressure Schur solve using cached SPD velocity
operators and Z-aware MG. Pressure is solved at every cell. Accepted state is
published only after independently checking the complete momentum/continuity
system. The open datum removes the constant pressure nullspace.

## Independent reference and declared gates

Continuous rectangular-duct Fourier series from the existing independently
verified reference helpers. Exact pressure p=G*(L-x), G=mu*Q/(H*W*mean_response).
Reference velocity, each of four wall shear integrals and power G*L*Q are derived
from the continuous series, not the native matrix. Pressure drop is measured
from solved cross-section pressure traces; it is never filled from reference G.

Fixed case L/H/W=4/2/2 m, rho=1 kg/m3, mu=0.1 Pa s, Q=0.008 m3/s.
Three grids 16x8x8, 32x16x16, 64x32x32. Finest limits:
- relative face-velocity L2, solved pressure-drop and inlet/outlet Q errors <=1%;
- each integrated wall shear and physical strain dissipation error <=2%;
- physical boundary work versus physical dissipation imbalance <=2%;
- complete scaled momentum residual <=1e-11, max divergence <1e-8 s^-1;
- flux conservation <=1e-10 relative; numerical allocations stay within budget.

Pressure inlet/outlet traces use second-order extrapolation of solved cell
cross-section means. Also report common upstream fitted gradient and outlet
pressure trace to expose boundary error. Report individual wall components.

Outlet-distance screen at fixed finest physical spacing: L=4,6,8 m,
Nx=64,96,128, Ny=Nz=32. Common upstream x in [1,3] m fitted pressure gradient
and integrated four-wall shear per unit length/physical dissipation per unit
length must change <=1% between 4/6 and 6/8. These are steady straight-duct
checks, not transient wake/outlet qualification. Include an unequal-spacing
rectangular case, pressure-datum covariance, viscosity scaling and failure cleanup.

If accuracy stalls or numerical residuals fail, diagnose boundary geometry,
pressure coupling and wall reconstruction before adding resolution. No gate
relaxation after observing results. Stop at this passing boundary slice.

## Ownership and efficiency

Reuse-adopted: app cfd_memory, Cartesian geometry, continuous duct reference,
Z-aware sparse MG and existing scene/session ownership. Reuse-deferred: a generic
shared core_math mixed-CFD abstraction; equations/boundary policy stay app-owned.
core_sim/core_scene semantics and shared module versions are unchanged.
The vector-Laplacian weak-form context is the official FEniCS Stokes demo:
https://docs.fenicsproject.org/dolfinx/main/python/demos/demo_stokes.html
(its pressure sign differs).

Cached Schur/Krylov workspace; report optimized setup/solve CPU and wall costs,
numerical peak and process RSS separately. The numerical budget excludes JSON/RSS.

Verified outcome: see [C3D-6 completion](cfd_open3d_completion.md). The original gates above are unchanged.
