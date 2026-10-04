# Bounded two-block exact Cholesky sweep investigation

The original cube with three scalar exact principal factors and four fixed
symmetric sweeps uses 336 MiB but takes 2112 iterations / 175.8 s. Momentum stalls;
continuity is already much smaller. Reject that path for larger controls.

Compare contiguous component groupings (u,v)|w and u|(v,w), each with two exact
positive principal Cholesky factors. Retain every off-block coupling and both
symmetric directions. A fixed block SGS inverse and its fixed Richardson repeats
have the same majorization/SPD proof as the three-component sweep. Preserve the
complete mixed equations/RHS, local reconstruction, pressure modes/diagonals,
full FE residual, flux/divergence/energy, raw traction/reaction and resource gates.
No shift/floor/scaling, altered force, altered residual or relaxed cap.

Prove exact dense/anisotropic principal inverses, symmetry/linearity/positivity,
majorization, repeated action, original FE inputs/reconstruction and handle
cleanup. Start with original L4, both groupings, one sweep; allow fixed two sweeps
if measured residual/cost warrants them. Under 50000 tets / 1800 MiB / 180 s /
3000 iterations, retain all failures without accepted fields. Only useful accepted
original controls may extend to the admitted count6 base, then the exact stopped
23616-tet normal cube. Compare all accepted fields/forces to prior same-mesh truth.
Explicit existing chunk sizes may be compared with resource/cost evidence.

Record full setup, solve, reconstruction, diagnostic and publication costs, owned
and sampled RSS, principal factor bytes and unchanged input identities. An
accepted normal mesh permits the next force refinement test; numerical success
does not itself certify physical accuracy. No native/shared API, dependency,
version, commit, package, install or deploy change; generic extraction deferred.
Stage 1 and the full object/wind-tunnel goal remain open.
