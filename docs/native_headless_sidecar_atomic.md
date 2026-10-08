# Atomic native summary/progress publication

Main Edit now writes each sidecar update into an exclusive same-directory
`.headless-sidecar-<pid>-<sequence>.pending` file. At most 64 name collisions are
tried without modifying existing files. Stage and directory are flushed before
writing. Publication requires matching stream/stage descriptors, no stream error,
successful flush/close, unchanged admitted predecessor and parent identities,
and a matching regular singly-linked staging entry. Only then does atomic rename
publish the generation. The client rebinds its descriptor to that generation,
flushes the directory and rechecks identity. Descriptors use close-on-exec.

A partial or failed write no longer truncates the last published file. A failed
stage remains for diagnosis, and the client becomes held rather than retrying or
marking the output owner complete. Forced death leaves the last published file
and pending stage. No automatic restart, stage removal, adoption or pruning occurs.
Before the first successful publication, the exclusive empty reservation records
an allocated intent; there is no previous readable generation to preserve yet.
Successful mutable updates replace prior generations without retaining a full
progress history. Complete run predecessors remain covered by output retention.

These are cooperative POSIX filesystem controls. They do not prove protection
against every uncooperative race/in-place edit, multi-file atomicity, complete
JSON/schema/numerical validity or hardware power-loss behavior. Summary and
progress publish independently. A directory flush failure after rename is held
and may leave the newly published generation present. Startup reconciliation of
running owners and pending stages remains open. Native detached runner metadata
still needs its own atomic publication and lifetime ownership controls.

Validation: 48 targeted checks passed, including forced death during partial
staging, concurrent readers across 80 JSON generations, foreign destination and
staged-link replacement, close failure and a real RLIMIT_FSIZE I/O error. The
admission/clean/retention suites and actual CLI checks also passed. All six final
CLI/Water/scene-project/runner fixtures passed. The Make native proof now includes
six atomic checks (25 native checks total). Final source builds reported no new
warnings. The initial direct-API harness alias path was corrected to its admitted
canonical spelling; path admission was not relaxed.

Evidence: `data/experiments/lifecycle-validation/20261007-native-sidecar-atomic-publication`.
No canonical adoption, release, original evidence deletion or independent archive
update occurred. This packet is outside the prepared backup.
