# Corner-focused fixed-cost physical refinement control

Continue the full object/wind-tunnel goal from the rejected selective outer control.
First trace signed pressure/viscous jump-minus-volume contributions on immutable
accepted fields. Centroid-distance buckets guide testing and are not clipped
physical bands or certified force-error bounds. No raw force replacement is allowed.

Test a P4/DG-P3 mesh control which redistributes existing interior axis nodes toward
the cube edges, keeping macro connectivity, unknown count, outer/normal planes,
body shape and resource caps unchanged. The four interior intervals use first
edge distances .1, .08 or .0625 m; .1 is the first moderate candidate. Compare
Alfeld Jacobian conditioning on the corner macros before expensive assembly,
prove affine macro centers, conformity, mirror symmetry, volume/area and original
FE quadrature action, and reject geometrically unsuitable controls before solving.
Node redistribution trades central face spacing for edge spacing; it is not
uniform refinement or a nested approximation and improvement must be measured.

Solve the original equations with exact condensation and the optional Cholesky
backend. Full residual, divergence, flux, energy, independent raw pressure,
viscous and reaction loads and 50000-tet/1800-MiB/180-s/3000-iteration caps stay
unchanged. Independently observe stress identities and defects; retain failures.
A finer edge spacing warrants execution only after the first admitted candidate
shows useful force/stress evidence. If useful, compare a matched L4/L8 control.
All separate 1% raw-force/scalar and raw/reaction requirements remain open until
proven. Native corrections/authored stationary objects and transient/inertial
wake follow that gate; curved/moving/free-surface/atmosphere remain subsequent.
Existing FE/quadrature/scene/acceptance conventions are reused; generic FE/factor
and local jobs stay reuse-deferred. No native/shared API, version, dependency,
commit, package, install, release or deployment changes are implied.

The fixed-cost geometry survey rejected all six L4/L8 redistribution controls:
corner maximum and mean Jacobian conditioning worsen. No PDE solve is justified
for those controls. Keep that gate and evidence intact. Continue with a local
conforming bisection control selected among edge-near macro pairs. Before solving,
require genuine boundaries, body shape, fixed background planes and lower maximum
conditioning across the affected macro region; global worst conditioning may not
worsen beyond numerical geometry roundoff. Survey geometry first, preserve all
rejections, and select the nearest-edge admissible pair rather than tuning to
force results. Use paired Y/Z marking and all eight mirror symmetries. This
geometry-guided selection is experimental; all original physical gates remain.

All 63 single longest-edge paired controls worsen worst shape on their affected
region, including a vertex-label/scale-invariant all-edge-length/volume measure.
Keep those rejections. Next test a deterministic multi-edge stellar subdivision:
split the six original edges of each marked macro across every incident tetrahedron,
using shared midpoints and longest-edge-first order. This adds conformity closures
but avoids a single isolated cut. Preserve the same prospective shape and true
boundary gates and prove volume partition/shared facets independently before
using the new mesh in the original equations. No force results have been used
to tune this geometry selection.

Both isolated and multi-edge local families reject all 63 paired cases by their
prospective geometry gates. Stop that branch. The existing count6 tensor family
has better nearest-edge cell shape (worst inverse shape 6.077 versus 9.338 for the
count4 normal control). Try its smaller 18816-tet L4 base case with the current
stronger solver and unchanged gates; compare with the matched count4 base, since
this case does not split the first-normal interval. A resource failure is retained
and motivates bounded reduced assembly, not a higher cap. The exact current
all-entry COO scratch requirement is 798475776 bytes for count6 base, excluding
CSR conversion, geometry, fields and factors; this is an allocation-size calculation,
not measured RSS or admission proof. Native/shared code remains unchanged.
