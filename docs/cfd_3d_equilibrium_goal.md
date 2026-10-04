# Cube pressure-trace equilibrium investigation

2026-10-03. Continue the active object/wind-tunnel CFD goal from the verified
spatial checkpoint. Diagnose the remaining raw-surface/reaction error using
existing immutable fields before selecting another refinement. Preserve all
native sources, reference equations, pressure, traction, physical gates and caps.

For each scalar body lift eta (one on the cube, zero at outer boundaries), verify
by elementwise integration by parts that raw body force minus weak lift equals
interior stress-jump load minus weighted volume stress divergence. Separate
pressure and symmetric viscous contributions; their weak parts depend on the
lift and are diagnostics, not alternative physical force components.

Verify cubic Hessians independently on non-axis-aligned tetrahedra and verify the
full identity on arbitrary continuous cubic velocity and discontinuous quadratic
pressure, including non-solenoidal fields. Smooth global polynomial stress must
have zero interior jump. Include deliberately discontinuous pressure. Preserve
fluid outward-normal signs on both sides of interior facets. Use polynomial-exact
volume/facet quadrature and bounded cell/facet batches. Compare the observer's raw
force and weak lift with predecessor measurements before interpreting defects.

First inspect body6 base, first-normal split and targeted-edge fields, then the
original cube if useful. Attribute signed loads and unsigned defects to whole
cell/facet centroid distance buckets near cube edges; these are sampling regions,
not geometrically clipped bands or rigorous error bounds. Keep snapshots read-only
and bind every diagnostic to its input receipt/snapshot and frozen observer source.
No observer may mark a physical field qualified or repair it.

Observer children retain 50000-tet, 1800-MiB and 180-s resource caps. They do not
solve a new system, so the original solver residual/iteration receipts remain the
authority for their inputs. Use the defect evidence to choose one physical mesh
or discretization improvement and test whether it is actually useful under the
unchanged <=1% separate force/scalar gates on L4/L8 before native reference use.
The persistent goal remains active; no commit/package/install is authorized.

Observed physical continuation: unsigned mesh-scaled volume scores are dominated
by coarse outer-flow cells, and the edge-only candidate decreases defect norms
while worsening raw/reaction mismatch. Try one score-guided macro refinement of
the body6 base field. Reconstruct the predecessor geometry exactly from its
structured axes, group scores by all eight reflection orbits, mark the top 32
macros in one octant, refine conformingly and mirror before Alfeld splitting.
Preflight is 27264 tets and captures 51.53% of the score. Use the unchanged working
factor/mass solver, residual/force observers and all caps. Scores are not force-error
bounds; accept the physical change only from separate raw-force/reaction/scalar
convergence evidence. Preserve failed or unhelpful outcomes.

The reference probe gains only an optional explicit mesh-data argument for reuse
of the same solver/observer on the adaptive geometry. Its existing default mesh
path must reproduce the prior exact-cube matrix and fields; all form, element,
preconditioner, traction and consistency sources remain unchanged. Record this
small orchestration change separately from unchanged native source/headers.
