# Exact two-block Cholesky sweep checkpoint

Six support tests prove both dense groupings' exact principal inverses, fixed
repeat symmetry/linearity/positivity/majorization, refined anisotropic FE inputs,
original nonzero RHS/reconstruction/pressure diagonals and owned cleanup.

| Original L4 control | Iterations | Wall seconds | Owned peak MiB | Full residual | Accepted |
| --- | ---: | ---: | ---: | ---: | --- |
| (u,v) / w, one sweep | 3000 | 83.718 | 472.219 | 2.61943e-7 | no |
| u / (v,w), one sweep | 2027 | 52.603 | 508.953 | 5.80968e-10 | yes |
| u / (v,w), two sweeps | 1381 | 63.856 | 479.422 | 2.21901e-10 | yes |

Both accepted fields pass unchanged full FE/divergence/flux/energy/resource and
publication gates and match the prior exact coupled factor fields/forces. Both
miss the requested 1e-10 full-residual target but pass the unchanged 1e-8 numerical
gate; library success is not the authority. Raw/reaction mismatch remains 3.64358%.

The cheaper one-sweep grouping extends to the exact admitted 18816-tet count6
base with existing chunk size 512. It stops at the unchanged 180-second cap,
sampled peak 1197.375 MiB, last iteration 1900: retained momentum 2.43139e-5,
continuity 1.99895e-8. There is no accepted refined field. All original mesh/free
DOF/RHS identities match. Reject this path for larger numerical/force testing:
lower factor memory alone does not solve global momentum convergence.

Immutable evidence: `build/c3d-block-cholesky/checkpoint-audit.json`, six support
proofs, four frozen numerical receipts and two accepted original fields. Prior
source/evidence/native workers remain intact. No native/shared API/version/commit/
package/install/deploy change. Physical accuracy and Stage 1 remain open.

The next [bounded global coarse velocity correction](cfd_3d_coarse_velocity_goal.md)
adds an exact nested macro-linear coarse inverse to the coupled local sweeps.
