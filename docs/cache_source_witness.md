# Cache publication source witness

A native fault changed source bytes after validation but before their first
copy. A finite density change, opaque pack change and semantically equivalent
bundle whitespace change all passed the preceding complete-copy comparison:
all staged copies equaled the new source. Publication accepted all three.

The publisher now captures a bounded selected-file/directory inventory before
source semantic validation. It verifies the inventory immediately after that
validation and again after the four staged comparisons, before predecessor
checks/displacement. Source validation moved inside this witness lifetime; it
still precedes attempt creation. Failure releases the in-memory inventory.
Failures after attempt creation retain staged output, every predecessor and the
existing pending hold. The normal source format and publication layout are unchanged.

Selected names use the existing VF3D/physics copy predicates, including optional
packs and water sidecars. At most 10,000 source entries are enumerated; a fixed
10,000-entry calloc holds host NAME_MAX-sized names and stat witnesses. Selected
files must be regular, single-linked, no-follow admitted entries. Rechecks use
device/inode, mode, link count, size, nanosecond mtime and ctime. Source directory
identity/metadata is checked before and after the complete file recheck so entry
additions/removals are held, including conservative holds for unrelated directory
changes. Access-time changes alone do not hold ordinary reads.

Two new native methods cover three changed-byte source cases, a same-byte rewrite
and a newly added selected water sidecar. All copies remain equal to current
source in these probes; all seven predecessors remain intact and the attempt is
held. This distinguishes the new witness from copy comparison. Successful actual
producer publication, existing copy/payload/recovery and compiler checks also run.

Reuse: this uses the existing app copy predicates, bounded source reader and
named-entry witness; it introduces no shared semantic/API/version change.
The durable streaming SHA-256 requirement remains deferred for the same reasons
recorded in cache_copy_integrity.md; witnesses are not cryptographic hashes.

This is an in-process consistency check for ordinary local drift, not durable
source authentication or an immutable whole-plan snapshot. The witnesses are not
serialized as an authenticated source manifest. Staged entries are not pinned
across every subsequent publication step; source or destinations can change after
final checks. Full hostile ancestor/entry races, immutable generation visibility,
digest-bound forward recovery, aggregate/hard quotas, retirement and canonical
adoption remain open. Status still cannot detect an arbitrary finite active-field
change without an authenticated digest. No solver or CFD numerical claims follow.

Evidence: data/experiments/lifecycle-validation/20261007-cache-source-witness.
