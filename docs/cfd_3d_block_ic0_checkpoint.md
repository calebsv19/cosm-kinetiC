# Fixed-pattern IC0 investigation: rejected before a flow solve

Five independent controls passed: dense full-pattern factor and coordinate order,
sparse factor-model positive definiteness and compensation, balanced correction
formula/coarse reproduction/complement retention, anisotropic original FE/nonzero
load reconstruction, and admission/invalid input/lifecycle/physical-C-prefix checks.
The anisotropic inverse was extremely large; positive factor pivots alone did not
establish numerical usefulness. Symmetry is checked relative to its scale.

The one declared original L4/body2/4992-tetrahedron full control terminated after
4.001s, sampled314.281MiB. Fresh projected admission306.582MiB passed, including
both actual Arnoldi bases/work, pressure and velocity coarse construction, factor,
solve scratch and32MiB reserve. The balanced velocity coarse-reproduction guard
failed during construction. No outer solve, reconstructed residual, force estimate,
or published field exists. This is a numerical setup rejection, not time or memory
failure. Do not advance to the larger control or finer mesh or retry unchanged.

Complete original matrix/RHS/mesh hashes, exact physical conversion/encoding and
allocator-preservation guards passed before rejection. All original physical
coefficients, pressure modes and residual/resource limits remain authoritative.
No native/default/package/install adoption. Persistent goal remains active.

Next distinct hypothesis: compensate every omitted factor-fill Schur update with
positive diagonal blocks bounding its Frobenius norm, instead of repairing only
failed late pivots. These changes belong solely to the preconditioner. Require
independent model-error positivity, moderate inverse growth, physical preservation,
coarse reproduction and full strict small control before any larger trial.
