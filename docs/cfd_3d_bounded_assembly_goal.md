# Bounded exact assembly to admit the better body mesh

Next active object/wind-tunnel slice after the corner preflight checkpoint.
The six-interval body family improves the nearest-edge geometry, but its exact
18816-tet L4 base case stops during sparse factor setup at 2001.9 MiB observed RSS.
Original 1800-MiB, 180-s, 3000-iteration and all residual/conservation/force gates
remain authoritative. No accepted field exists for that case.

Reduce peak allocations without changing equations or pressure modes. The current
macro constructor materializes every local 103-by-103 entry in global COO arrays
(798475776 bytes for this body6 base), then creates full retained CSR, a free-row
slice and a free-column slice. Investigate bounded symbolic/numeric construction
directly on the free retained variables. Prefer existing exact local forms,
field reconstruction and original FE residual authority. Preserve every numerical
entry including the pressure diagonal and symmetric coupling. If a single-triangle
representation is needed, prove its symmetric action and factor input separately;
never silently drop rows, pressure modes, roundoff terms or introduce a penalty.

Freeze predecessor sources/fields/workers and the failed resource receipt. A new
reference-only runner owns new source/binary provenance. Before adoption, compare
operator action against original assembly on anisotropic and refined controls,
then a matched accepted body4 case: original mesh/free-DOF/RHS identity, force and
scalar equivalence <1e-7 relative, velocity/pressure differences <1e-8 / 1e-6.
Measure phase owned RSS and periodic RSS separately, allocation sizes, factor
storage, total time and all acceptance gates. Favor measured total cost over an
unverified theoretical memory saving. Retry the exact body6 case only after the
matched control demonstrates useful savings. Keep failures and publish no field
on numerical/resource rejection.

If admitted, compare matched count4/count6 base forces and independent stress
identities/defects before trying the finer normal case or L8. All separate 1%
raw-pressure/viscous/reaction/scalar and raw/reaction requirements remain. Native
traction correction, authored objects, transient/outlet/inertial wake and further
geometry/material qualification remain subsequent. Generic FE/factor/job extraction
is reuse-deferred; existing app reference/scene/acceptance conventions are reused.
No shared/native API, version, dependency, package, install, commit or deployment
change is implied.
