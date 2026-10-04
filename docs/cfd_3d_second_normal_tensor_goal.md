# Same-surface tensor remesh of the two closest normal slabs

The nested original-macro cut is geometry-correct but requires41216tet; block3
Float estimated2360MiB, and scalar ordering stopped at1833MiB owned peak during
symbolic setup. Neither produced a numerical field. Preserve both failures and
all original partition/quality evidence. Do not weaken the completed nested-cut
experiment's original-parent requirement or its unchanged1800MiB cap.

Declare a distinct non-nested macro remesh: retain every cosine count6 axis/body/
end plane and actual cube surface triangle; add one X midpoint in the closest
front/back fluid slabs. Tensor bricks partition their original bricks, while new
tetrahedral diagonals cross some old tetrahedra. Prove that crossing explicitly;
claim no old-tetrahedron containment or nested FE space. Verify exact physical
volume, positive mapping, no false boundaries, facet matching, reflection symmetry,
original axes retained and no worse global intrinsic shape/Jacobian conditioning.
Test complete independent P4/DG-P3 quadrature/action, nonzero-load elimination and
reconstruction on a representative normal brick. Retain stress indicators as
observations, not force bounds. Geometry alone is not physical acceptance.

Use unchanged coupled Float/block3 factor, original full float64 mixed operator,
pressure modes, load, restart60 flexible iteration, chunk512, full1e-10 and
1800MiB/180s/3000iterations/50000tet caps. Screen before numeric allocation; if
admitted, solve and compare pressure/rawviscous/reaction/scalars against the
accepted cosine count6 normal. Run independent original stress and signed
traction identities only on a complete accepted field. All separate1% force,
raw/reaction, flux/divergence/energy and publication gates remain. Retain an
informative diagnostic field without physical adoption if any physical gate
fails. Extend to matched L8 end2 only if the force evidence is useful. Native and
general-object qualification remain later requirements.
