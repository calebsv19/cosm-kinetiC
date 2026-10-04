# L8 sensitivity and further bounded stress resolution

Continue the active object/wind-tunnel goal from the Cholesky admission checkpoint.
Reuse the exact condensed solver, adopted optional factor and original full field,
traction, energy, residual and resource gates. No native/public/shared API change.

Run matched body4 base/first-normal controls on L8. Compare raw pressure and viscous
loads, independent reaction and scalar response separately; preserve the separate
1% raw/reaction criterion. Compare with L4 at matched resolution, acknowledging
that the original outer grading changes its first-normal spacing with length.
This is a measured domain/mesh sensitivity check, not a certified force-error bound.

Use independent stress identities and measured defects to select a further bounded
body/normal/outer control. Preserve conforming sorted Alfeld topology, body geometry,
full reconstructed pressure modes, numerical gates and 50000-tet/1800-MiB/180-s/
3000-iteration caps. Retain unsuccessful candidates and publish no accepted field
on failure. Mesh/probe additions must have independent geometry and algebra proofs.

Keep predecessor sources, fields, binaries and audits immutable. Source/binary
provenance and numerical/physical acceptance remain separate. Stage 1 and the broad
goal stay open until all L4/L8 raw force/scalar and raw/reaction requirements pass.
Native corrections, authored objects, transient/outlet/inertial wake and further
geometry/material qualification remain subsequent work. Generic FE/core_math and
core_jobs extraction stays reuse-deferred; existing FE/scene/acceptance semantics
are reused. No commit/package/install/deployment or dependency install is implied.
