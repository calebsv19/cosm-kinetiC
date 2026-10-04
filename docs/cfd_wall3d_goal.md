# C3D-7A/B/C: predeclared transient wall and outlet contract

2026-09-29, declared before new numerical measurements. Full active objective:
A known-answer wall transient; B conservative transport; C pressure-driven channel
startup and transient open-boundary qualification. Preserve C3D-6, periodic XYZ
and 2D proofs; no commit/package, obstacle, turbulence or moving body.

## Coupling, placement and cost

Uniform staggered MAC faces/cell Pa pressure. Integrated continuity B; momentum
pressure term -B^T. Solve rho*alpha/dt*M*u + H_mu*u - B^T*p = f, B*u=g jointly,
BE startup then constant-dt BDF2. Report complete true momentum/continuity residuals.
Periodic-X/wall-YZ pressure gauge is zero mean; natural X tractions set open datum.
Component dual volumes differ at outlet/inlet normal faces. Cache matrices,
Krylov, multigrid and references; one BE-to-BDF2 mass transition permitted.
Cancellation at Krylov checkpoints must preserve the last accepted fields/time.

Reuse-adopted: cfd_memory, Cartesian geometry, Z-aware sparse MG, existing scene
session/runtime cadence, authored immutable identities and artifact contracts.
Reuse-deferred: generic core_math mixed-CFD extraction and core_jobs integration;
the numerical equations/boundary policies and worker-control checkpoint adapter
stay app-owned. No shared API/version/adoption change. A new app mixed kernel is
shared by A/B/C; the accepted older solver paths stay intact.

## A: known answer with walls

Box 2x2.5x3 m, periodic X, no-slip Y/Z, rho=1, mu=.1 SI. Define
psi=sin(2pi*x/Lx)*sin(pi*y/Ly)^2*sin(pi*z/Lz)^2 and vector potential
A=(.011,.007,.013)*psi m2/s. u(t)=[1+.2*sin(2pi*t)] curl(A).
All three components and derivatives vary through XYZ; div(u)=0 analytically,
and velocity is zero on Y/Z walls because each envelope and its first derivative
vanishes there. Face-area averages are analytical separable integrals.

p=.01*cos(2pi*t+.3)*cos(2pi*x/Lx+.2)*
  [sin(pi*y/Ly+.3)+.5*cos(2pi*y/Ly)]*
  [cos(pi*z/Lz+.4)+.4*sin(2pi*z/Lz)] Pa (zero mean in X).
Its wall-normal derivative is nonzero in general: no artificial pressure-Neumann
constraint may be imposed. Derive f=rho*u_t-mu*laplacian(u)+grad(p) from continuous
functions, independently of the native matrix. Compare pressure after gauge removal.
Pressure RMS errors use the fixed nonzero .01 Pa reference peak scale, so a pressure zero crossing does not produce an undefined relative error.
Use independently evaluated physical derivatives/strain and signed component wall
shear RMS errors, normalized by nonzero wall-reference RMS (avoid cancelling loads).

Matched t=.4 s. Spatial n=8/16/32 each axis, dt=.0025 s initially; if time contamination
is measurable, lower dt consistently on all grids, without changing accuracy gates.
Separate time refinement on fixed n=16: dt=.04/.02/.01/.005/.0025, matched t=.4.
Measure self differences of velocity AND pressure to isolate the spatial floor.
Expected order screen >=1.8 for successive asymptotic spatial/time differences.
Finest velocity/pressure relative RMS <=3%, each wall-shear RMS <=5%, physical
strain dissipation error <=5%. This is a stronger-gradient wall test, not the steady
C3D-6 tolerance inherited by assertion. Full true momentum <=1e-11,
max div<1e-8 s^-1. Physical energy budget imbalance <=5% of a nonzero reference
power scale. Energy rate, forcing power, dissipation and discrete work stay separate.

## B: conservative transport

Same known flow/pressure and box; continuous f adds rho*(u dot grad)u. Native
transport shares normal flux at every component dual-face. For div-free velocity
and impermeable walls, central conservative transport has negligible domain
kinetic work; integrated component transport includes its legitimate wall fluxes.
No density-only advection or independent cell flux surrogate. BE/BDF2 time and
explicit extrapolated transport; enforce actual global component-flux CFL <=.25.
Same spatial/time/residual/physical gates as A. Expose amplitude projection error,
spatial orthogonal error, time-series harmonic phase/amplitude errors separately;
matched sine/cosine least-squares history over one period. Phase finest <=1 degree
and harmonic amplitude relative error <=3%. Measure pressure and shear independently.
No solver tolerance reduction may conceal transport error.

