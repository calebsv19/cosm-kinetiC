# Selective outer cube control: solved, measured, rejected for accuracy

2026-10-03. Persistent Main Edit, optional local P4/DG-P3 reference. The stronger
exact condensed solver admits a 14208-tet selective outer refinement under all
original numerical and resource gates. Independent stress evidence rejects this
mesh as an accuracy improvement. Force-convergence testing remains operational;
physical qualification and the broader object/wind-tunnel goal remain open.

The preceding controlled-domain checkpoint found that old L4-to-L8 grading also
changed near-body spacing. Holding those planes gives L8 normal-refinement
pressure/viscous/reaction changes below 1%, but raw/reaction mismatch remains
2.102%. Matched held L4-to-L8 pressure/reaction changes still exceed 1%. A whole
outer plane exceeds the original memory cap and publishes no accepted field.
See [controlled domain measurements and resource-policy proofs](cfd_3d_domain_checkpoint.md).

The selective helper binds the exact accepted held-L4 L8 normal solver receipt,
field, result and independent stress observation, including all artifact/source
hashes. It rebuilds the parent tensor/octant geometry, folds eight mirror orbits,
selects the highest-scored long outer macros, performs conforming bisection and
then sorted Alfeld splitting. Near-body axes and actual body facets are preserved.
This is a diagnostic mesh family, not the default or a certified force-error bound.
Recomputing barycenters introduces up to 2.22e-16 summation roundoff; saved input
hashes remain exact, reconstruction uses a 1e-14 geometry comparison and centroid
matching is bijective within 1e-11. Initial construction rejections are retained.

Four focused tests independently prove genuine boundaries and areas, volume,
mirror symmetry, unchanged body geometry, all Alfeld centers, paired Y/Z marks,
tampered-observation rejection and explicit original FE quadrature action after
exact condensation on a refined anisotropic control. All pass. The geometry
survey records 14016/14208/14400/15168 tetrahedra for 1/2/4/8 marks per octant.
The smallest Y/Z-paired candidate uses two marks; conformity adds twelve parents
per octant, or 384 tetrahedra globally. Its original indicator capture is 11.48%.

| Measurement | Held L8 normal base | Selective paired outer candidate |
| --- | ---: | ---: |
| Tetrahedra | 13824 | 14208 |
| Iterations | 290 | 330 |
| Total solver job time | 69.77 s | 75.85 s |
| Owned peak RSS | 1574.0 MiB | 1546.5 MiB |
| Periodically sampled peak RSS | 1574.0 MiB | 1279.6 MiB |
| Jacobian condition maximum | 543.30 | 720.18 |
| Strong volume-equilibrium defect | 0.27592 | 0.39789 |
| Interior stress-jump norm | 0.03288 | 0.04226 |
| Raw surface / reaction mismatch | 2.102% | 2.118% |

Do not infer robust memory savings from these two runs: factor storage increases
679184848 to 697951720 bytes, timing and resident samples vary, and periodic
sampling understates the candidate's owned peak. The original 1800-MiB limit
is checked by both supervision and owned phase measurements. Original full
residual is 9.18e-11; momentum and continuity blocks are 9.17e-11 / 2.20e-12.
Volume divergence is 3.12e-11, flux error 2.21e-12 and energy imbalance 2.32e-12.
All numerical gates pass and its numerically accepted field is retained. It is
not an accepted physical-accuracy field.

Raw pressure/viscous/reaction changes are 0.138% / 0.0145% / 0.0779%, and inlet
pressure/dissipation changes are 0.0477% / 0.0669%. These small changes alone
cannot qualify the force: the independent raw/reaction gap slightly worsens.
The observer verifies component identities to 2.18e-14 N, but volume and jump
defects rise 44.2% / 28.5%, and worst mesh conditioning rises 32.6%. The weighted
indicator also rises. Alfeld splitting after parent bisection does not guarantee
a nested approximation or monotonic stress error. Larger versions of this outer
candidate are not run, because the smallest paired control fails its intended
accuracy improvement. No failed accuracy control becomes a physical default.

The reproducible entry points are `scripts/run_cfd_reference3d_selective.py` and
`scripts/run_cfd_reference3d_selective_equilibrium.py`. A solver invocation supplies
`--diagnostic <immutable held-normal-stress.json> --marks 2`; each runner freezes
its source, optional factor binary/build identity and original caps. Use
`make test-cfd-reference3d-selective` for support proofs and
`make audit-cfd-3d-selective` for source/binary/field, acceptance and observer
verification. The audit is `build/c3d-selective/checkpoint-audit.json`. It preserves
all predecessor receipts, native workers and source files. No native/shared API,
version, dependency, commit, package, install, release or deployment changed.
Generic FE/factor/job extraction remains reuse-deferred.

The next bounded gate is a body/corner control whose macro geometry improves
shape before expensive factoring. First measure where signed component stress
loads contribute to the raw/reaction gap, since a long-cell weighted indicator
alone did not select a useful force refinement. Prove conformity, pressure-mode
retention and original full action, then solve within the same caps and compare
raw pressure, viscous, reaction, scalar response and independent stress defects.
If useful, repeat the identical near-body mesh on L4/L8 and close all separate
1% force/scalar and raw/reaction requirements. Native traction correction and
authored stationary-object qualification come afterward; transient/outlet/inertial
wake, curved/moving/free-surface and atmosphere work remain subsequent stages.
