# Source and header content reuse identity

Main Edit compiler publication now records a bounded content snapshot of the
source/header closure declared by a generated dependency file. The compact
snapshot binds canonical original/resolved input paths and SHA-256 bytes, records
its input count and explicitly states `postcompiler_observation` scope. The object
ownership receipt also binds the exact generated dependency SHA-256. The existing
ownership schema remains readable; neither field authenticates compiler execution.
The restricted dependency grammar is now shared inside `build_outputs.py`, an
existing copied helper, without adding a module dependency to standalone fixtures.

Build graph admission compares the saved snapshot with current source/header
contents. A changed snapshot, missing legacy snapshot or unmatched object/dep
binding forces the affected owning object to rebuild through existing atomic
publication. This is an explicit prerequisite, not a timestamp comparison or a
reason to run clean. A valid legacy owned pair refreshes once and then becomes a
matching no-op. Contradictory snapshot fields hold instead of being silently reset.
Exact output ownership remains required; unknown/changed output bytes cannot be
adopted by the refresh. Header/source changes with restored timestamps are detected.

Input preflight permits at most 10,000 source/header paths per closure, 64 MiB per
file and 256 MiB closure bytes. The complete selected graph pass charges repeated
closure entries against 100,000 entries and both hash reads against 1 GiB. A late
closure exceeding the remaining budget holds before its first input hash; earlier
read-only observations do not authorize a stamp write. Streaming reads stop at
the admitted original length plus an overflow observation. Descriptor identity
must match the stat identity charged during preflight. A second full closure read
checks identities/resolution after the first hash pass. Legitimate source/tool
hardlinks and resolved include aliases remain separate from uniquely owned
compilation output policy. Nonregular/multiply linked staged dependencies hold.

Before-code actual Make counterproof shows a changed header with original mtime
restored leaves the old object unchanged. Native repaired checks prove that case
rebuilds and then returns to a no-op, that preserved-time source changes rebuild
only the affected object, and that legacy owned pairs refresh once. Malformed
snapshots hold without changing output; per-file and cross-closure limits hold
before the relevant hashes. The earlier admission/executable-syntax and escaped-
space header tests remain passing.

Across the initial, consumer, budget and final source gates, 76 distinct methods
pass. The final exact-source gate includes thirteen dependency/input methods,
six read-admission methods, twelve atomic-output methods and one build identity
method. Other gates cover environment/configuration admission, output isolation,
contract writers/fixtures, cleanup and inventory budgets. Read-only inspection of
the retained native profile admits 330 dependencies and reports all 330 objects
need a legacy refresh. No refresh/build or user cleanup was applied to that profile.

This advances content-based reuse without proving compiler input immutability:
the snapshot is observed after compilation and does not establish which bytes
were consumed throughout the compiler lifetime. System headers omitted by -MMD,
linked library/SDK/config contents, full compiler toolchain provenance, coherent
object/dep publication under interruption and hostile post-admission races remain
open. Complete pass limits are sampled byte/entry admission, not hard I/O deadlines.
Status still does not claim full profile verification. CFD accuracy is separate.

No canonical adoption, commit, installation, release, deletion or independent
backup coverage changed. Evidence is sealed in 20261007-build-input-content, outside
the frozen seventy-packet backup batch. The full TL01–TL13 goal remains active.
