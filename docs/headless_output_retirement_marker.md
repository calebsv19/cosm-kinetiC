# Headless output retirement marker admission

The detached-worker audit found that output retirement trusted a completed marker
without a single-link requirement or a pre-move marker witness recheck. Native
probes demonstrated prior acceptance of a hardlinked marker, five noncanonical
representations and three marker changes during retained-slot allocation.

Completed markers now must exactly equal the producer's existing v1 bytes for
that root device/inode. No unbounded scanf numeric conversion is used. The reader
requires a no-follow, regular, single-linked, exact-size file, checks exact pread
with EINTR/short-read handling and EOF, and verifies descriptor/named identity and
nanosecond mtime/ctime before checked close. The same marker identity is read and
verified again after retained-slot allocation and before moving the original root.
A running-state change, same-byte rewrite or marker replacement holds retirement.
No original is deleted; a failed late check can retain an empty allocated capsule.

The existing canonical writer and completed-marker format are unchanged. Earlier
manually written permissive representations are held rather than adopted. Markers
are trusted local receipts, not cryptographic proof of who wrote them. There is
still a check-to-rename race; this does not prove whole-worker ownership or freeze
all output contents. Retention is local preservation, not independently archived
retirement or permission to prune it. Full worker supervision, authenticated
process identity, log/workflow quotas, durable recovery and canonical adoption
remain open.

Ten native output methods pass, including three new methods / nine rejection
cases. The actual 28-method headless output/sidecar target and supported detached
job-runner bundle smoke pass. The ten output tests overlap the 28-method target.
Prior-source race counterproof has three expected failing cases and zero harness
errors; current checks preserve the original sentinel and do not create a moved
predecessor. The prior source SHA is
1bc621e15ca2d90cfad257a7815de1d3d2114692a4d03351fe1ddb5d3bf2832e.

Reuse: the existing app-local retained-output adapter and producer-owned marker
format remain in place; generic crypto/process ownership is not reimplemented.
No shared API/version, solver, installed product, release or canonical change.
Evidence: data/experiments/lifecycle-validation/20261007-output-retirement-marker.
This new packet is outside the independently archived backup batches.
