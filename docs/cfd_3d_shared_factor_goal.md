# Shared velocity-triangle input for the exact reference factor

Declared after triangle storage proves the exact 18816-tet body6 base with 8.04%
less owned peak memory, but the 23616-tet normal case stops at 1889.8 MiB during
factor setup after 18.03 s. The 1800-MiB cap and all numerical/physical gates remain
unchanged. The full object/wind-tunnel goal and Stage 1 physical qualification are
still open. Force testing is operational on admitted reference meshes.

Separate the explicit upper mixed storage into its complete velocity triangle,
velocity/pressure rectangle and pressure triangle. Preserve every coefficient,
including tiny pressure diagonals, and use the exact symmetric block action.
Share the contiguous float64/int32 velocity triangle arrays with the unchanged
existing C Cholesky shim, rather than allocating duplicate factor-input rows and
values. Copy only the required int64 column pointer array. Keep input owners alive
through factor cleanup and prove that factor creation/solve leave them unchanged.
No coordinate scaling, penalty, pressure removal, field modification or relaxed
residual/resource gate is authorized. This remains optional macOS reference work;
no native/shared API/version/dependency/package/install/release/deploy change.

Before runtime adoption, independently prove block action on anisotropic/refined
FE controls, original full-equation reconstruction, exact pressure diagonals,
zero-copy array sharing, input preservation, dense/FE inverses, invalid data and
owned lifecycle cleanup. Then compare accepted body6 base full fields/forces,
original mesh/free/RHS identity and measured total resource cost with the triangle
predecessor. If measured headroom supports it, retry the exact stopped normal mesh
with matching mesh/free/RHS identity, unchanged 50000-tet/1800-MiB/180-s/3000-iteration
caps and atomic accepted-field publication. Retain failures without a field.

If admitted, compare separate pressure/viscous/reaction/scalar refinement and raw
surface/reaction gates, plus independent stress identities and volume/jump defects.
Continue matched near-body L4/L8 normal controls where demonstrated resources allow.
Physical mesh promotion requires the existing independent criteria; numerical
admission alone is insufficient. Native traction correction, authored stationary
objects, transient/outlet/inertial wake and broader geometry/material work follow
reference qualification. Generic FE/factor/local-job extraction is reuse-deferred.
