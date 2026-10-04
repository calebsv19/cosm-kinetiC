# Full native steady-Stokes known-answer convergence

2026-10-04, PhysicsSim Main Edit. Four resolutions solve the unchanged native
mixed cube operator with analytically integrated physical forcing, independently
of native matrix actions. The compact streamfunction and pressure in the
[predeclared case](cfd_3d_native_manufactured_goal.md) supply exact divergence-free
velocity, zero traces near cube/walls/open ends and a fixed physical pressure zero.
No pressure gauge fit, reference drag fit or matrix-manufactured RHS is used.

| Grid | Relative velocity error | Relative pressure error |
|---|---:|---:|
|16x8x8|34.3487%|8.1430%|
|32x16x16|7.3061%|2.1488%|
|64x32x32|1.7066%|0.5399%|
|128x64x64|0.4221%|0.1351%|

Velocity is compared with exact physical face-area averages and pressure with
exact cell-volume means, using volume-weighted L2 norms. Observed final orders
are about2.02/2.00. The fine case passes predeclared velocity1%/pressure0.3% targets
and error-ratio0.4; original momentum1e-11 and divergence1e-8 remain. Fine momentum
residual2.709e-14, maximum divergence1.241e-14/s,140.412s and474.960MiB owned
allocation fit the explicitly declared1024MiB/600s fine-case allowance.

Independent six-point Gauss and unexpanded Leibniz product derivatives validate
84 actual C forcing-factor integral samples through third derivatives, including
clipped and narrow support-endpoint intervals. Maximum absolute difference is
7.97e-10. Native adjoint/SPD, exact cleanup and full residual checks remain.
No native physical/default changes. The first three cases and forcing controls
are sealed in `build/c3d-native-manufactured-stokes/checkpoint-audit.json`; the fine
case/cap-only transform is sealed separately in
`build/c3d-native-manufactured-stokes-fine/checkpoint-audit.json`.

This is a retained full-field steady-Stokes accuracy regression case. It checks
smooth forcing in the fluid and the native cube-domain boundary treatment, with
exact fields zero near cube edges. It does not qualify the singular cube-force
case, arbitrary geometry, transient/inertial flow, wakes or turbulence. Native
pressure-gradient and force-reconstruction controls remain separate diagnostics.
The existing cube reference raw surface/reaction1% criterion remains open.

Source-local probe commands are documented by the sealed compiler/source records.
The C fixtures and `scripts/verify_cfd_native_manufactured_stokes.py` remain available
for regression checks. This development fixture is separate from public first-start
headless proofs, default solver adoption and packaged desktop acceptance.
