# Tighter discarded-fill compensation at unchanged storage cost

The adaptive inner action was rejected for accuracy. This new candidate changes
only the local factor's compensation for discarded 3x3 fill blocks. For every
block U, ||U||2 <= min(||U||F, sqrt(||U||1 ||U||infinity)). Adding this bound times
I to both diagonals preserves the positive-semidefinite omitted-pair model. Use
scaled arithmetic, reject nonfinite inputs and allow conservative roundoff.
Compute the bound only for omitted blocks. No new heap allocation, graph, fill
quota, ordering, or physical matrix change is permitted. Preserve the entire
encoded operator/complete-factor prefix and original IC0 implementation.

Keep the distributed P3 space, Float coarse factor with three physical corrections,
fixed eight-step flexible velocity action, fixed linear pressure-column proxy,
ten pressure directions, scale 10, original physical residuals and all caps.
Fresh admissions reserve both restart-30 bases, original work and pressure scratch
before each numeric allocation. Retire all completed factor owners before full FE.

First test the norm bound against independent SVD and the 6x6 omitted-pair
positive-semidefinite model, including anisotropy, rotation and extreme finite
scales. Compare actual FE factors' graph, storage, inputs, pivots, positive work
and reconstructed factor inverse. Freeze exact source transformations and support.
One original 4,992-tetrahedron pressure-column diagnostic compares to the sealed
original Double control. Require BOTH fixed-proxy Schur W and projected K errors
<=0.90 of that control before a full flow trial. The original physical identity,
Float predictor, fixed pressure symmetry/positivity/reproduction, exact-Double
reference and resource/retirement gates remain mandatory. Do not alter this gate
or run a parameter sweep after seeing the result.

Only an eligible diagnostic permits one original full small flow trial. The full
FE residual must pass 1e-10, retained residual 1e-11, force/scalar equivalence 1e-7,
flux/divergence 1e-8, physical energy .03 and whole runtime <=23.5018023327 seconds.
All original 1800 MiB / 180 seconds / 3000 iterations / 50000 tetrahedra caps and
atomic field checks stay. A useful small field then permits matched 28,416-tet L4:
whole <=106.642085 seconds and complete velocity factor/work saving >=300 MiB
versus 873198472 bytes. Only a useful matched result permits the saved finer L4.
Native/default adoption and physical force certification require separate proof.
The persistent goal remains active.