## C: physical startup and open transient

Straight 4x2x2 duct, rho=1, mu=.1. Start velocity at rest and apply constant natural
pressure traction Pin=G*L, Pout=0, where G is from the continuous Q=.008 steady duct
reference. No manufactured body forcing. Both X ends are open; all X-normal face
velocities are unknown, with half dual volumes at each end; transverse X derivatives
are natural. This is pressure-driven startup, not prescribed interior pressure.

Independent continuous solution: rectangular sine modes n,m odd with
lambda=(n*pi/H)^2+(m*pi/W)^2, coefficient
c_nm=16*G/(mu*pi^2*n*m*lambda),
u=sum c_nm*(1-exp(-nu*lambda*t))*sin(n*pi*y/H)*sin(m*pi*z/W).
Evaluate decaying corrections to the existing continuous steady Fourier reference
for stable late-time and wall-tail accuracy. Independently derive Q, individual
wall shear, kinetic E, dE/dt, dissipation and boundary power G*L*Q.
Check reference tail sensitivity. Verify spatial and timestep convergence, pressure,
Q/shear/energy at multiple early/intermediate times, and approach to C3D-6's steady
reference by t=12 s (lowest-mode decay exp(-nu*lambda_min*12)<.003). Declared
startup acceptance times are t=.5,2,12 s; the initial impulsive boundary layer
is reported separately and is not certified before it is resolved.
Finest transient Q/velocity/pressure <=1%, each integrated wall shear/dissipation
<=2%, energy budget imbalance <=2% of reference input power. True residual/div
as A. Outlet lengths L=4/6/8 at fixed spacing, compare same physical startup time;
Q, shear/length, physical energy/length must change <=1%.

Additionally exercise nonuniform three-component unsteady Stokes with open X ends:
reuse analytic curl/pressure family with fixed physical X wavelength; apply
mu*du/dn-p*n traction derived continuously at each end, not pinned cell pressure.
Verify spatial/time/reference and physical boundary work, retaining upstream
physical forcing on L=4/6/8 extensions. These manufactured natural tractions
qualify the declared unsteady mixed boundary implementation, not arbitrary wake
exit/backflow stability or an unforced open Navier-Stokes outflow.

### Nonuniform open-transient numerical matrix

Before the complete open-transient runs: L/H/W=4/2/2, fixed X wavelength 4 m;
Y/Z envelopes span their 2 m walls. Spatial cross sections 8/16/32 with Nx=2*Ny,
dt=.005, matched t=.4 s. Separate n=16 time levels .04/.02/.01/.005/.0025.
Use A's <=3% velocity/peak-normalized pressure, <=5% individual wall-shear RMS,
physical dissipation and physical power imbalance gates, asymptotic orders >=1.8.
Natural tractions are continuous mu*du/dn-p*n, with both end-normal faces unknown.
The X-normal momentum body source integrates its actual half slab; no reference
velocity or pressure is imposed at those ends. Reference E/D use independent
closed-form separable integrals, also checked against global physical quadrature.
Cell strain uses cubic one-sided derivatives near real boundaries and fourth-order
centred cross derivatives elsewhere, retaining face-area averaging and its measured
second-order physical reconstruction error. A quadratic reconstruction test and
exact-face dissipation probe distinguish observation error from solver error.
Reference physical boundary work is zero for these end planes; reconstructed
symmetric stress work is measured and included, never assigned zero numerically.
L=4/6/8 at fixed dx=dy=dz=.0625 and the same dt/t: compare solved three-component
velocity and absolute Pa pressure on common x=[1,3] to <=1% (pressure scale .01 Pa),
and kinetic/dissipation per length to <=1%. This controlled natural-traction test
is distinct from the pressure-only startup and arbitrary nonlinear wake outflow.

## Completion audit

Require A/B/C numerical and agent evidence, actual diagnostics/cost/cleanup and
non-mutating samples, safe cancellation, BE/BDF2 workspace reuse, strict failed
solve rejection and exported physical field readback. Preserve the existing 2D,
periodic and C3D-6 regression gates. Never replace one required delivery with an
extra steady channel. Each stage can remain in progress across goal turns; complete
only with evidence for all three. Stop before obstacles/local refinement.
