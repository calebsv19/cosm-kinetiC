# Complete vector storage checkpoint

Eight controls pass for complete bounded 3x3 node-pair storage, original bitwise
scalar coefficients (including tiny values and structural padding), upper row-major
block/lower column-major transpose convention, independent dense and anisotropic/
refined action/inverse, exact symbolic costs, mixed pressure/RHS/full FE preservation,
shared caller factor ownership, redundant scalar-owner release, partial/100 cleanup,
and diagnostic-only fitting/over-budget admission. Initial stage Python import-scope
failure and its corrected passing receipt are retained.

| Accepted control | Iterations | Wall seconds | Owned peak MiB | Full residual |
| --- | ---: | ---: | ---: | ---: |
| Original 4992-tet L4 | 150 | 15.927 | 381.000 | 1.97019e-11 |
| Count6 18816-tet L4 base | 160 | 121.915 | 1135.547 | 3.14604e-11 |

Both meet requested 1e-10 and unchanged full FE/divergence/flux/energy/resource/
publication gates. Original/base fields, component forces and scalars match the
prior caller-owned exact path. Base uses matched chunk size 128: owned RSS improves
16.0826%; measured total time rises 4.9293%. Adopt the optional measured exact vector
storage path on original/base. No scalar coefficients, mixed equations or pressure
modes are dropped; all diagonal and intercomponent directions remain.

The exact stopped 23616-tet normal has the same original mesh/free/RHS and all
scalar velocity/coupling/pressure hashes as the prior stage. Complete conversion
preserves the original mixed action. Vector factor storage is 1336344064 bytes,
scratch 49771768, measured current RSS 490029056 and predeclared reserve 33554432.
Estimated numeric stage is 1909699320 bytes (1821.231 MiB), above 1800. Numeric
launch remains withheld; symbolic handle is cleaned up and no field is published.
This estimate is not actual numeric RSS. API release report remains zero; current
RSS and owned high-water are separate. Normal budget improves but is not admitted.

Evidence: `build/c3d-vector-storage/checkpoint-audit.json`, eight tests, strict
SDK-backed C build, two accepted fields/receipts and symbolic-only normal stage.
Physical force/domain/component/raw-reaction/stress gates remain open (base raw/
reaction 2.17123%). Native/shared API/version/commit/package/install/deploy unchanged;
old checkpoints/fields and protected workers retained. Stage 1/full goal remain open.
Next losslessly retain sparse full inlet load, remove redundant dense load copies
during factorization, rematerialize bitwise after exact factor cleanup, and remeasure
normal stage before any numerical launch.
