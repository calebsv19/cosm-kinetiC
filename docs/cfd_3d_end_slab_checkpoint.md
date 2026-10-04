# End-slab shape improves; exact factor budget rejects allocation

L8 held-normal outer2 uses 28416 tetrahedra versus accepted outer1's23616.
The18816 inner tetrahedra retain geometry/connectivity up to vertex numbering
and~1e-15m reflection/reconstruction rounding. Body surface, Y/Z coordinates,
inner X planes, domain/flow/boundaries and31m3 fluid volume are preserved.
Four controls pass. Initial strict body-coordinate equality control caught harmless
rounding; the corrected canonical geometry control uses1e-13m resolution, retains
that initial failure and checks actual inner/body shape, not just counts.

Maximum tetrahedron Jacobian condition improves543.299→271.723. This is geometric
proof only: there is no new numerical field, force result or physical certificate.
Exact symbolic/current-residency stage estimate2363295495bytes (2253.813MiB)
includes1745502112-byte factor,58443623-byte scratch,525795328-byte current RSS
and32MiB reserve. It exceeds unchanged1800MiB; numerical launch is withheld.
Symbolic cleanup and every original mixed input/action remain verified.
Stage completes21.389s, sampled1249.266MiB, owned high-water1366.547MiB.
API released-byte report remains0; current and high-water retain separate authority.

Retain accepted L4/L8 anchors, originalP4/DG-P3 equations, pressure modes,
condensation/reconstruction, requested1e-10 full residual and all numerical/
force/resource/publication gates. No cap relaxation or retry to get fitting RSS.
Predecessor evidence and protected native workers remain intact.

Next test the deferred nodal-vanishing quartic complement Y=(I-ZT)E with
TZ=I and bijective[Z,Y], exact congruent block factors and fixed SPD sweeps.
The rejected unit complement stalled momentum on the original cube; this basis
change targets that failure while preserving the original mixed operator. Prove
rank/nodal vanishing/congruence/SPD/fullFE/cleanup first, original cube next, and
extend only on useful accepted measured results. If cost or convergence fails,
retain the rejection and select another bounded resource/force refinement route.

Evidence: build/c3d-end-slab/checkpoint-audit.json, support-test-receipt.json,
frozen stage-runs. Goal: docs/cfd_3d_end_slab_goal.md. Native/shared source/APIs,
version/package/install/commit are unchanged by this slice. Stage1 and broad goal
remain open.
