# Headless output completion marker ownership

The prior completion writer unconditionally replaced the running receipt. Native
probes showed six accepted changes after prepare: hardlink, removal, symlink,
same-content replacement, same-byte rewrite and changed content. Completion then
claimed a root whose original ownership receipt was no longer current.

The source output handle now retains the stat witness of its original running
marker. Preparation requires strict exact-byte running-marker readback after
creation/sync. Completion requires the original single-linked marker bytes and
full identity (mode, link count, size, nanosecond mtime/ctime, device/inode) before
allocating a candidate and again immediately before promotion. A changed original
is held; it is not replaced, removed or repaired. The candidate is created
exclusively/no-follow/close-on-exec, synced and checked against its named witness.
After rename, strict completed receipt readback and inode identity must match the
writer's candidate, and the directory must sync. Legitimate rename ctime changes
are not mistaken for external drift. Output-root identity is also rechecked.

A stage-time fault changes the running marker after the first completion check.
The second check refuses promotion and retains the pending completed receipt and
the changed original. No automatic recovery or pruning is introduced. Failures
after successful rename/sync remain uncertain rather than claiming an atomic
multi-artifact terminal transaction. A completion handle is not an idempotent
repeat-completion API; callers complete once while the original running witness
is current.

Twelve native output methods (two new methods / seven mutation cases) pass within
the actual 30-method output/sidecar target. The supported detached job-runner
bundle smoke passes. Existing retention-marker, predecessor-byte, sidecar and
path-boundary behavior remains covered. The original six fault cases were
accepted by prior source; its retained copy accompanies the evidence.

The app source handle layout adds a stat witness; all source callers are rebuilt.
The v1 on-disk producer bytes remain unchanged. No shared API/version or public
package ABI was adopted, installed, released or committed. This reuses the
existing strict marker reader and retained output adapter. It is local receipt
consistency, not cryptographic origin authentication or kernel-authenticated
worker identity. Check-to-rename and hostile ancestor races, worker-lifetime
supervision, log/workflow limits, terminal artifact coherence, independent archive
retirement, recovery and canonical adoption remain open.

Evidence: data/experiments/lifecycle-validation/20261007-output-completion-marker.
The newer packet is outside the independently archived backup batches.
