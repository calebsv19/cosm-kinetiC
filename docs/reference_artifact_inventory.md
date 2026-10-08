# Reference artifact inventory admission

All 194 identified reference wrappers declare the artifact paths owned by their
run, using the same tuple their fresh receipt hashes. Cached receipt admission
now requires exactly the declared paths which currently exist. It holds omitted
existing artifacts, unrelated in-run artifact substitutions and unexpected entries
before hash reads. Declared paths must remain within the selected run, without
parent traversal or duplicates. Existing linked/special leaves cannot be silently
classified as absent; bounded nofollow digest admission rejects them.

A successful outcome with no diagnostic failure requires every declared artifact.
On fresh zero-exit runs, missing required artifacts become a retained
`missing_required_artifacts` diagnostic failure before receipt publication. Fresh
return behavior and cached return behavior both reject that failure. Failed runs
may retain a partial artifact set and remain readable as failures; every declared
artifact which exists must be hash-accounted. No old receipt or output is reset.

The command/source checks, bounded JSON/progress reads and streaming file hashes
from reference-record admission remain in force. All 194 solver/compiler command
assignments compared unchanged against a before-edit baseline. Family control
fixtures now create all declared output forms, including geometry, coefficient
catalogue and exact-prediction outputs. Their real controlled compiler libraries
and synthetic numerical probes do not qualify real CFD factor ABI or accuracy.

Remaining lifecycle scope includes root namespace admission before allocation,
complete writer inventory, transitive cached toolchain/config provenance, aggregate
hash/workflow quotas, coherent interruption recovery, terminal owner eligibility,
archive-backed retirement and guarded pruning. This inventory validates artifacts
of a selected run; it is not cryptographic authentication of a receipt or complete
namespace policing of all other files under its shared directory.

No canonical adoption, commit, package installation, release, archive transfer or
pruning is part of this change. Evidence is retained under
`data/experiments/lifecycle-validation/20261007-reference-artifact-inventory`.
