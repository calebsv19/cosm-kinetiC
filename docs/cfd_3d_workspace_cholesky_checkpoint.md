# Exact caller-owned Cholesky checkpoint

Eight support proofs (six factor plus two stage controls) establish exact dense/
anisotropic/refined inverse, agreement of symbolic/actual factor bytes, caller-owned
aligned storage and scratch disposal, shared input/RHS/pressure/full FE preservation,
live symbolic pressure safety, partial/100 cleanup, and diagnostic-only fitting/
over-budget admission without numeric allocation. Installed SDK header/API/build
receipts bind the staged overload and ownership. All original equations remain.

| Accepted control | Iterations | Wall seconds | Owned peak MiB | Full residual |
| --- | ---: | ---: | ---: | ---: |
| Original 4992-tet L4 | 150 | 16.198 | 443.484 | 1.56121e-11 |
| Count6 18816-tet L4 base | 160 | 116.188 | 1353.172 | 3.15287e-11 |

Both pass unchanged full FE/divergence/flux/energy/resource/publication gates and
requested 1e-10 target, and match prior exact fields/forces/scalars. Base uses the
same chunk size 128 as prior shared-factor control; memory improves about 8.72%,
time increases about 3.10%. Original comparator predates shared storage, so its
memory change is not attributable solely to caller workspace control. Adopt the
new optional exact path only on measured original/base meshes; physical component/
domain/raw-reaction/stress gates remain open (base raw/reaction about 2.17123%).

Exact stopped 23616-tet normal is measured through symbolic/free-page stages and
cleaned up without numeric factor allocation or field publication. Its original
mesh/free/RHS identity matches previous stopped runs. After the pressure control,
current RSS is 619364352 bytes, exact factor 1325505336, scratch 26931968 and
predeclared reserve 33554432. Estimated numeric stage is 2005356088 bytes
(about 1912.46 MiB), above unchanged 1800 MiB. This is a conservative admission
estimate, not actual numeric RSS. Reject numeric launch; no new normal force
result exists. API reports zero released bytes; observed RSS changes and owned
high-water remain separate measurements. No peak counter is reset.

Evidence: `build/c3d-workspace-cholesky/checkpoint-audit.json`, eight support proofs,
two accepted numerical receipts/fields and one symbolic-only normal-stage receipt.
Predecessor/source/native workers preserved; no native/shared API/version/commit/
package/install/deploy change. Stage 1 and full object/wind-tunnel goal remain open.
Next: [complete shared three-component block representation](cfd_3d_vector_storage_goal.md)
to reduce operator/ordering index storage while preserving exact coupled Cholesky.
