# Exact symmetric storage for finer normal/domain controls

Continue the full object/wind-tunnel goal after bounded CSR assembly admits the
18816-tet finer-body base at 1745.8 MiB owned RSS, only about 54 MiB below the
original 1800-MiB cap. Raw/reaction mismatch improves but remains 2.171%, pressure
force changes 1.056% and global stress defects worsen. No physical mesh promotion.
The next needed comparison is finer-body normal resolution and matched L4/L8;
more operator/factor headroom is required before attempting those larger cases.

Investigate a single symmetric triangle of the reduced mixed operator, with
explicit T*x + T.T*x - diag(T)*x action. Preserve all velocity/pressure coupling,
macro pressure diagonals (including tiny roundoff entries), exact reconstruction,
original full FE residual and raw traction/energy diagnostics. Validate triangular
structure, sorted indices, finite values, symmetry/action and the positive velocity
prefix separately. Reuse the existing C factor library; an explicit new triangle
adapter must make representation assumptions visible, never pretend a triangular
CSR is a full matrix or silently drop pressure terms. Keep all original resource
and numerical/physical gates.

Prove full-action equivalence on anisotropic/refined controls and pressure-diagonal
preservation, then compare the accepted bounded body4 and body6 fields/forces,
recording allocation/owned RSS/sampled RSS, factor storage and time. Only adopt
where measured total cost and proof justify it; the original small-cube runner
remains available. If headroom is demonstrated, attempt the exact body6 normal
case, compare all separate force/scalar/raw-reaction gates and independent stress
identities/defects, then extend the identical near-body mesh to L8. Reject and
retain failures. Stage 1 and the broad goal remain open until physical requirements
pass; native correction/authored objects/transient/outlet/inertial wake and further
materials/geometry remain subsequent work. Shared FE/factor/local-job extraction
is reuse-deferred. No native/shared API, version, dependency, commit, package,
install, release or deployment changes are implied.
