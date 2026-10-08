# Compiler input boundary observation

Main Edit dependency-producing object compilation now first runs supervised
compiler dependency discovery. -MMD uses -MM discovery and -MD uses -M discovery,
matching each command's user/system-header policy. The same compiler flags and
cooperatively inherited ownership descriptors are used. A failed discovery never
starts actual compilation. Dependency sidecar paths, including attached -MF/-MT/
-MQ overrides, remain outside the caller-selected contract. Link commands without
dependency generation retain their existing supervision/publication behavior.

A bounded complete source/header content and resolved-file identity snapshot is
captured after discovery and before actual compilation. After successful actual
compilation, its emitted dependency closure must have identical content digests,
resolved paths and full identity witnesses. This detects changed bytes, same-byte
rewrites, changed resolutions and changed dependency sets. Bounded nofollow reads
admit exact original-size regular single-linked dependency metadata. The two
snapshot observations share the existing complete-pass entry/hash-byte budget.

A mismatched or unreadable postcompile input set holds publication. Existing
object/dependency bytes and their ownership receipts remain unchanged. A fresh
attempt publishes neither output nor ownership receipt. The candidate object,
dependency files and an exclusive `input-hold.json` classified as retained compiler
output remain in the attempt directory. A matching result records
`compiler_boundary_observation` scope. Earlier structurally valid postcompiler
snapshots are recognized but force one refresh, rather than being relabeled as
stronger evidence. Unknown/contradictory snapshots continue to hold.

Actual Clang counterprobes show three prior bad-publication cases: changed header
bytes after compilation, a same-byte header rewrite and a mutation on a fresh
build. The fourth before-code failure is the new scope assertion against the
older postcompiler scope, not an additional security acceptance case. Seven final
boundary methods include predecessor/candidate preservation, disappeared inputs,
preparation failure and -MD system-header compatibility. A sparse-file fixture
resource warning was also corrected by closing its file explicitly.

The authoritative gate passes 64 distinct methods: seven boundary, thirteen
build dependency/input, twelve atomic output, four environment, one build identity,
five configuration admission, three fixture ownership, one contract writer and
eighteen cleanup methods. The real supported headless source build passes in a
fresh isolated compiler-input-boundary-20261007 profile. Its matching repeat makes
no changes to 232 objects and the headless binary. All 232 generated dependency
receipts have boundary scope (largest closure: 105 inputs). Supported Water smoke
and scene-project cache-output proofs pass; their complete small retained fixture
roots are copied into this evidence packet. Read-only clean preview admits 467
owned files. No user cleanup or refresh of an older profile was applied.

These are before/after observations, not immutable compiler input snapshots or
proof that a hostile source writer cannot race between observations and promotion.
System headers omitted by -MMD, linked libraries, compiler configuration/resources,
actual wrapper-selected tools, escaped descendants, power-loss coherence and
object/dependency multi-file recovery remain open. Input-hold evidence is preserved
locally; independent archive coverage and retirement are separate. Supported smoke
proofs do not qualify CFD physical accuracy or installed/public packages.

No canonical adoption, commit, installation, release, deletion of user evidence
or independent backup coverage changed. Evidence is sealed in
20261007-compiler-input-boundary, outside the frozen seventy-packet backup batch.
The complete TL01–TL13 hardening goal remains active and incomplete.
