# Iterative spatial improvement toward useful object CFD

2026-10-02. Persistent user goal: improve and test physical 3D CFD, integrate useful
changes, and work toward reusable objects and wind-tunnel diagnostics. This slice
continues the reference pressure/corner gate; it does not redefine the full goal
as reference-only work. Existing Main Edit and unrelated dirty work are preserved.

First retain the body4 and genuine-normal controls on the unchanged predecessor
reference. Then isolate front/back normal spacing at fixed topology/body resolution.
Use an affine-per-parent axis warp of the conforming Alfeld mesh, preserving domain,
body, natural boundaries, P3/DG-P2 equations and all caps. Confirm positive cell
volumes, correct Alfeld centers, wall/body planes and exact legacy behavior when
no spacing is requested. Compare raw pressure/viscous loads, reaction, dissipation,
flux and divergence; do not replace the surface force with a weak load.

Caps remain 50000 tets, 1800 MiB observed own RSS, 180 s and 3000 iterations per
child; true residual <1e-8, ordinary early target <=1e-10. Reference robustness
requires <=1% separate component and scalar changes and surface/reaction mismatch
on both L4 and L8. Each next candidate follows observed error and measured cost.
If more resolution cannot fit, improve memory use with same-operator/field controls
before raising any resource contract. Keep failed controls and frozen sources.

After reference qualification, return to native pressure/traction improvements and
an authored stationary-object contract, then qualify transient/outlet behavior.
Useful integration requires actual scene/session/field/assessment readback, not
just reference-script success. No commit or package/install/release is authorized.
Existing shared cfd_memory/mixed/checkpoint/core_scene ownership is reused; numerical
reference policies remain app-local, with no shared API/version changes.


Observed continuation (2026-10-03): inserted normal planes preserve the outer
mesh; the initial axis warp is not adopted. Strong pressure trace sensitivity
persists, and the first larger child exceeds the RSS cap before assembly.
Same-form cell batches, streamed logical-matrix hashing, compact reaction weights
and an implicit saddle operator must pass matched field/force controls before
using their measured memory savings for the larger case. Record both observed
process RSS and own peak RSS where available.

Check every local macro pressure bubble rank as well as global macro constants
on the exact original cube. Try at most two mode-informed pressure actions with
SPD/scaling/known-solution controls and exact operator identities; retain mass if
cost or convergence worsens. Do not alter physical pressure, boundary conditions,
raw traction, or tolerances to make a preconditioner comparison pass.

Then preflight one conforming macro refinement near cube edges, followed by the
same Alfeld split. Keep all natural/body boundary planes and reject cracks or
false walls. Use one pass within 0.04 m, chosen from mesh/DOF preflight, under the
original 180 s/1800 MiB/3000-iteration caps. Measure separate raw components,
weak reaction, energy and divergence. Any finite-element force reference remains
unqualified until genuine refinements pass <=1% on both L4 and L8. The next
continuation follows the measured pressure-trace or resource defect; the broader
object/native/transient goal stays active.
