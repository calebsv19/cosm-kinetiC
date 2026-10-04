# Prospective signed-force-local geometry admission

Use the immutable accepted end2 field and signed cell/face observer. Sum all
signed Alfeld-child pressure+viscous drag terms within each parent, retain both
lifts, rank max magnitude and sum magnitudes over the eight symmetric orbits.
Consider the first eight Y/Z-paired macro sets with centroid edge distance below
.15m, using conforming bisection and existing all-edge-star subdivision separately.
These are diagnostic scores, not proven force error bounds.

Admission before PDE allocation requires positive mapping, volume31m3, true
boundaries with areas6/4/4/64m2, mirror symmetry, no extra wall, and unchanged body
shape/BCs. Compare the same removed parent geometry and replacement children
with label/scale-invariant all-edge/volume shape measure: affected worst shape
must not worsen (1e-8 rounding allowance), affected mean must improve, and global
Jacobian maximum must not worsen. Retain rejected geometry metadata; do not
relax a gate to admit a candidate. If a candidate passes, test independent
original FE action/reconstruction on conforming controls and stage-screen its
full Float factor/scratch/current/basis reserve before a full solve. Numerical
and force/component/raw/scalar/resource gates remain unchanged.
