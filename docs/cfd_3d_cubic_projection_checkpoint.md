# Cubic reference pressure projection and native trace diagnosis

The new diagnostic integrates the actual discontinuous cubic pressure from an
accepted P4/DG-P3 field over clipped Cartesian fluid slabs. It preserves each
parent element's pressure coefficients, triangulates clipped convex pieces and
uses degree-three-exact tetrahedral quadrature. It then applies the current native
three-interval pressure trace weights (11,-7,2)/6. It replaces neither the raw
reference pressure force nor any native solver field.

Three independent controls pass: all20 polynomial moments through total degree3
on an anisotropic nonaligned clipped box; discontinuous per-element constant
pressures; and quadratic-exact interval reconstruction, gauge response and its
known cubic truncation bias. These establish the diagnostic's tested mathematical
scope, not arbitrary float64 clipping/conditioning bounds or obstacle accuracy.

Both trials read the same source-bound numerically accepted81792tet graded L4 field,
verify original full residual, flux/divergence, energy, geometry/pressure-DOF identity,
finite coefficients and source/artifact hashes, and preserve all input artifacts.
The saved projection snapshots contain diagnostic averages, not new flow fields.

| Cartesian resolution | Raw reference pressure force N | Native stencil on exact reference volume averages N | Relative functional difference |
|---|---:|---:|---:|
| n16 |0.0247851997703|0.0233668354054|5.7226%|
| n32 |0.0247851997703|0.0239023539493|3.5620%|

Whole observer times6.846/3.789s and sampled281.063/449.063MiB fit the declared
1024MiB/180s diagnostic contract. Raw reference pressure and viscous loads remain
separate. The reference itself still fails1% total raw/reaction agreement; these
projection values do not remove that uncertainty or certify physical pressure.

The native stencil has a measurable reconstruction bias even when supplied exact
volume averages of this reference pressure. This does not measure error in the
native solved pressure. A complete attribution also needs a source-bound native
field with matched units/geometry/boundary conditions, comparing native and exact
projected pressures under the same functional. A trace functional is not a complete
pressure-field error norm. No native stencil/operator/default is changed or adopted.

Next, continue reference stress convergence while using this calibrated diagnostic
to evaluate bounded native reconstruction candidates and independent manufactured
pressure/channel tests. Preserve open-boundary, momentum, energy, separate force
and domain criteria. Evidence: `build/c3d-cubic-pressure-projection/` holds frozen
sources, test proof, two successful source-bound receipts/diagnostic snapshots and
once-only audit. Native binaries/audits remain protected; the broad CFD goal is open.
