# L8 held normal end-slab quality control

Subdivide each end slab once (outer_layers2) with the accepted L8 count6 normal
inner X planes, Y/Z coordinates, body, Q=.008 m3/s and mu=.1 Pa s held. Preserve
inner tetrahedron geometry/connectivity up to vertex numbering; no filled body
cells or changes to physical domain/boundaries. Compare positive Jacobians,
31m3 fluid volume and actual Jacobian condition distribution before solve.

Use the unchanged P4/DG-P3 exact condensed/reference probe and coupled vector
caller-owned factor. Full original equations, pressure modes and reconstructed FE
residual remain authoritative. Independent symbolic/live stage guard precedes
allocation and repeats on a numerical attempt. Keep 50000 tet,1800MiB,180s,
3000iterations,1e-10 requested residual target and32MiB admission reserve.
A rejected stage has no numeric allocation or field, and is not a physical result.
No retries to obtain a fitting memory sample; retain rejected diagnostics.

Four controls cover actual inner geometry/boundaries/volume and fitting/rejected
allocation with mode/outer-layer forwarding. If admitted, compare accepted L8
outer1 anchor against outer2 using each pressure/viscous/reaction force and raw
surface/reaction <=1% plus scalar convergence and full numerical/resource gates.
If not admitted, record exact cost/shape evidence and select a bounded cheaper
fixed SPD preconditioner preserving all equations/modes/residual authority.
Native/desktop qualification and broad goal remain open.
