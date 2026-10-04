# Deterministic FE metadata control: numerical base pass, normal guard rejected

Five controls and installed Basis/affine API snapshots prove exact anisotropic/
refined coordinate/trace/mapping restoration, original owner release, complete
mixed action/exact inverse, nonzero RHS/full FE/free identity preservation, and
fail-closed retained owner/mesh mutation/manifest corruption/over-budget checks.
All metadata restoration completes before reconstruction and full residual.

Original/base pass 150/160 iterations, 16.064/140.458 s, 395.922/1272.063 MiB,
meet requested 1e-10 full target and numerical gates. Same-mesh fields/forces match
lossless-load predecessors. Base whole-process cost worsens; this is not adopted
as a general improvement. Normal detaches 23078360 bytes of unused coordinates,
trace inverse and affine caches. Symbolic-only estimate is 1860629240 bytes
(1774.434 MiB), but numerical live guard measures current RSS 474021888 and
predicts 1893692152 bytes (1805.965 MiB). It rejects before numeric factor
allocation at about 17 s. No normal field/force result exists.

Evidence: `build/c3d-factor-metadata/checkpoint-audit.json`, five controls, frozen
installed API provenance, two accepted fields and stage/live rejection receipts.
Caps/reserve/equations/pressure/full residual unchanged; older fields/checkpoints
and native workers preserved. Stage 1/full goal remain open. No native/shared
API/version/commit/package/install/deploy. Next include unused full FE DOF catalogs
in bounded deterministic suspension, require all original numbering/tables/scalars
bitwise after exact factor cleanup, and remeasure before guarded normal testing.
