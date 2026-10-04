# Exact macro-cubic balanced velocity checkpoint

Five support proofs cover constant through cubic reproduction, shared edge/face
orientation, explicit sparse P4-evaluation left inverse T Z=I, boundary vanishing
and projected injectivity, exact Galerkin/coarse action, dense balanced SPD/
linearity/symmetry for component and two-block smoothing, original FE/RHS/pressure/
reconstruction and owned partial/100 cleanup. Initial support-stage missing import
and restricted-basis element indexing errors are fixed; failed logs remain.

| Original L4 smoother | Iterations | Wall seconds | Owned MiB | Full residual |
| --- | ---: | ---: | ---: | ---: |
| one component SGS sweep | 1000 | 53.893 | 543.469 | 9.60209e-11 |
| one two-block SGS sweep | 696 | 40.747 | 651.953 | 6.53241e-11 |

Both pass unchanged full numerical/resource/publication gates and requested
1e-10 target; same-mesh field/force/scalar equivalence passes. Physical raw/reaction
mismatch remains about 3.64358%. No physical mesh is promoted.

Exact admitted count6 base with two-block smoothing still hits 180 seconds,
sampled peak 1466.4375 MiB, last iteration 800 momentum 1.97093e-8 and continuity
8.52946e-11. No accepted field exists. Original equations, pressure modes and
resource gaps remain unchanged. Symbolic same-base diagnostics independently
match all actual factors: coarse 248767792, longitudinal 102079032, transverse
404235208 bytes, total 755082032. Maximum numeric workspace 18012852 bytes;
requirements exclude live input/FE/interpolation/symbolic/runtime allocations and
are not RSS predictions. This diagnostic publishes no numerical field.

Evidence: `build/c3d-cubic-coarse/checkpoint-audit.json`, five support proofs,
three numerical receipts, two accepted original fields and one structural cost
receipt. Reject larger balanced-path adoption/normal numeric admission. Native/
shared source, predecessors and protected workers remain preserved. No commit/
package/install/deploy/version change. Stage 1 and the full goal remain open.

Next: [additive coarse/local SPD cost control](cfd_3d_additive_coarse_goal.md).
