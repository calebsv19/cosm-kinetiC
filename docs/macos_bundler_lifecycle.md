# macOS dependency bundler lifecycle

The public source helper tools/packaging/macos/bundle-dylibs.sh now delegates to a
Python lifecycle owner. The owner requires an exact existing package reservation
for one app under checkout build/ or dist/, an admitted Contents/MacOS binary and
its sibling Contents/Frameworks. Linked payloads, malformed reservations, outside
paths and sealed evidence parents refuse before package mutation. Existing
package_outputs reservation validation, desktop_replace bounded inventories,
cfd_evidence path admission, build_owner descriptor verification and
agent_session.owned_command supervision are reused.

Every admitted invocation creates a UUID attempt under
`data/experiments/package-bundler-attempts/`.
A shared global cleanup lock and exclusive per-app bundler lock cover snapshot,
mutation and readback. Those descriptors, the attempt owner and verified inherited
build descriptors pass through retained command supervision. This serializes
cooperating bundlers and blocks cooperating global cleanup; it is not a sandbox
against unrelated filesystem writers. The existing outer package owner remains
responsible for the complete package transaction.

Before mutation, the owner retains the selected binary, complete Frameworks
snapshot, reservation, three selected controls and immutable request. Snapshot
content/mode inventories are compared with admitted inputs. Each inventory pass
admits at most 4,096 entries, 512 MiB total and 128 MiB per file using existing
cooperative sampled inventory limits. The whole engine uses the existing retained
anchor supervisor with 900-second wall and sampled 64 MiB combined stdout/stderr
limits. API bounds refuse invalid/nonfinite limits; CLI uses fixed defaults.

The internal shell engine receives a fresh private work directory; it neither
allocates predictable PID storage nor recursively removes output. Its queue,
seen list and otool diagnostics remain. At most 256 queued binary inspections
are allowed. otool now runs as a checked command before parsing. install_name_tool
ID/change failures and unresolved selected @rpath dependencies fail instead of
being hidden. Optional Vulkan/MoltenVK ID changes are also checked. Selected
control/reservation bytes are rechecked and bounded output inventory is admitted
before a passed terminal receipt can be written. Nonzero exits, timeout/log-cap
and local failures retain a failed receipt; uncertain teardown retains held state.
A process killed before terminal publication leaves its running request for
reconciliation, not a proved terminal artifact.

A failed engine can have partially changed its package staging output. The saved
predecessor is retained; automatic rollback is intentionally not claimed. The
complete package must remain held after failure. This does not qualify coherent
package recovery, basename collision resolution, all loader-reference forms,
whitespace in dependency names, optional dependency transitive closure, full
external tool/library provenance, aggregate disk/RSS limits, hostile path-swap
confinement, power-loss recovery, complete descendant termination, pruning,
canonical adoption or installed/released product state. In particular otool work
files and dependency copies are not covered by the stdout/stderr byte cap; output
inventory is an admission/readback bound, not a filesystem quota.

## Verification

Twenty-four distinct actual lifecycle/engine fixture methods pass, including real
shell-wrapper CLI invocation, functional copy/rewrite, repeated attempt byte
preservation, checked otool/ID/change/optional failure, unresolved dependency,
partial-mutation snapshot retention, links, missing/malformed reservations,
protected paths, sealed parents, invalid limits, timeout/log cap, competing cleanup
and same-app ownership, and source-control drift. Fixtures substitute only fixed
Mach-O tool paths with synthetic executables; no actual Mach-O transformation,
package build, signing or application launch occurs.

Eight package-output, five reservation-schema and eight retained-command
regressions also pass: 45 distinct methods total. The real control-only Make target
repeats the 24 lifecycle methods with compiler/pkg-config unavailable and creates
no requested build profile. Shell syntax and git diff whitespace checks pass.
Persistent copies of three selected functional/failure fixtures retain output,
snapshots and logs; absolute request paths still name their original disposable
fixture roots, so relocated request replay is not claimed. A safe synthetic old
control demonstrates that the original script returned success when otool exited
17; the repaired script rejects that case and retains diagnostics. All processes
started for these checks reached observed terminal completion.

Source changes are limited to Main Edit. Canonical still uses the legacy bundler
and broad cleanup recipes. The pending archive upload was not retried during this
slice; the new packet lies outside that frozen backup. The full TL01-TL13 goal,
other packaging entrypoint findings and canonical adoption remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-macos-bundler-lifecycle.
