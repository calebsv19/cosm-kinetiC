# Native manufactured steady-Stokes accuracy control

2026-10-04. This source-checkout control solves the unchanged native mixed cube
operator with independent physical forcing. Let phi(t)=256*t^4*(1-t)^4 on each
support interval, zero outside. Supports x=[.25,1.25],y/z=[.25,1.75] lie upstream
of the stationary cube and inside all walls. Define streamfunction
psi=.0001*phi_x*phi_y*phi_z, u=(partial_y psi,-partial_x psi,0), and
p=.001*phi_x*phi_y*phi_z. No-slip and open-end homogeneous data hold because all
fields and needed traces vanish outside the compact support. Pressure uses an
explicit physical zero at open ends; no mean subtraction or gauge fit.

The force density is -mu*Laplacian(u)+gradient(p),mu=.1, integrated analytically on
actual velocity dual volumes. No matrix action generates this RHS. Compare native
velocity with exact physical face-area averages and pressure with exact cell-volume
averages using volume-weighted L2 norms. The coarse n8 run is already recorded.
Predeclare n16/n32 within512MiB owned allocation/180s checkpoint limits each, original
momentum<=1e-11 and maximum divergence<1e-8. Report actual convergence ratios/order;
require errors to decrease at least50% per refinement, and n32 velocity<=.03 and
pressure<=.01 for this smooth-case diagnostic. These are new manufactured-case
criteria, not replacement thresholds for cube force/energy qualification.

Independent Gauss/Leibniz controls must check the analytic forcing factors through
third derivatives, including clipped support intervals. Protect native sources,
worker and old evidence. No native physical equation/default change, and this single
steady Stokes case does not qualify general objects or transient/inertial CFD.
