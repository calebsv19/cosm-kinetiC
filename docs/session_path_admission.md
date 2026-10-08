# Trusted-local session storage admission

Main Edit admits local session roots before allocating scenes/runs or locks.
Supplied symlink components are rejected, rather than resolved to a different
storage destination. The exact macOS `/tmp` and `/var` aliases are normalized
only when their observed targets are `/private/tmp` and `/private/var`.
Traversal components and paths deeper than 256 components are held.

A session root cannot be the source checkout, its ancestor, a source subtree,
protected `.git`/`.aws`/`.ssh` storage, an app bundle, another Git checkout or a
broad system/user directory. Within the owning source checkout it must be a
strict descendant of `build`, `tmp`, `data/experiments` or `data/runtime`. External
explicit session directories remain supported. The source default stays
`data/runtime/agent_sessions`. Packaged callers must select an admitted user
runtime root; writing runtime data into application resources is held.

Both fixed storage directories and the service lock are admitted before their
creation. The service rechecks storage when taking its lock. Run lookup rejects
linked run directories. JSON reads/writes and service/owner lock opens reject
linked components; special-file locks and JSON inputs fail without blocking.
Existing regular lock files remain supported. Asset names must select descendants
of their scene and cannot be absolute, traverse upward or follow links.

JSON reads use no-follow descriptors and a 16 MiB byte bound, and recheck file/path
identity around the read. An explicit lexical depth check runs before decoding
(128 nested objects/arrays), respecting quoted/escaped strings. Duplicate fields,
non-finite constants and excessive nesting are held. Atomic writes recheck the
selected path before replacement and retain existing file-type admission.

These controls improve the trusted-local workflow. They do not sandbox arbitrary
public uploads or replace filesystem permissions. Admission/rechecks are
cooperative; an uncooperative process can race path components between checks.
The native worker, asset hashing/copying, subprocess logs, complete root ownership,
compile/validation scratch retention and forced-death recovery still need their
own lifecycle audit. Source/package/installed qualification remain separate.

The path test suite exercises protected/linked roots, bad child storage before
allocation, live storage replacement, source/foreign checkout admission, special
locks/JSON, escaping assets, linked runs, ordinary JSON/locks, system aliases and
metadata bounds. Control-only tests require no compiler or pkg-config. Local
session and diagnostic-retention regression use temporary session roots and the
existing worker; no worker replacement or installed-product change occurs.

Final validation: 14 focused/control-only path checks and 19 local-session /
sample-retention regression checks passed (33 total). The first deep-nesting
fixture exposed runtime-dependent decoder behavior; an explicit pre-decode
bound repaired that gap and the final tests passed. Evidence is retained at
`data/experiments/lifecycle-validation/20261007-session-path-admission`.


## Session storage identity witnesses

Each client now pins four non-inherited descriptors for its root, scenes, runs
and service lock. Public operations check those identities before and after
execution; locks compare the acquired file with the pinned lock. Authoring and
validation also check storage before completing retained attempts. Validation
uses the existing service lock. Ordinary directory or regular lock replacement
is held, including replacement during an operation. The original incomplete
receipt remains held rather than writing a terminal receipt into a replacement
root. Constructor admission creates the empty service lock when absent.

Client close releases the four descriptors and prevents reuse; it does not stop
a native worker. These are cooperative checks, not descriptor-relative writes
or protection against transient uncooperative races. Individual scene/run
ownership, native worker storage and forced-death reconciliation remain open.

Twenty path checks, eighteen retained-attempt checks and nineteen operational
regressions passed (57 total). Evidence:
`data/experiments/lifecycle-validation/20261007-session-storage-witnesses`.
This packet remains outside the prepared independent backup and canonical.
