# Lossless full-load residency checkpoint

Five new controls plus the sealed eight vector controls prove lossless indexed
full FE load (tiny values/signed zeros/zero and dense loads), original dense hash,
reduced RHS, full reconstruction/action, dense owner release, immutable indexed
storage and fitting/over-budget diagnostic cleanup. Dense full RHS is restored
bitwise after exact factor release; original free/RHS identity and scale are
rechecked before independent full FE residual. No equation/load/pressure change.

Original/base pass 150/160 iterations, 16.118/122.218 s, 387.859/1119.453 MiB and
full residual 1.56408e-11/3.15139e-11. Same-mesh fields/forces/scalars match vector
predecessors. Base matched chunk128 saves 1.4173% owned RSS with .2487% more total
time; original peak increases, so no general memory improvement is inferred.
Retain the optional lossless load path for its exact reduced factor residency.

The exact 23616-tet normal symbolic-only stage predicts 1802072824 bytes
(1718.591 MiB): current RSS 382402560, exact factor 1336344064, scratch 49771768,
unchanged reserve 33554432. Prior diagnostic admits an attempt. The numerical
process's live guard then measures current RSS 485933056 and predicts 1905603320
bytes (1817.325 MiB), exceeding unchanged 1800 cap. It stops before numeric factor
allocation at 17.374 s, retains its explicit phase rejection, and publishes no
field. The stage estimate is not actual completed-run RSS, and identical stored
arrays do not guarantee identical allocator/Python residency. Do not select only
the fitting stage as proof of robust admission.

Evidence: `build/c3d-sparse-load/checkpoint-audit.json`, five support proofs, two
accepted fields and stage/guarded rejection receipts. All older evidence/native
workers unchanged. Force/domain/raw-reaction/stress qualification, Stage 1 and
broad goal remain open. No native/shared API/version/commit/package/install/deploy.
Next: verify controlled collection of unreachable Python objects before the
allocator pressure call, hash all live inputs/action, preserve owned high-water,
and require completed-run gates after any live budget admission.
