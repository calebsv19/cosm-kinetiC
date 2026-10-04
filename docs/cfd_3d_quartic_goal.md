# Higher-order cube force reference investigation

2026-10-03. Continue the active object/wind-tunnel goal from the equilibrium
checkpoint. The P3/DG-P2 normal, edge and score-guided mesh changes do not pass the
original raw pressure/viscous/reaction force gates. Test P4 continuous velocity and
DG-P3 pressure on the same conforming Alfeld geometry before further mesh sweeps.
The continuous Stokes equations, no-slip cube/walls, natural ends, fixed flow,
raw Cauchy traction, separate component/reaction gates and all caps stay intact.

Use sorted tetrahedral vertices for both multi-DOF edge and face orientation.
Require nodal/partition/gradient support, exact quartic reproduction on shared
faces including random nodal fields, and divergence contained in the cubic DG
pressure span. Independently verify pressure mass scaling against library mass
assembly. Known polynomial Stokes solutions must recover quartic velocity/cubic
pressure and raw wall traction with Dirichlet and natural-boundary cases across
three axis orientations and pressure offsets. Existing P3 sources/fields remain
unchanged. Numerical reference code remains app-owned; no shared API/version or
native solver change is implied.

Then calibrate empty ducts against the independent Fourier result before testing
the original 4992-tet cube and genuine first-normal subdivision. Test actual tighter
residual stability if helpful, and report any callback target not attained. Keep
failed, resource-stopped and unhelpful controls. Use bounded-cell assembly and the
already tested implicit saddle operator, sparse scalar-velocity factor, and exact
DG pressure mass inverse with cubic block size. No stress/pressure smoothing or
projection, matrix regularization, pressure penalty, or weak-force replacement.

Caps are unchanged: 50000 tets, 1800 MiB own/observed RSS, 180 s, 3000 iterations;
true residual <1e-8 and ordinary early target <=1e-10. Physical qualification still
requires <=1% separate pressure/viscous/reaction/scalar refinement changes and raw
surface/reaction mismatch on L4 and L8. Support/calibration or one converged cube
alone cannot complete stage 1 or the persistent goal. Choose the next physical
improvement from measured force error and resource cost, then return to native
pressure/traction, authored stationary objects and transient/outlet wind-tunnel
qualification. No commit/package/install is authorized.
